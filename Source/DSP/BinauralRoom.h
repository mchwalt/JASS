#pragma once
#include <cmath>
#include <algorithm>
#include "KemarHrir.h"   // embedded MIT KEMAR HRIRs (see attribution in that header)

// ── Binaural early-reflection stage ("Kunstkopf externalization", Epic 10 / Story 10.4) ──────
// Dry binaural stays IN-HEAD on headphones almost regardless of HRTF quality — externalization is
// driven mainly by early reflections. This stage adds a handful of them: the bus signal, delayed a
// few non-harmonic times and rendered through LATERAL measured HRIRs (a reflection arriving from
// the side is what carries the out-of-head cue). It runs ONCE on the stereo bus, not per voice —
// reflections do not need per-source accuracy to externalize, so the shared pattern gets nearly
// all of the perceptual benefit at a fixed, tiny cost (the per-generator direct sound keeps its
// individual azimuth).
//
// Design constants were MEASURED, not ear-guessed (tools history: two of this epic's constants were
// wrong until computed) — scratch harness re-runnable, results 2026-07-29:
//   * tap delays = primes in samples @44.1k (8.3–24.1 ms) → mutually non-harmonic by construction
//     (closest pair ratio is 0.106 away from an integer; nothing rings on a pitch). All ≥ 8 ms —
//     below ~5 ms reflections fuse into comb colouration instead of room.
//   * kWetPowerLo/Hi are the EMPIRICAL per-ear pink-weighted powers of the whole wet path
//     (kernels + damping + cross-tap incoherence) relative to dry, measured at the two damping
//     endpoints; the constant-power normalisation dry=1/sqrt(1+w^2*P(room)), wet=w*dry then holds
//     the output level to within ±0.2 dB of dry at any knob position — the five output modes were
//     just level-matched, this must not break that.
//   * centre transparency: octave bands at full deflection stay within −2.6…+1.4 dB of dry (a
//     smooth room tilt, no comb colour) — the Story-10.3 centre win is kept.
//   * the reflection send is damped by a one-pole lowpass (wall/air absorption): keeps the direct
//     sound crisp, the room dark — and the HF comb inaudible. The cutoff rides the knob (see
//     process()).
//
// RT-safe by construction: fully static-sized (ring + fixed tap table), kernels are POINTERS into
// the constexpr KEMAR table (no copies), no allocation anywhere. Gains ramp linearly across each
// block (anti-zipper). While the knob is at 0 the taps are skipped but the ring keeps filling, so
// turning the room up later is seamless (no stale-buffer ghost).
class BinauralRoom
{
public:
    void prepare (double sampleRate)
    {
        sampleRateHz = (float) sampleRate;
        // Delays are defined in ms (primes in samples at the 44.1 kHz reference) and scale with the
        // host rate; the guard keeps the longest delay + kernel window inside the static ring.
        for (int t = 0; t < kNumTaps; ++t)
            delaySmp[t] = std::min (kRingSize - KemarHrir::kTaps - 1,
                                    (int) std::lround (kTapDelayMs[t] * 0.001 * sampleRate));
        dampAlpha = 1.0f - std::exp (-kTwoPi * kDampLoHz / sampleRateHz);

        // Lateral kernels, fixed per tap: negative azimuth = source on the LEFT → the symmetric
        // head swaps the ears (same convention as HrtfPanner).
        for (int t = 0; t < kNumTaps; ++t)
        {
            const int idx = kTapAzimuthDeg[t] >= 0 ?  kTapAzimuthDeg[t] / KemarHrir::kAzimuthStepDeg
                                                   : -kTapAzimuthDeg[t] / KemarHrir::kAzimuthStepDeg;
            kernL[t] = (kTapAzimuthDeg[t] >= 0) ? KemarHrir::kHrirLeft[idx]  : KemarHrir::kHrirRight[idx];
            kernR[t] = (kTapAzimuthDeg[t] >= 0) ? KemarHrir::kHrirRight[idx] : KemarHrir::kHrirLeft[idx];
        }
        reset();
    }

    void reset()
    {
        std::fill (std::begin (ring), std::end (ring), 0.0f);
        writePos = 0;
        lpState  = 0.0f;
        curDry   = 1.0f;
        curWet   = 0.0f;
    }

