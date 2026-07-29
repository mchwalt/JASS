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
//   * kReflectionPower is the EMPIRICAL per-ear pink-weighted power of the whole wet path
//     (kernels + damping + cross-tap incoherence) relative to dry. The constant-power normalisation
//     dry=1/sqrt(1+r^2*P), wet=r*dry then holds the output level to within ±0.2 dB of dry at any
//     knob position — the five output modes were just level-matched, this must not break that.
//   * centre transparency: octave bands at full deflection stay within −2.4…+1.0 dB of dry (a
//     smooth room tilt, no comb colour; at the 0.67 default it is −1.4…+0.6 dB) — the Story-10.3
//     centre win is kept.
//   * the reflection send is damped by a one-pole lowpass (~5.5 kHz, wall/air absorption): keeps
//     the direct sound crisp, the room dark — and the HF comb inaudible.
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
        // Delays are defined in ms (primes in samples at the 44.1 kHz reference) and scale with the
        // host rate; the guard keeps the longest delay + kernel window inside the static ring.
        for (int t = 0; t < kNumTaps; ++t)
            delaySmp[t] = std::min (kRingSize - KemarHrir::kTaps - 1,
                                    (int) std::lround (kTapDelayMs[t] * 0.001 * sampleRate));
        dampAlpha = 1.0f - std::exp ((float) (-2.0 * 3.14159265358979 * kDampHz / sampleRate));

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

    // In-place stereo. room ∈ [0,1]: 0 = dry (exact unity passthrough), 1 = full reflections.
    // Gains ramp from their previous block's values (anti-zipper).
    void process (float* l, float* r, int n, float room) noexcept
    {
        room = std::clamp (room, 0.0f, 1.0f);
        const float tgtDry = 1.0f / std::sqrt (1.0f + room * room * kReflectionPower);
        const float tgtWet = room * tgtDry;
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
    // Range extended 2026-07-29 (user ear test): the original max (P=0.45, wet −3.5 dB) turned out
    // to be the PREFERRED amount, not the ceiling — it now sits at knob 0.67 (the default) and full
    // deflection delivers wet == dry power (P=1.0), i.e. ~3.5 dB more room on top.
    static constexpr float kTapGain[kNumTaps]     = { 0.785f, 0.707f, 0.628f, 0.565f, 0.503f, 0.456f };
    static constexpr float kReflectionPower       = 1.0f;     // measured per-ear pink power of the wet path
    static constexpr float kDampHz                = 5500.0f;  // one-pole LP on the send (absorption)

    // 24.06 ms + 128-tap window fits up to ~185 kHz host rate; prepare() clamps beyond that.
    static constexpr int kRingSize = 8192;
    static constexpr int kRingMask = kRingSize - 1;
    static_assert ((kRingSize & kRingMask) == 0, "ring size must be a power of two");

    float ring[kRingSize] = {};
    const float* kernL[kNumTaps] = {};
    const float* kernR[kNumTaps] = {};
    int   delaySmp[kNumTaps] = {};
    int   writePos  = 0;
    float lpState   = 0.0f;
    float dampAlpha = 0.5f;
    float curDry    = 1.0f;
    float curWet    = 0.0f;
};
