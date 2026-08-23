#pragma once
#include <JuceHeader.h>
#include <array>
#include <algorithm>

// STEP SEQ (Story 15.1) — a 32-step note sequencer. Where the Arpeggiator turns a held chord into
// an automatic sequence, this plays an AUTHORED figure: each step carries its own semitone offset,
// and the whole pattern is transposed by the key you hold (the LOWEST held note is the root).
// Emitted as MIDI into the synth's buffer from processBlock, exactly like Arpeggiator — the raw
// held notes are filtered out upstream so only the pattern sounds.
//
// A step has a pitch and an ON switch; note LENGTH is one global gate rather than a value per step.
// That split took two rounds to find. Per-step gate knobs went first (they were all sitting at 1 —
// the length of a single step is a refinement almost no figure uses). The switches that replaced
// them went too, briefly, because in a LEGATO figure a silent step reads as the sound breaking off
// rather than as rhythm. They came back for percussion, where a targeted gap is exactly the point —
// but in the corner of the pitch knob instead of a row of their own, so they cost no rack height.
//
// The reference this was built against is measured, not described (DAF "Der Mussolini", 1981):
// 192.0 ms per step = 156.25 BPM on eighths, dead stable over the full track; a 16-step figure of
// `0,+15,0,+12,0,0,+12,0,+12,0,+12,+10,0,+12,0,+5` semitones over a B pedal; **legato** — the
// envelope never falls between steps — and no accents. Legato is the reason gate is a fraction with
// 1.0 meaning "hold through": at gate 1.0 the previous step's note-off is emitted AFTER the next
// note-on, so the voices overlap instead of leaving a hole.
//
// Timing is sample-accurate and driven by a step interval the processor resolves once per block
// (tempo-synced via SyncDivision, or the free-running RATE knob). Allocation-free: the pattern is a
// fixed array and the only state is a sample counter plus the note currently sounding.
class StepSequencer
{
public:
    static constexpr int kMaxSteps = 32;

    bool   enabled = false;
    double stepSeconds = 0.192;                  // one step; resolved per block by the processor
    int    length = kMaxSteps;                   // 1..32 steps before the pattern repeats
    std::array<int, kMaxSteps>  pitch {};        // semitone offset from the root, -24..+24
    std::array<bool, kMaxSteps> on {};           // false = rest: that step stays silent
    std::array<bool, kMaxSteps> accent {};       // 15.2: an accented step is emitted HOT (velocity
                                                 // 127 vs the plain 100) — what that does to the
                                                 // sound is the voice's ACCENT mapping, so a MIDI
                                                 // export of the figure carries the accents for free
    double gate = 1.0;                           // note length, ONE value for the whole pattern

    void prepare (double sr) { sampleRate = sr; reset(); }

    void reset()
    {
        sampleCounter = 0;
        stepIndex = 0;
        soundingNote = -1;
        gateCountdown = -1;
    }

    // Release whatever the pattern left sounding (switched off, or the last key let go).
    void releaseAll (juce::MidiBuffer& out, int channel, int sampleOffset = 0)
    {
        if (soundingNote >= 0)
        {
            out.addEvent (juce::MidiMessage::noteOff (channel, soundingNote), sampleOffset);
            soundingNote = -1;
        }
        gateCountdown = -1;
    }

    void setRoot (int midiNote) { root = juce::jlimit (0, 127, midiNote); }

    // Quantised entry (Story 16.1): hold down the samples until PERC's pattern wraps, so a figure
    // started mid-bar joins the beat on its downbeat instead of running a fixed distance off it
    // forever. Set once by the processor at the moment the first key arrives; ignored (0) when no
    // drum pattern is running, which leaves 15.1's immediate start exactly as it was.
    void setStartDelay (int samples) { startDelay = juce::jmax (0, samples); }

