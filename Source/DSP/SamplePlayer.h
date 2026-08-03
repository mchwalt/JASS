#pragma once
#include <cmath>
#include <algorithm>
#include "SampleBank.h"

// ── Per-voice sample playback (Story 12.1, zone-aware since Story 12.2) ─────────────────────
// The genuine opposite of WavetableOscillator (which traverses ONE cycle at pitch rate): here an
// ABSOLUTE read position runs once (or looped) through a long recording at
//   rate = transposeRatio · 2^((60 − root)/12) · fileSR / hostSR
// so the root key plays the file at its original speed and every other key transposes it,
// formants and all. Since 12.2 the root comes from the ZONE the played note falls into
// (multisample sets — the ROOT knob is inert there); single-sample sets keep the ROOT knob as
// their live root, exactly the 12.1 behaviour. Zone pointers stay valid forever (never-freed
// store), so caching them across blocks is safe by the same contract as the wavetable bank.
//
// Interpolation is 4-point Hermite — MEASURED, not guessed (scratch harness 2026-07-30): on a
// bright 6.7 kHz component transposed +7 semitones, linear = 28.5 dB SNR (audible grit),
// Hermite = 38.3 dB. Cost: a few extra multiplies per voice-sample — irrelevant.
//
// LOOP modes wrap with a short equal-gain crossfade (~6 ms) at the join — the classic loop-click
// defect is designed out rather than fixed later. RT-safe: no allocation anywhere.
class SamplePlayer
{
public:
    enum class Mode { OneShot = 0, Loop, Reverse, RevLoop };
    struct Out { float l, r; };   // stereo pair; mono zones deliver l == r

    void setSampleRate(double sr)              { hostSampleRate = sr > 0 ? sr : 44100.0; }
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

    // Called every block from applyToVoice. A SET switched mid-note follows the combo (12.1
    // behaviour): re-pick this voice's zone for its note; pos clamps naturally in nextSample
    // against the new zone's region.
    void setSource(const SampleSet* s)
    {
        if (s == set)
            return;
        set  = s;
        zone = (s != nullptr) ? s->zoneFor(lastNote) : nullptr;
        if (! active)
            return;
        if (zone == nullptr || zone->getLength() < 4) { active = false; return; }
        computeRate();
    }

    // Shared loop clock (Story 12.1 follow-up): the processor advances ONE master loop phase (at
    // the reference-zone root rate) and hands it to every voice per block; loop-mode notes START
    // at that phase, so simultaneous/late notes stay in step (same pitch = sample-locked,
    // octaves = rhythm-locked) instead of each note restarting the loop at START and drifting.
    void setLoopSyncPhase(double f)
    {
        f = std::clamp(f, 0.0, 1.0);
        // Master wrap detected (phase jumped back) => HARD-RESYNC every sounding loop voice to the
        // new round, beat-sync style: start-phase alignment alone cannot keep DIFFERENT pitches
        // together (they traverse the region at different rates — physics of resampling), so all
        // loop voices restart each round together; a transposed voice cuts its pass at the wrap.
        const bool wrapped = f < syncPhase - 0.5;
        syncPhase = f;
        if (wrapped && active && zone != nullptr && (mode == Mode::Loop || mode == Mode::RevLoop))
            pos = (mode == Mode::Loop) ? startSample() + f * (endSample() - startSample())
                                       : endSample()   - f * (endSample() - startSample());
    }

    bool   isEnabled() const   { return enabled; }
    double getLevel() const    { return level; }     // base capture for per-voice modulation
    // SET-level stereo (any zone): keeps the voice's L/R sub-source routing consistent with the
    // ±0.5 pan-slot spread computed per block in applyToVoice. A mono zone inside a stereo set
    // delivers l == r and lands centred between the spread slots.
    bool   sourceIsStereo() const { return set != nullptr && set->isStereo(); }

    // Start playback for a note. transposeRatio = f(note)/f(C4), like every other generator;
    // midiNote picks the zone (12.2) — mapped sets use ITS root, single samples the ROOT knob.
    void trigger(double transposeRatio, int midiNote)
    {
        lastRatio = transposeRatio;
        lastNote  = std::clamp(midiNote, 0, 127);
        zone = (set != nullptr) ? set->zoneFor(lastNote) : nullptr;
        if (zone == nullptr || zone->getLength() < 4) { active = false; return; }
        computeRate();
        const bool rev = (mode == Mode::Reverse || mode == Mode::RevLoop);
        if (mode == Mode::Loop)         pos = startSample() + syncPhase * (endSample() - startSample());
        else if (mode == Mode::RevLoop) pos = endSample()   - syncPhase * (endSample() - startSample());
        else                            pos = rev ? endSample() : startSample();
        active = true;
    }

    void reset() { active = false; }

    Out nextSample()
    {
        if (!enabled || !active || zone == nullptr)
            return { 0.0f, 0.0f };

        const double s0 = startSample();
        const double s1 = endSample();
        const double len = s1 - s0;
        if (len < 4.0) { active = false; return { 0.0f, 0.0f }; }

        const bool rev     = (mode == Mode::Reverse || mode == Mode::RevLoop);
        const bool looping = (mode == Mode::Loop    || mode == Mode::RevLoop);
        const bool stereo  = zone->isStereo();

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

    double startSample() const { return regionStart * (double) (zone->getLength() - 1); }
    double endSample() const   { return regionEnd   * (double) (zone->getLength() - 1); }

    // Fixed at note-on (and on a mid-note SET switch): a mapped set's zone root wins, a single
    // sample follows the ROOT knob as captured at trigger time — 12.1 semantics unchanged.
    void computeRate()
    {
        const double root = (set != nullptr && set->isMapped()) ? (double) zone->rootKey
                                                                : (double) rootKey;
        rate = lastRatio
             * std::pow(2.0, (60.0 - root) / 12.0)
             * zone->fileSampleRate / hostSampleRate;
    }

    // 4-point Hermite (Catmull-Rom) around the fractional position, edge-clamped.
    float read(double p, int ch) const
    {
        const int n = zone->getLength();
        const float* d = zone->getData(ch);
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

    const SampleSet*  set  = nullptr;
    const SampleZone* zone = nullptr;   // this voice's zone (picked at trigger / SET switch)
    bool   enabled = false;
    bool   active  = false;
    double level   = 0.5;
    int    rootKey = 60;
    int    lastNote = 60;               // note that picked the zone (kept for SET switches)
    double lastRatio = 1.0;             // transposeRatio at note-on (kept for SET switches)
    double regionStart = 0.0, regionEnd = 1.0;
    Mode   mode = Mode::OneShot;
    double hostSampleRate = 44100.0;
    double rate  = 1.0;
    double speed = 1.0;
    double pos   = 0.0;
    double syncPhase = 0.0;   // master loop phase at note-on (shared clock, set per block)
};
