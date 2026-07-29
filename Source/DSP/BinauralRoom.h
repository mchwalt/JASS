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
        dampAlpha = 1.0f - std::exp (-kTwoPi * kDetDampHz[0] / sampleRateHz);

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

    // In-place stereo. room ∈ [0,1] is a 5-DETENT MACRO (param step 0.25), not a linear wet gain:
    // the direct-to-reverberant JND is ~5-6 dB [Zahorik 2002], spatial impression saturates within
    // ~10 dB of the direct sound [Barron 1971 / precedence], and wide/fine spreads wasted travel
    // below the user's personal effect threshold (ear-calibrated 2026-07-29: -12 dB wet inaudible,
    // -6 dB slight, 0 dB perfect). Every detent sits INSIDE the audible window and gangs wet level
    // + send damping (each step = stronger AND brighter room); the ear-tested "perfect" setting
    // sits at the CENTRE detent (= the default), the upper half goes beyond it. Per-detent
    // measured normalisation constants — see the tables below. Gains ramp across the block.
    void process (float* l, float* r, int n, float room) noexcept
    {
        const int det = std::clamp ((int) std::lround (room * 4.0f), 0, kNumDetents - 1);
        const float w = kDetWet[det];
        dampAlpha = 1.0f - std::exp (-kTwoPi * kDetDampHz[det] / sampleRateHz);
        const float tgtDry = 1.0f / std::sqrt (1.0f + w * w * kDetWetPower[det]);
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
    // Gains tuned so detent 2 delivers wet == dry power (the user's ear-tested "perfect").
    static constexpr float kTapGain[kNumTaps]     = { 0.785f, 0.707f, 0.628f, 0.565f, 0.503f, 0.456f };
    // Per-detent design (knob step 0.25 → detent 0..4), ear-calibrated 2026-07-29. The ear-tested
    // optimum sits in the MIDDLE (detent 2, the default); the upper half goes beyond it — at the
    // stop the reflections carry twice the direct power (sounds distant, bass thins ~5 dB by
    // partial cancellation — measured, accepted as the extreme).
    //   wet     {off, −3, 0, +3, +6} dB rel. dry (as amplitude below)
    //   damping {—, 5, 7, 8.5, 10} kHz send lowpass (darker small room → brighter big one)
    //   power   = MEASURED per-ear pink wet power at that damping (tools/measure_binaural_room.py)
    static constexpr int   kNumDetents            = 5;
    static constexpr float kDetWet[kNumDetents]      = { 0.0f, 0.708f, 1.0f, 1.413f, 1.995f };
    static constexpr float kDetDampHz[kNumDetents]   = { 5000.0f, 5000.0f, 7000.0f, 8500.0f, 10000.0f };
    static constexpr float kDetWetPower[kNumDetents] = { 0.958f, 0.958f, 1.107f, 1.192f, 1.258f };
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
