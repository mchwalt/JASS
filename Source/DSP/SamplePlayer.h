#pragma once
#include <cmath>
#include <algorithm>
#include <atomic>
#include "SampleBank.h"
// Vendored third-party (MIT, see ThirdParty README): not ours to keep warning-clean — silence
// its conversion/shadowing noise HERE instead of editing upstream files (breaks the update path).
JUCE_BEGIN_IGNORE_WARNINGS_MSVC (4127 4244 4305 4456 4458)
JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wconversion", "-Wshadow", "-Wfloat-conversion", "-Wsign-conversion", "-Wswitch-enum")
#include "../ThirdParty/signalsmith-stretch/signalsmith-stretch.h"
JUCE_END_IGNORE_WARNINGS_GCC_LIKE
JUCE_END_IGNORE_WARNINGS_MSVC

// ── Per-voice sample playback (Story 12.1, zones 12.2, pitch/time decoupling 12.3) ──────────
// TAPE mode (default, 12.1): an ABSOLUTE read position runs once (or looped) through the
// recording at  rate = transposeRatio · 2^((60 − root)/12) · fileSR / hostSR  — pitch and time
// are one number, formants shift with pitch. Since 12.2 the root comes from the ZONE the played
// note falls into (multisample sets); single samples keep the ROOT knob live.
//
// STRETCH mode (12.3, `samplerStretch`): pitch and time are DECOUPLED. The read position walks
// the TIME axis only (speed · fileSR/hostSR — no pitch factor), feeding a per-voice
// signalsmith-stretch instance that shifts pitch at time-rate 1:1. Consequences: every loop
// voice traverses START..END in the same wall-clock time regardless of pitch, so transposed
// loop voices stay beat-locked BY CONSTRUCTION (the master-wrap hard resync of tape mode is
// skipped — nothing drifts); recordings keep their duration across the keyboard. Cost, MEASURED
// (bake-off 2026-08-03, story 12.3 Dev Agent Record): ~60 ms latency and ~19% of one core for
// 16 stereo voices with the chosen 0.06/0.015 config (~35–40 dB spectral SNR at ±7/±12 st,
// metric ceiling 43 dB; the naive granular alternative measured NEGATIVE SNR and was dropped).
// Engine RT-contract verified: configure() allocates (called from setSampleRate only, i.e.
// prepareToPlay); process()/reset() stay within capacity established there.
//
// Interpolation is 4-point Hermite — MEASURED (2026-07-30): linear 28.5 dB vs Hermite 38.3 dB
// SNR on a bright component at +7 st. LOOP joins wrap with a ~6 ms equal-gain crossfade (in
// BOTH modes — in stretch mode the crossfade is applied to the input feed).
class SamplePlayer
{
public:
    enum class Mode { OneShot = 0, Loop, Reverse, RevLoop };
    struct Out { float l, r; };   // stereo pair; mono zones deliver l == r

    void setSampleRate(double sr)
    {
        hostSampleRate = sr > 0 ? sr : 44100.0;
        if (hostSampleRate != stretchConfiguredRate)   // prepareToPlay only — configure ALLOCATES
        {
            // "short" config from the 12.3 bake-off: 60 ms latency, ~35–40 dB, same CPU as the
            // 120 ms default. presetDefault's extra ~5 dB is not worth doubling the latency for
            // a playable generator.
            stretch.configure(2, (int) (hostSampleRate * 0.06), (int) (hostSampleRate * 0.015));
            // Note-on pre-roll scratch (see trigger): inputLatency+outputLatency frames.
            seekBufL.resize((size_t) (stretch.inputLatency() + stretch.outputLatency()));
            seekBufR.resize(seekBufL.size());
            stretchConfiguredRate = hostSampleRate;
        }
    }
    void setEnabled(bool e)                    { enabled = e; }
    void setLevel(double l)                    { level = l; }
    void setRootKey(int k)                     { rootKey = k; }
    // 12.4: REL knob — the note-off fade for zones the .sfz gave no ampeg_release. 0 = OFF.
    void setReleaseFallback(double seconds)    { relFallback = std::clamp(seconds, 0.0, 8.0); }
    void setRegion(double startFrac, double endFrac)
    {
        regionStart = std::clamp(startFrac, 0.0, 1.0);
        regionEnd   = std::clamp(endFrac,   0.0, 1.0);
        if (regionEnd < regionStart) std::swap(regionStart, regionEnd);
    }
    void setMode(Mode m)                       { mode = m; }
    void setSpeed(double s)                    { speed = std::clamp(s, 0.05, 8.0); }   // rate multiplier