    void processBlock (int numSamples, juce::MidiBuffer& out, int channel, bool anyKeyHeld)
    {
        if (! anyKeyHeld)
        {
            // Nothing held: stop, and re-arm so the NEXT key starts the figure at step 0 rather
            // than wherever a free-running counter happened to be. Adding a key to an already
            // running pattern does not restart it — only playing from silence does.
            releaseAll (out, channel);
            sampleCounter = 0;
            stepIndex = 0;
            litStep   = -1;   // nothing running => no playhead
            startDelay = 0;   // a new entry gets a fresh quantisation, not the last one's leftover
            return;
        }

        const int steps    = juce::jlimit (1, kMaxSteps, length);
        const int interval = juce::jmax (1, (int) (sampleRate * juce::jmax (0.01, stepSeconds)));

        for (int i = 0; i < numSamples; ++i)
        {
            // Waiting for the drums' downbeat: nothing sounds and the clock does not advance, so
            // the pattern still enters at step 0 — it just enters later.
            if (startDelay > 0)
            {
                --startDelay;
                continue;
            }

            // Gate expiry: release the sounding note. At gate 1.0 the countdown reaches the step
            // boundary, where the note-on below is emitted FIRST (same sample) — so a legato
            // pattern overlaps by construction and never leaves a gap.
            if (gateCountdown == 0 && soundingNote >= 0)
            {
                out.addEvent (juce::MidiMessage::noteOff (channel, soundingNote), i);
                soundingNote = -1;
            }
            if (gateCountdown > 0)
                --gateCountdown;

            if (sampleCounter == 0)
            {
                const int s = stepIndex % steps;
                litStep = s;   // the playhead: what is ON now, not the next one (see playingStep)
                const double g = juce::jlimit (0.05, 1.0, gate);
                if (on[(size_t) s])
                {
                    const int note = juce::jlimit (0, 127, root + pitch[(size_t) s]);
                    // Same note as the one still sounding? Re-strike it: note-off first, or the
                    // synth would see two note-ons for one key and the note-off of the first would
                    // kill the second. A DIFFERENT note is left alone — that is the legato overlap.
                    if (soundingNote == note)
                    {
                        out.addEvent (juce::MidiMessage::noteOff (channel, soundingNote), i);
                        soundingNote = -1;
                    }
                    out.addEvent (juce::MidiMessage::noteOn (channel, note,
                                                             (juce::uint8) (accent[(size_t) s] ? 127 : 100)), i);
                    const int prev = soundingNote;   // still sounding => legato tail to close
                    soundingNote = note;
                    gateCountdown = juce::jmax (1, (int) (interval * g));
                    if (prev >= 0)
                        out.addEvent (juce::MidiMessage::noteOff (channel, prev), i);
                }
                else if (soundingNote >= 0)
                {
                    // A rest ends the note that is running, even mid-gate. That IS a hole in a
                    // legato pattern — which is what a rest is for.
                    out.addEvent (juce::MidiMessage::noteOff (channel, soundingNote), i);
                    soundingNote = -1;
                    gateCountdown = -1;
                }
                stepIndex = (stepIndex + 1) % steps;
            }

            if (++sampleCounter >= interval)
                sampleCounter = 0;
        }
    }

    int currentStep() const noexcept { return stepIndex; }
    // The step the pattern is ON right now, for the module's playhead — NOT `stepIndex`, which has
    // already been advanced to the next one by the time anybody looks. -1 when nothing is running.
    int playingStep() const noexcept { return litStep; }
    // The MIDI note the pattern is holding right now, or -1 between notes. The sequencer writes its
    // notes straight into the MIDI buffer and never touches the MidiKeyboardState — deliberately, or
    // its own notes would come back as held keys and the root search would follow the pattern around.
    // So the on-screen keyboard cannot see them, and the processor mirrors this into an atomic for it.
    int currentNote() const noexcept { return soundingNote; }

private:
    double sampleRate = 44100.0;
    int    root = 60;
    int    sampleCounter = 0;
    int    stepIndex = 0;
    int    litStep = -1;      // step currently sounding (playhead); stepIndex is already the next one
    int    soundingNote = -1;
    int    gateCountdown = -1;
    int    startDelay = 0;   // samples still to wait before the first step (quantised entry, 16.1)
};
