#pragma once
#include <cmath>
#include <algorithm>
#include "KemarHrir.h"   // embedded MIT KEMAR HRIRs, post-processed (see attribution in that header)

// ── HRIR-convolution binaural panner ("real Kunstkopf", Epic 10 / Story 10.3) ────────────────
// Convolves ONE mono source with the measured MIT KEMAR head-related impulse response for its azimuth
// → genuine out-of-head 3-D on headphones (real ITD/ILD/spectral cues baked into the IR), unlike the
// parametric BinauralPanner. One instance per generator per voice.
//
// ALL tonal correction lives in the KERNELS, not here (tools/gen_kemar_hrir.py, applied offline, so the
// runtime cost is zero). That matters, because the raw MIT kernels cannot be used directly: their
// frontal response — the one playing at pan CENTRE, where no spatial effect is wanted at all — is a
// 21.6 dB bandpass with no bass, heard as "buzzy, loses its warmth". The generator equalises the
// frontal response out, synthesises a flat correctly-delayed low end (a 128-tap kernel is too short to
// carry real bass, but "flat with the right delay" is just a delayed impulse, which is compact), and
// level-matches every azimuth pair to Stereo-Pan. Pan centre therefore comes out transparent while the
// ITD and the per-frequency ILD stay intact.
//
// This class deliberately stays a PLAIN convolution. An earlier version split the signal and routed
// the low band around the kernels through a delay line, to spare the bass; that was wrong twice over —
// the kernel's phase below ~300 Hz is not a meaningful delay (it measured -94 samples at 100 Hz), so
// the bypass could not be aligned against it and the two paths partially cancelled into an 8.8 dB hole
// at the crossover; and with a gentle 1st-order split most of a played note's energy skipped the
// convolution entirely, which made the whole mode indistinguishable from plain stereo panning. Both
// problems disappear when the low end is built into the kernel instead, and the runtime gets simpler.
//
// Fully static-sized, so there is NO allocation anywhere (RT-safe by construction): the KEMAR table is
// a static constexpr (KemarHrir.h); each panner holds a 128-sample input ring and its two currently-
// selected per-ear kernels. The azimuth kernel is chosen ONCE PER BLOCK (setPanForBlock) — never per
// sample — which is the anti-zipper granularity for auto-panning; the per-sample process() is just two
// 128-tap dot products.
//
// v1 uses the 44.1 kHz IRs at the host rate directly (no resample). At 48/96 kHz the notches shift a
// few percent — inaudible for placement; a windowed-sinc resample in prepare() is a documented later
// refinement. pan → front-hemisphere azimuth (−90..+90°); the symmetric head mirrors the ears for the
// left side. At pan 0 both ears get the frontal IR (centered, continuous through 0).
class HrtfPanner
{
public:
    struct Out { float l, r; };

    void prepare (double /*sampleRate*/)   // no host-rate resample in v1 (fixed 128-tap 44.1k IRs)
    {
        reset();
        setPanForBlock (0.0f);
    }

    void reset()
    {
        std::fill (std::begin (history), std::end (history), 0.0f);
        writePos = 0;
    }

    // Choose/interpolate the per-ear HRIR kernel for pan p ∈ [-1,+1]. Call ONCE PER BLOCK (not per
    // sample). p ≥ 0 → source right (kHrirLeft/Right used directly); p < 0 → source left (ears swapped).
    void setPanForBlock (float p) noexcept
    {
        const float ap    = std::min (1.0f, std::fabs (p));
        const float fidx  = (ap * 90.0f) / (float) KemarHrir::kAzimuthStepDeg;   // 0 .. kNumAzimuths-1
        int   i0    = (int) fidx;
        if (i0 > KemarHrir::kNumAzimuths - 2) i0 = KemarHrir::kNumAzimuths - 2;
        const int   i1    = i0 + 1;
        const float frac  = std::clamp (fidx - (float) i0, 0.0f, 1.0f);

        // Ear assignment: for a source on the RIGHT the left ear = kHrirLeft, right ear = kHrirRight;
        // for the LEFT the head symmetry swaps them.
        const auto& earLsrc = (p >= 0.0f) ? KemarHrir::kHrirLeft  : KemarHrir::kHrirRight;
        const auto& earRsrc = (p >= 0.0f) ? KemarHrir::kHrirRight : KemarHrir::kHrirLeft;
        for (int k = 0; k < KemarHrir::kTaps; ++k)
        {
            coefL[k] = earLsrc[i0][k] + frac * (earLsrc[i1][k] - earLsrc[i0][k]);
            coefR[k] = earRsrc[i0][k] + frac * (earRsrc[i1][k] - earRsrc[i0][k]);
        }
    }

    // Convolve one input sample against the current L/R kernels. p is unused (kernel is pre-selected
    // per block via setPanForBlock) — kept for call-site symmetry with BinauralPanner.
    Out process (float x, float /*p*/ = 0.0f) noexcept
    {
        // Silence short-circuit. SynthVoice pans EVERY generator every sample, whether or not its
        // module is on — a disabled generator simply returns 0. Convolving that zero is exact but
        // pointless work, and here it is not cheap: one 128-tap render costs ~1.3 % of a core (SSE2,
        // measured 2026-08-10), so nine generators across eight voices spend ~94 % of a core
        // filtering nothing. That is what made a busy patch stumble in Kunstkopf while a single held
        // note stayed clean.
        //
        // Exact, not approximate: once kTaps consecutive zeros have been written the history holds
        // nothing but zeros, so the output is provably zero. While skipping we neither write nor
        // advance — an all-zero buffer has no meaningful write position — and the first non-zero
        // sample resumes normally into a buffer that is genuinely clean.
        if (x == 0.0f)
        {
            if (silentRun >= KemarHrir::kTaps)
                return { 0.0f, 0.0f };
            ++silentRun;
        }
        else
            silentRun = 0;

        history[writePos] = x;
        float accL = 0.0f, accR = 0.0f;
        int idx = writePos;
        for (int k = 0; k < KemarHrir::kTaps; ++k)   // k=0 = newest sample
        {
            const float h = history[idx];
            accL += coefL[k] * h;
            accR += coefR[k] * h;
            idx = (idx + (KemarHrir::kTaps - 1)) & (KemarHrir::kTaps - 1);   // (idx-1) mod 128, 128 = pow2
        }
        writePos = (writePos + 1) & (KemarHrir::kTaps - 1);
        return { accL, accR };
    }

private:
    static_assert ((KemarHrir::kTaps & (KemarHrir::kTaps - 1)) == 0, "kTaps must be a power of two for the ring mask");

    int   silentRun = KemarHrir::kTaps;   // consecutive zero inputs; starts "drained"
    float history[KemarHrir::kTaps] = {};
    float coefL[KemarHrir::kTaps]   = {};
    float coefR[KemarHrir::kTaps]   = {};
    int   writePos = 0;
};