    // 12.3: pitch/time decoupling toggle. Switching MID-NOTE re-parks the engine (reset stays
    // within configure()'s capacity — no allocation) so the voice continues cleanly in the new
    // regime instead of replaying stale FIFO content.
    void setStretchMode(bool m)
    {
        if (m == stretchMode)
            return;
        stretchMode = m;
        if (active && m) { stretch.reset(); outRead = kChunk; drainRemaining = -1; }
    }

    // Called every block from applyToVoice. A SET switched mid-note follows the combo (12.1
    // behaviour): re-pick this voice's zone for its note; pos clamps naturally against the new
    // zone's region.
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
        // Review 2026-08-04: in stretch mode the engine + FIFO still hold the OLD sample's
        // audio — re-park like setStretchMode does, so the voice continues in the new source
        // instead of replaying stale content (brief warm-up gap, acceptable for a combo edit).
        if (stretchMode) { stretch.reset(); outRead = kChunk; drainRemaining = -1; }
    }

    // Shared loop clock: the processor advances ONE master loop phase and hands it to every
    // voice per block; loop-mode notes START at that phase, so simultaneous/late notes stay in
    // step. The HARD resync at each master wrap runs in BOTH modes (review 2026-08-04): in tape
    // mode it catches pitch-rate drift; in stretch mode same-zone voices never drift, so the
    // resync is an exact no-op for them — but a MULTISAMPLE set's zones can differ in length /
    // file rate, and without the resync those loop voices would drift apart forever.
    void setLoopSyncPhase(double f)
    {
        f = std::clamp(f, 0.0, 1.0);
        const bool wrapped = f < syncPhase - 0.5;
        syncPhase = f;
        if (wrapped && active && zone != nullptr
            && (mode == Mode::Loop || mode == Mode::RevLoop))
            pos = (mode == Mode::Loop) ? startSample() + f * (endSample() - startSample())
                                       : endSample()   - f * (endSample() - startSample());
    }

    bool   isEnabled() const   { return enabled; }
    double getLevel() const    { return level; }     // base capture for per-voice modulation
    // SET-level stereo (any zone): keeps the voice's L/R sub-source routing consistent with the
    // ±0.5 pan-slot spread computed per block in applyToVoice.
    bool   sourceIsStereo() const { return set != nullptr && set->isStereo(); }

    // Start playback for a note. transposeRatio = f(note)/f(C4); midiNote picks the zone (12.2).
    void trigger(double transposeRatio, int midiNote)
    {
        // Retrigger declick (user report 2026-08-04: fast piano playing crackled): a reused
        // voice jumps from mid-ring to the attack — an unsmoothed waveform step. TAPE mode
        // crossfades ~6 ms of the OLD material (position/rate/zone captured here; zone pointers
        // never die — never-freed store). STRETCH mode can't crossfade by position (pos is the
        // input FEED, ~60 ms ahead of what is heard), so it kills the step with a decaying
        // last-output remnant instead.
        if (active && zone != nullptr)
        {
            if (! stretchMode)
            {
                dkZone   = zone;
                dkPos    = pos;
                dkStep   = ((mode == Mode::Reverse || mode == Mode::RevLoop) ? -1.0 : 1.0) * rate * speed;
                dkRemain = kXfadeSamples;
            }
            else
            {
                clickL = lastL;
                clickR = lastR;
            }
        }
        lastRatio = transposeRatio;
        lastNote  = std::clamp(midiNote, 0, 127);
        zone = (set != nullptr) ? set->zoneFor(lastNote) : nullptr;
        if (zone == nullptr || zone->getLength() < 4) { active = false; return; }
        computeRate();
        const bool rev = (mode == Mode::Reverse || mode == Mode::RevLoop);
        if (mode == Mode::Loop)         pos = startSample() + syncPhase * (endSample() - startSample());
        else if (mode == Mode::RevLoop) pos = endSample()   - syncPhase * (endSample() - startSample());
        else                            pos = rev ? endSample() : startSample();
        active   = true;
        released = false;   // 12.4: a retrigger re-arms the gate; the old ramp state is void
        relGain  = 1.0f;
        if (stretchMode)
        {
            // Pre-roll the engine at note-on (outputSeek): without this the first ~60 ms of
            // output are warm-up SILENCE, which made fast playing impossible (user report
            // 2026-08-04). outputSeek computes the latency's worth of output in one go —
            // MEASURED 0.57 ms per stereo voice (seekbench) — so the attack is immediate.
            // No allocation: it works inside capacities established by configure().
            drainRemaining = -1;   // BEFORE fillInput — a short region may end during pre-roll
            outRead = kChunk;
            if (! seekBufL.empty())   // sized in setSampleRate; empty only before prepareToPlay
            {
                // Burst cap (review 2026-08-04, user decision "B"): at most kMaxSeeksPerBlock
                // full pre-rolls per audio block — a sequencer chord landing N note-ons in ONE
                // callback would stack N × 0.57 ms and blow small-buffer deadlines. Overflow
                // voices take the CHEAP seek (0.01 ms): they still sound, their attack just
                // arrives ~outputLatency (~30 ms) late. The budget refills every block.
                if (seekBudget.fetch_sub(1, std::memory_order_relaxed) > 0)
                {
                    const int n = (int) seekBufL.size();
                    fillInput(seekBufL.data(), seekBufR.data(), n);
                    float* bufs[2] = { seekBufL.data(), seekBufR.data() };
                    stretch.outputSeek(bufs, n);
                }
                else
                {
                    stretch.reset();
                    const int n = stretch.inputLatency();
                    fillInput(seekBufL.data(), seekBufR.data(), n);
                    float* bufs[2] = { seekBufL.data(), seekBufR.data() };
                    stretch.seek(bufs, n, 1.0);
                }
            }
            else
                stretch.reset();
        }
    }

    // Refilled by the processor ONCE per audio block (before the voices render), so the note-on
    // pre-roll burst above stays bounded no matter how many voices start in the same callback.
    static void resetSeekBudget() { seekBudget.store(kMaxSeeksPerBlock, std::memory_order_relaxed); }

    // 12.4: true while the note-off fade is still audible (ramp running, or a stretch tail
    // draining behind it). The voice holds its ADSR-bypass gate open on this — so the sampler's
    // own release also works with the ENVELOPE module switched OFF (user report 2026-08-04:
    // the 10 ms bypass gate cut the tail the moment the key was released).
    bool isRingingOut() const { return enabled && released && (active || drainRemaining > 0); }

    // ── Story 12.4: the sampler's OWN release ────────────────────────────────────────────────
    // A recording carries its attack/decay/sustain in the material itself — the only envelope
    // job left to the sampler is the NOTE-OFF fade. gateOff starts a per-voice exponential ramp
    // (time-to-−60 dB, the common ampeg_release reading): the zone's .sfz value wins, the REL
    // knob covers zones without one, neither (≤0) keeps the pre-12.4 behaviour — no sampler-side
    // fade, the global ADSR/gate alone shapes the tail. The ramp rides INSIDE the voice gain:
    // the global ADSR still multiplies on top (its release is the audible CEILING — documented
    // in the help: sampled instruments want A 0 / D 0 / S max / R ≥ the longest fade). Because
    // released voices now decay on their own, same-note retriggers and steals hit material that
    // is already fading — the "hard cut" artefact of fast playing goes away with it.
    void gateOff()
    {
        if (! active || zone == nullptr)
            return;
        const double sec = zone->releaseSeconds >= 0.0f ? (double) zone->releaseSeconds
                                                        : relFallback;
        if (sec <= 0.0)
            return;   // no sampler-side release configured — legacy behaviour
        released = true;
        relCoef  = (float) std::exp(-6.907755 / (std::max(sec, 0.005) * hostSampleRate));
    }

    // Hard stop — also the first half of a VOICE STEAL (stopNote(false) + immediate restart).
    // Arm the retrigger declick HERE: by the time trigger() runs, active is already false, so
    // capturing at trigger alone missed exactly the stolen-voice case (user report 2026-08-04:
    // fast piano playing crackled hardest once the 16 voices were exhausted).
    void reset()
    {
        if (active && zone != nullptr)
        {
            if (! stretchMode)
            {
                dkZone   = zone;
                dkPos    = pos;
                dkStep   = ((mode == Mode::Reverse || mode == Mode::RevLoop) ? -1.0 : 1.0) * rate * speed;
                dkRemain = kXfadeSamples;
            }
            else
            {
                clickL = lastL;
                clickR = lastR;
            }
        }
        active = false;
        drainRemaining = -1;
    }

    Out nextSample()
    {
        if (!enabled || zone == nullptr || (!active && !(stretchMode && drainRemaining > 0)))
            return { 0.0f, 0.0f };

        const double s0 = startSample();
        const double s1 = endSample();
        const double len = s1 - s0;
        if (len < 4.0) { active = false; return { 0.0f, 0.0f }; }

        const float g = (float) level * relGain;   // 12.4: the release ramp rides in the gain
        if (stretchMode)
        {
            if (outRead >= kChunk)
                refillStretchChunk();
            float l = outBufL[outRead] * g, r = outBufR[outRead] * g;
            ++outRead;
            // Drain accounting lives on the OUTPUT side (review 2026-08-04): the note-on
            // pre-roll must not consume the budget (it made short one-shots fully silent),
            // and counting inputs cut the FIFO's last partial chunk of tail.
            if (! active && drainRemaining > 0)
                --drainRemaining;
            l += clickL; r += clickR;          // retrigger step suppressor (decaying remnant)
            clickL *= kClickDecay; clickR *= kClickDecay;
            advanceRelease();
            lastL = l; lastR = r;
            return { l, r };
        }

        // ---- TAPE mode (12.1 + retrigger declick) -----------------------------------------
        const bool rev     = (mode == Mode::Reverse || mode == Mode::RevLoop);
        const bool looping = (mode == Mode::Loop    || mode == Mode::RevLoop);
        const bool stereo  = zone->isStereo();

        float outL = readXf(pos, 0, s0, s1, len, rev, looping);
        float outR = stereo ? readXf(pos, 1, s0, s1, len, rev, looping) : outL;

        if (dkRemain > 0)   // blend ~6 ms of the pre-retrigger material over the new attack
        {
            const float f = (float) dkRemain / (float) kXfadeSamples;
            const float oL = readZone(dkZone, dkPos, 0);
            const float oR = dkZone->isStereo() ? readZone(dkZone, dkPos, 1) : oL;
            outL = outL * (1.0f - f) + oL * f;
            outR = outR * (1.0f - f) + oR * f;
            dkPos += dkStep;
            --dkRemain;
        }

        const double step = rate * speed;   // SPEED rides on top of the key-derived rate, live
        pos += rev ? -step : step;

        if (!rev && pos >= s1)      { if (looping) pos -= len; else active = false; }
        else if (rev && pos <  s0)  { if (looping) pos += len; else active = false; }

        advanceRelease();
        lastL = outL * g; lastR = outR * g;
        return { lastL, lastR };
    }