    // In-place stereo. room ∈ [0,1] is a MACRO, not a linear wet gain (psychoacoustics: the
    // direct-to-reverberant JND is ~5-6 dB [Zahorik 2002] and spatial impression saturates within
    // ~10 dB of the direct sound [Barron 1971 / precedence] — a linear gain knob is dead above ~0.2).
    // The knob gangs two parameters so every step changes something audible:
    //   wet level : -kWetRangeDb .. 0 dB relative to dry, exponential (hard off at 0)
    //   damping   : send lowpass 3.5 kHz .. 7 kHz (small dark room -> bigger brighter one)
    // The UI exposes it as 5 detents (step 0.25 = 6 dB ~ 1 JND apart — the honest resolution).
    // Gains ramp from their previous block's values (anti-zipper).
    void process (float* l, float* r, int n, float room) noexcept
    {
        room = std::clamp (room, 0.0f, 1.0f);
        const float w = room <= 0.0f ? 0.0f
                                     : std::pow (10.0f, -(kWetRangeDb * (1.0f - room)) * (1.0f / 20.0f));
        dampAlpha = 1.0f - std::exp (-kTwoPi * kDampLoHz * std::pow (2.0f, room) / sampleRateHz);
        const float P = kWetPowerLo + (kWetPowerHi - kWetPowerLo) * room;   // wet power tracks damping
        const float tgtDry = 1.0f / std::sqrt (1.0f + w * w * P);
        const float tgtWet = w * tgtDry;
        const float stepDry = (tgtDry - curDry) / (float) n;
        const float stepWet = (tgtWet - curWet) / (float) n;

        // Inactive and staying inactive: keep the ring warm (cheap) but skip the convolutions.
        if (curWet < 1.0e-4f && tgtWet < 1.0e-4f)
        {
            for (int i = 0; i < n; ++i)
            {
                lpState += dampAlpha * (0.5f * (l[i] + r[i]) - lpState);
                ring[writePos] = lpState;
                writePos = (writePos + 1) & kRingMask;
            }
            curDry = tgtDry;
            curWet = tgtWet;
            return;
        }

        for (int i = 0; i < n; ++i)
        {
            lpState += dampAlpha * (0.5f * (l[i] + r[i]) - lpState);   // damped mono send
            ring[writePos] = lpState;

            float accL = 0.0f, accR = 0.0f;
            for (int t = 0; t < kNumTaps; ++t)
            {
                const float* kL = kernL[t];
                const float* kR = kernR[t];
                int idx = (writePos - delaySmp[t]) & kRingMask;        // newest sample of the delayed window
                float aL = 0.0f, aR = 0.0f;
                for (int k = 0; k < KemarHrir::kTaps; ++k)
                {
                    const float h = ring[idx];
                    aL += kL[k] * h;
                    aR += kR[k] * h;
                    idx = (idx - 1) & kRingMask;
                }
                accL += kTapGain[t] * aL;
                accR += kTapGain[t] * aR;
            }

            writePos = (writePos + 1) & kRingMask;

            curDry += stepDry;
            curWet += stepWet;
            l[i] = curDry * l[i] + curWet * accL;
            r[i] = curDry * r[i] + curWet * accR;
        }
        curDry = tgtDry;   // land exactly on the targets (no drift across blocks)
        curWet = tgtWet;
    }

private:
    static constexpr int kNumTaps = 6;
    // 367/499/641/773/919/1061 samples @44.1k — primes, hence mutually non-harmonic.
    static constexpr float kTapDelayMs[kNumTaps]  = { 8.32f, 11.32f, 14.54f, 17.53f, 20.84f, 24.06f };
    static constexpr int   kTapAzimuthDeg[kNumTaps] = { 55, -40, 70, -60, 30, -50 };   // lateral, alternating sides
    // Gains tuned so full deflection delivers wet == dry power (user ear test 2026-07-29: the
    // original -3.5 dB ceiling was the PREFERRED amount, which now sits ~1 JND below detent 3).
    static constexpr float kTapGain[kNumTaps]     = { 0.785f, 0.707f, 0.628f, 0.565f, 0.503f, 0.456f };
    static constexpr float kWetRangeDb            = 24.0f;    // knob macro: wet -24 dB .. 0 dB rel. dry
    static constexpr float kDampLoHz              = 3500.0f;  // send lowpass at knob 0+ (dark)
    // kDampHiHz == kDampLoHz * 2^1 = 7 kHz at knob 1 (exponential, one octave).
    // Measured per-ear pink wet power (unit wet gain) at the two damping endpoints — the
    // normalisation lerps between them (tools/measure_binaural_room.py, 2026-07-29):
    static constexpr float kWetPowerLo            = 0.814f;   // at 3.5 kHz damping
    static constexpr float kWetPowerHi            = 1.107f;   // at 7 kHz damping
    static constexpr float kTwoPi                 = 6.2831853f;

    // 24.06 ms + 128-tap window fits up to ~185 kHz host rate; prepare() clamps beyond that.
    static constexpr int kRingSize = 8192;
    static constexpr int kRingMask = kRingSize - 1;
    static_assert ((kRingSize & kRingMask) == 0, "ring size must be a power of two");

    float ring[kRingSize] = {};
    const float* kernL[kNumTaps] = {};
    const float* kernR[kNumTaps] = {};
    int   delaySmp[kNumTaps] = {};
    int   writePos     = 0;
    float lpState      = 0.0f;
    float dampAlpha    = 0.5f;
    float sampleRateHz = 44100.0f;
    float curDry       = 1.0f;
    float curWet       = 0.0f;
};
