#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

// ── Parametric binaural ("Kunstkopf") panner (Epic 10, Story 10.3) ───────────────────────────
// Renders ONE mono source at pan p ∈ [-1,+1] to a 3-D image on ordinary stereo headphones, using a
// no-assets parametric HRTF model: ITD (interaural time difference — a short fractional delay on the
// FAR ear) + ILD (level difference — supplied as the equal-power gains gL/gR, reused from the pan
// path so there is no per-sample trig here) + a head-shadow one-pole low-pass on the far ear.
//
// One instance per generator per voice (Binaural is the per-generator spatial render before the mix).
//
// Center-continuity is STRUCTURAL: every extra term (delay, LP coef) is an odd split of p that → 0 as
// p → 0, and gL/gR are the equal-power gains, so at p=0 the output is exactly {gL·x, gR·x} — identical
// to the amplitude Stereo-Pan center and seamless across the mode boundary. With kMaxITDSeconds=0 and
// a 0 head-shadow depth the whole thing collapses to Stereo-Pan byte-for-byte (regression oracle).
//
// RT-safe: the delay line is sized once in prepare(); process() is bounded arithmetic + two
// interpolated reads (no alloc/lock/trig). Denormals are handled by the caller's ScopedNoDenormals.
class BinauralPanner
{
public:
    struct Out { float l, r; };

    void prepare (double sampleRate)
    {
        maxITDSamples     = (float) std::ceil (kMaxITDSeconds * sampleRate);
        delayLine.assign ((size_t) ((int) maxITDSamples + 2), 0.0f);
        writePos          = 0;
        lpStateL = lpStateR = slewL = slewR = 0.0f;
        headShadowCoefMax = (float) std::exp (-2.0 * 3.14159265358979 * kHeadShadowMinCutoffHz / sampleRate);
        slewCoef          = (float) std::exp (-1.0 / (kSlewSeconds * sampleRate));
    }

    // Clear the tail (call on note-on) so the previous note's samples in the 0.7 ms line don't click.
    void reset()
    {
        std::fill (delayLine.begin(), delayLine.end(), 0.0f);
        writePos = 0;
        lpStateL = lpStateR = slewL = slewR = 0.0f;
    }

    // x = mono input; p = pan (-1 L .. +1 R). The panner computes its OWN ILD so the far ear stays
    // AUDIBLE (attenuated, NOT silenced) — the ITD + head-shadow cues live in the far ear, so zeroing
    // it (as equal-power amplitude panning does at full side) would make the whole binaural effect
    // inaudible. At p=0 both ears are direct/unity → a centered (mono) source, no discontinuity.
    Out process (float x, float p) noexcept
    {
        if (delayLine.empty()) return { x, x };   // unprepared guard

        delayLine[(size_t) writePos] = x;

        // Far-ear ITD delay targets (near ear = 0). Slewed (~1 ms) so a fast-modulated pan doesn't
        // jump the read position → no zipper with linear interpolation.
        const float tgtL = (p > 0.0f) ? maxITDSamples * p        : 0.0f;   // left is FAR when source is right
        const float tgtR = (p < 0.0f) ? maxITDSamples * (-p)     : 0.0f;   // right is FAR when source is left
        slewL += (1.0f - slewCoef) * (tgtL - slewL);
        slewR += (1.0f - slewCoef) * (tgtR - slewR);

        // Head-shadow low-pass coefficient per ear (0 = transparent at center, coefMax at full side).
        const float cL = headShadowCoefMax * std::max (0.0f,  p);
        const float cR = headShadowCoefMax * std::max (0.0f, -p);

        // ILD: near ear unity, far ear attenuated by up to kILDDepth (NOT to zero — keeps the cues).
        const float gainL = 1.0f - kILDDepth * std::max (0.0f,  p);
        const float gainR = 1.0f - kILDDepth * std::max (0.0f, -p);

        const float yL = readFrac (slewL);
        const float yR = readFrac (slewR);

        lpStateL = yL + cL * (lpStateL - yL);   // one-pole LP: out = (1-c)·in + c·prev
        lpStateR = yR + cR * (lpStateR - yR);

        writePos = (writePos + 1) % (int) delayLine.size();
        return { gainL * lpStateL, gainR * lpStateR };
    }

private:
    // Read `d` samples behind the write head, linear-interpolated. d==0 returns the just-written
    // sample (true zero delay for the near ear). d ∈ [0, maxITDSamples].
    float readFrac (float d) const noexcept
    {
        const int size = (int) delayLine.size();
        float rp = (float) writePos - d;
        while (rp < 0.0f) rp += (float) size;
        int i0 = (int) rp;
        const float frac = rp - (float) i0;
        if (i0 >= size) i0 -= size;
        int i1 = i0 + 1; if (i1 >= size) i1 -= size;
        return delayLine[(size_t) i0] + frac * (delayLine[(size_t) i1] - delayLine[(size_t) i0]);
    }

    static constexpr double kMaxITDSeconds         = 0.0009;   // ~0.9 ms max ITD (a touch beyond physical, for a stronger effect)
    static constexpr double kHeadShadowMinCutoffHz = 700.0;    // far-ear low-pass floor at full side (strong shadow)
    static constexpr float  kILDDepth              = 0.85f;    // far-ear level cut at full side (1→0.15, ≈ −16 dB)
    static constexpr double kSlewSeconds           = 0.001;    // delay-target slew (anti-zipper)

    std::vector<float> delayLine;
    int   writePos = 0;
    float maxITDSamples = 0.0f;
    float headShadowCoefMax = 0.0f;
    float slewCoef = 0.0f;
    float lpStateL = 0.0f, lpStateR = 0.0f;
    float slewL = 0.0f, slewR = 0.0f;
};
