#pragma once
#include <cmath>
#include <algorithm>
#include "SampleBank.h"

// ── Per-voice sample playback (Story 12.1) ───────────────────────────────────────────────────
// The genuine opposite of WavetableOscillator (which traverses ONE cycle at pitch rate): here an
// ABSOLUTE read position runs once (or looped) through a long recording at
//   rate = transposeRatio · 2^((60 − rootKey)/12) · fileSR / hostSR
// so the ROOT key plays the file at its original speed and every other key transposes it,
// formants and all (documented in the help text: this is material, not a defect — JASS bends
// recordings, it does not imitate instruments; multisample import is Story 12.2).
//
// Interpolation is 4-point Hermite — MEASURED, not guessed (scratch harness 2026-07-30): on a
// bright 6.7 kHz component transposed +7 semitones, linear = 28.5 dB SNR (audible grit),
// Hermite = 38.3 dB. Cost: a few extra multiplies per voice-sample — irrelevant.
//
// LOOP modes wrap with a short equal-gain crossfade (~6 ms) at the join — the classic loop-click
// defect is designed out rather than fixed later. RT-safe: no allocation anywhere; the set is a
// raw pointer into the never-freed store (same contract as the wavetable bank).
class SamplePlayer
{
public:
    enum class Mode { OneShot = 0, Loop, Reverse, RevLoop };
    struct Out { float l, r; };   // stereo pair; mono sets deliver l == r

    void setSampleRate(double sr)              { hostSampleRate = sr > 0 ? sr : 44100.0; }
    void setSource(const SampleSet* s)         { set = s; }
    void setEnabled(bool e)                    { enabled = e; }
    void setLevel(double l)                    { level = l; }
    void setRootKey(int k)                     { rootKey = k; }
    void setRegion(double startFrac, double endFrac)
    {
        regionStart = std::clamp(startFrac, 0.0, 1.0);
        regionEnd   = std::clamp(endFrac,   0.0, 1.0);
        if (regionEnd < regionStart) std::swap(regionStart, regionEnd);
    }
    void setMode(Mode m)                       { mode = m; }
    void setSpeed(double s)                    { speed = std::clamp(s, 0.05, 8.0); }   // rate multiplier (tape-style)
    // Shared loop clock (Story 12.1 follow-up): the processor advances ONE master loop phase (at
    // ROOT rate) and hands it to every voice per block; loop-mode notes START at that phase, so
    // simultaneous/late notes stay in step (same pitch = sample-locked, octaves = rhythm-locked)
    // instead of each note restarting the loop at START and drifting apart.
    void setLoopSyncPhase(double f)
    {
        f = std::clamp(f, 0.0, 1.0);
        // Master wrap detected (phase jumped back) => HARD-RESYNC every sounding loop voice to the
        // new round, beat-sync style: start-phase alignment alone cannot keep DIFFERENT pitches
        // together (they traverse the region at different rates — physics of resampling), so all
        // loop voices restart each round together; a transposed voice cuts its pass at the wrap.
        const bool wrapped = f < syncPhase - 0.5;
        syncPhase = f;
        if (wrapped && active && set != nullptr && (mode == Mode::Loop || mode == Mode::RevLoop))
            pos = (mode == Mode::Loop) ? startSample() + f * (endSample() - startSample())
                                       : endSample()   - f * (endSample() - startSample());
    }

    bool   isEnabled() const   { return enabled; }
    double getLevel() const    { return level; }     // base capture for per-voice modulation
    bool   sourceIsStereo() const { return set != nullptr && set->isStereo(); }

    // Start playback for a note. transposeRatio = f(note)/f(C4), like every other generator.
    void trigger(double transposeRatio)
    {
        if (set == nullptr || set->getLength() < 4) { active = false; return; }
        rate = transposeRatio
             * std::pow(2.0, (60.0 - (double) rootKey) / 12.0)
             * set->getFileSampleRate() / hostSampleRate;
        const bool rev = (mode == Mode::Reverse || mode == Mode::RevLoop);
        if (mode == Mode::Loop)         pos = startSample() + syncPhase * (endSample() - startSample());
        else if (mode == Mode::RevLoop) pos = endSample()   - syncPhase * (endSample() - startSample());
        else                            pos = rev ? endSample() : startSample();
        active = true;
    }

    void reset() { active = false; }

    Out nextSample()
    {
        if (!enabled || !active || set == nullptr)
            return { 0.0f, 0.0f };

        const double s0 = startSample();
        const double s1 = endSample();
        const double len = s1 - s0;
        if (len < 4.0) { active = false; return { 0.0f, 0.0f }; }

        const bool rev     = (mode == Mode::Reverse || mode == Mode::RevLoop);
        const bool looping = (mode == Mode::Loop    || mode == Mode::RevLoop);
        const bool stereo  = set->isStereo();

        float outL = read(pos, 0);
        float outR = stereo ? read(pos, 1) : outL;

        // Loop join: equal-gain crossfade over the last kXfadeSamples of the region, blending in
        // the material from one loop-length away — by the time pos wraps, the signals are already
        // identical, so the join is click-free by construction.
        if (looping)
        {
            const double xf = std::min((double) kXfadeSamples, len * 0.5);
            float f = -1.0f;
            double other = 0.0;
            if (!rev && pos > s1 - xf)      { f = (float) ((pos - (s1 - xf)) / xf); other = pos - len; }
            else if (rev && pos < s0 + xf)  { f = (float) (((s0 + xf) - pos) / xf); other = pos + len; }
            if (f >= 0.0f)
            {
                outL = outL * (1.0f - f) + read(other, 0) * f;
                outR = stereo ? outR * (1.0f - f) + read(other, 1) * f : outL;
            }
        }

        const double step = rate * speed;   // SPEED rides on top of the key-derived rate, live
        pos += rev ? -step : step;

        if (!rev && pos >= s1)      { if (looping) pos -= len; else active = false; }
        else if (rev && pos <  s0)  { if (looping) pos += len; else active = false; }

        const float g = (float) level;
        return { outL * g, outR * g };
    }

private:
    static constexpr int kXfadeSamples = 256;   // ~6 ms at 44.1k

    double startSample() const { return regionStart * (double) (set->getLength() - 1); }
    double endSample() const   { return regionEnd   * (double) (set->getLength() - 1); }

    // 4-point Hermite (Catmull-Rom) around the fractional position, edge-clamped.
    float read(double p, int ch) const
    {
        const int n = set->getLength();
        const float* d = set->getData(ch);
        p = std::clamp(p, 0.0, (double) (n - 1));
        const int i = (int) p;
        const float f = (float) (p - i);
        const float xm1 = d[std::max(i - 1, 0)];
        const float x0  = d[i];
        const float x1  = d[std::min(i + 1, n - 1)];
        const float x2  = d[std::min(i + 2, n - 1)];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    const SampleSet* set = nullptr;
    bool   enabled = false;
    bool   active  = false;
    double level   = 0.5;
    int    rootKey = 60;
    double regionStart = 0.0, regionEnd = 1.0;
    Mode   mode = Mode::OneShot;
    double hostSampleRate = 44100.0;
    double rate  = 1.0;
    double speed = 1.0;
    double pos   = 0.0;
    double syncPhase = 0.0;   // master loop phase at note-on (shared clock, set per block)
};