private:
    static constexpr int   kXfadeSamples = 256;   // ~6 ms at 44.1k (loop join + retrigger declick)
    static constexpr int   kChunk = 64;           // stretch FIFO granularity (~1.5 ms — negligible)
    static constexpr int   kMaxSeeksPerBlock = 4; // note-on pre-roll burst cap (user decision "B")
    static constexpr float kClickDecay = 0.97f;   // stretch retrigger remnant: ~-60 dB in ~5 ms
    static constexpr float kRelFloor   = 1.0e-4f; // release ramp floor (−80 dB) — voice is spent
    // Shared across all voices (and plugin instances — they also share the CPU): the per-block
    // budget of full note-on pre-rolls. Reset by the processor each block (resetSeekBudget).
    static inline std::atomic<int> seekBudget { kMaxSeeksPerBlock };

    double startSample() const { return regionStart * (double) (zone->getLength() - 1); }
    double endSample() const   { return regionEnd   * (double) (zone->getLength() - 1); }

    // 12.4: advance the release ramp one OUTPUT sample. At the −80 dB floor the tail is spent:
    // stop the voice's sampler (and any stretch drain) so it reads no more material.
    void advanceRelease()
    {
        if (! released)
            return;
        relGain *= relCoef;
        if (relGain < kRelFloor)
        {
            released = false;
            relGain  = 0.0f;   // silent until the next trigger() re-arms it
            active   = false;
            drainRemaining = -1;
        }
    }

    // Fixed at note-on (and on a mid-note SET switch). TAPE: full coupled rate. STRETCH: the
    // pitch part goes to the engine as a transpose factor, the time part (fileSR/hostSR) stays
    // in the walk step. A mapped set's zone root wins, a single sample follows the ROOT knob.
    void computeRate()
    {
        const double root = (set != nullptr && set->isMapped()) ? (double) zone->rootKey
                                                                : (double) rootKey;
        const double pitchFactor = lastRatio * std::pow(2.0, (60.0 - root) / 12.0);
        timeStepBase = zone->fileSampleRate / hostSampleRate;
        rate = pitchFactor * timeStepBase;
        stretch.setTransposeFactor((float) pitchFactor);
    }

    // Region read with the loop-join crossfade (equal-gain blend against material one
    // loop-length away). Shared by the tape path and the stretch input feed.
    float readXf(double p, int ch, double s0, double s1, double len, bool rev, bool looping) const
    {
        float out = read(p, ch);
        if (! looping)
            return out;
        const double xf = std::min((double) kXfadeSamples, len * 0.5);
        float f = -1.0f;
        double other = 0.0;
        if (!rev && p > s1 - xf)      { f = (float) ((p - (s1 - xf)) / xf); other = p - len; }
        else if (rev && p < s0 + xf)  { f = (float) (((s0 + xf) - p) / xf); other = p + len; }
        if (f >= 0.0f)
            out = out * (1.0f - f) + read(other, ch) * f;
        return out;
    }

    // STRETCH: walk n frames along the time axis (speed only — the decoupled axis) into L/R.
    // Shared by the per-chunk refill and the note-on pre-roll. After a one-shot's input ends,
    // zeros are fed until the engine's latency has drained, so the shifted tail is not cut.
    void fillInput(float* L, float* R, int n)
    {
        const double s0 = startSample();
        const double s1 = endSample();
        const double len = s1 - s0;
        const bool rev     = (mode == Mode::Reverse || mode == Mode::RevLoop);
        const bool looping = (mode == Mode::Loop    || mode == Mode::RevLoop);
        const bool stereo  = zone->isStereo();
        const double step  = timeStepBase * speed;

        for (int k = 0; k < n; ++k)
        {
            if (! active)
            {
                L[k] = R[k] = 0.0f;   // drain the engine's pipeline with silence
                continue;             // (budget is debited per OUTPUT sample in nextSample)
            }
            const float l = readXf(pos, 0, s0, s1, len, rev, looping);
            L[k] = l;
            R[k] = stereo ? readXf(pos, 1, s0, s1, len, rev, looping) : l;
            pos += rev ? -step : step;
            // Input ended (one-shot): grant the full pipeline depth as an OUTPUT budget so the
            // shifted tail — including anything pre-rolled at note-on — drains completely.
            if (!rev && pos >= s1)      { if (looping) pos -= len; else { active = false; drainRemaining = stretch.inputLatency() + stretch.outputLatency(); } }
            else if (rev && pos <  s0)  { if (looping) pos += len; else { active = false; drainRemaining = stretch.inputLatency() + stretch.outputLatency(); } }
        }
    }

    void refillStretchChunk()
    {
        fillInput(inBufL, inBufR, kChunk);
        const float* ins[2]  = { inBufL, inBufR };
        float*       outs[2] = { outBufL, outBufR };
        stretch.process(ins, kChunk, outs, kChunk);
        outRead = 0;
    }

    // 4-point Hermite (Catmull-Rom) around the fractional position, edge-clamped.
    float read(double p, int ch) const { return readZone(zone, p, ch); }

    static float readZone(const SampleZone* z, double p, int ch)
    {
        const int n = z->getLength();
        const float* d = z->getData(ch);
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
    double rate  = 1.0;                 // tape-mode step (pitch × time)
    double timeStepBase = 1.0;          // stretch-mode step base (fileSR/hostSR, time only)
    double speed = 1.0;
    double pos   = 0.0;
    double syncPhase = 0.0;             // master loop phase (shared clock, set per block)

    // ---- 12.3 stretch engine (per voice) ----
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    bool   stretchMode = false;
    double stretchConfiguredRate = 0.0;
    int    outRead = kChunk;            // FIFO exhausted => refill on next sample
    int    drainRemaining = -1;         // >0: one-shot ended, draining engine latency
    float  inBufL[kChunk] {}, inBufR[kChunk] {};
    float  outBufL[kChunk] {}, outBufR[kChunk] {};
    std::vector<float> seekBufL, seekBufR;   // note-on pre-roll scratch (sized in setSampleRate)

    // ---- 12.4 release ramp (per voice) ----
    double relFallback = 0.0;             // REL knob (seconds to −60 dB); 0 = OFF
    bool   released = false;              // note-off received, ramp running
    float  relGain  = 1.0f;               // current ramp gain (1 → 0)
    float  relCoef  = 1.0f;               // per-sample decay factor (set at gateOff)

    // ---- retrigger declick (user report 2026-08-04) ----
    const SampleZone* dkZone = nullptr;   // old material continuing under the new attack (tape)
    double dkPos = 0.0, dkStep = 0.0;
    int    dkRemain = 0;
    float  clickL = 0.0f, clickR = 0.0f;  // decaying output remnant (stretch retrigger)
    float  lastL = 0.0f, lastR = 0.0f;    // last emitted pair (captured at retrigger)
};
