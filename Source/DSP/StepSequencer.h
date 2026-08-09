#pragma once
#include <JuceHeader.h>
#include <array>
#include <algorithm>

// STEP SEQ (Story 15.1) — a 16-step note sequencer. Where the Arpeggiator turns a held chord into
// an automatic sequence, this plays an AUTHORED figure: each step carries its own semitone offset
// and gate, and the whole pattern is transposed by the key you hold (the LOWEST held note is the
// root). Emitted as MIDI into the synth's buffer from processBlock, exactly like Arpeggiator — the
// raw held notes are filtered out upstream so only the pattern sounds.
//
// The reference this was built against is measured, not described (DAF "Der Mussolini", 1981):
// 192.0 ms per step = 156.25 BPM on eighths, dead stable over the full track; a 16-step figure of
// `0,+15,0,+12,0,0,+12,0,+12,0,+12,+10,0,+12,0,+5` semitones over a B pedal; **legato** — the
// envelope never falls between steps — and no accents. Legato is the reason gate is a fraction with
// 1.0 meaning "hold through": at gate 1.0 the previous step's note-off is emitted AFTER the next
// note-on, so the voices overlap instead of leaving a hole. Gate 0 is a rest (no note at all).
//
// Timing is sample-accurate and driven by a step interval the processor resolves once per block
// (tempo-synced via SyncDivision, or the free-running RATE knob). Allocation-free: the pattern is a
// fixed array and the only state is a sample counter plus the note currently sounding.
class StepSequencer
{
public:
    static constexpr int kMaxSteps = 16;

    bool   enabled = false;
    double stepSeconds = 0.192;                  // one step; resolved per block by the processor
    int    length = kMaxSteps;                   // 1..16 steps before the pattern repeats
    std::array<int, kMaxSteps>   pitch {};       // semitone offset from the root, -24..+24
    std::array<double, kMaxSteps> gate {};       // 0 = rest, 1 = legato (hold into the next step)

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
            return;
        }

        const int steps    = juce::jlimit (1, kMaxSteps, length);
        const int interval = juce::jmax (1, (int) (sampleRate * juce::jmax (0.01, stepSeconds)));

        for (int i = 0; i < numSamples; ++i)
        {
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
                const int  s = stepIndex % steps;
                const double g = juce::jlimit (0.0, 1.0, gate[(size_t) s]);
                if (g > 0.0)
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
                    out.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) 100), i);
                    const int prev = soundingNote;   // still sounding => legato tail to close
                    soundingNote = note;
                    gateCountdown = juce::jmax (1, (int) (interval * g));
                    if (prev >= 0)
                        out.addEvent (juce::MidiMessage::noteOff (channel, prev), i);
                }
                else if (soundingNote >= 0)
                {
                    // A rest ends the previous note even if its gate would have run on.
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

private:
    double sampleRate = 44100.0;
    int    root = 60;
    int    sampleCounter = 0;
    int    stepIndex = 0;
    int    soundingNote = -1;
    int    gateCountdown = -1;
};
