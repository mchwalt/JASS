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
    // 16.3: the pattern capacity — a GENEROUS cap (the APVTS parameter list is fixed at
    // construction, so unlike the engine arrays it cannot grow at runtime; the arrays cost
    // kilobytes, the cap's real price is parameter COUNT — 4 per step). 16 pages of 48 holds a
    // 48-bar line of 16ths: a whole song section chain, the Roboter full melody included
    // (maintainer's call, 2026-09-02). The UI shows kPageSteps at a time behind the < n/16 >
    // pager; everything else derives from kMaxSteps — no other literal.
    static constexpr int kMaxSteps  = 768;   // was 192 (16.3), 48 (16.2), 32 before that
    static constexpr int kPageSteps = 48;    // one UI page: two knob rows of 24 (16.2 geometry)

    // 15.7: the per-step gate is ONE continuum (the BeatStep model) — 5..100 = percent of the
    // step, then two values past the top: TIE (held through the boundary; the next step takes
    // over WITHOUT a retrigger) and SLIDE (like TIE, but the pitch glides — the 303's slide).
    static constexpr int kGateTie   = 101;
    static constexpr int kGateSlide = 102;
    static constexpr double kSlideSeconds = 0.06;   // 303-ish glide; tuned by the maintainer's ear

    bool   enabled = false;
    double stepSeconds = 0.192;                  // one step; resolved per block by the processor
    int    length = kMaxSteps;                   // 1..32 steps before the pattern repeats
    std::array<int, kMaxSteps>  pitch {};        // semitone offset from the root, -24..+24
    std::array<bool, kMaxSteps> on {};           // false = rest: that step stays silent
    std::array<bool, kMaxSteps> accent {};       // 15.2: an accented step is emitted HOT (velocity
                                                 // 127 vs the plain 100) — what that does to the
                                                 // sound is the voice's ACCENT mapping, so a MIDI
                                                 // export of the figure carries the accents for free
    std::array<int, kMaxSteps>  sgate {};        // 15.7: 5..100 %, kGateTie, kGateSlide (see above);
                                                 // filled per block by the processor, default 100
    double gate = 1.0;                           // GLOBAL note length, scales every plain step

    // 15.7: a TIE/SLIDE boundary retunes the sounding voice instead of retriggering it. The
    // sequencer only speaks MIDI, and MIDI cannot say "change pitch, keep the envelope" — so the
    // takeover is recorded here and the PROCESSOR applies it to the voice after this block's
    // events are in (block-granular start, inaudible against a ≥60 ms step). `note` is the MIDI
    // identity the voice was started with (unchanged across a whole tie chain, so the final
    // note-off still finds it); `semitones` is the move from the pitch currently SOUNDING.
    struct Legato { int note; int semitones; bool slide; };

    StepSequencer() { sgate.fill (100); }

    void prepare (double sr) { sampleRate = sr; reset(); }

    void reset()
    {
        sampleCounter = 0;
        stepIndex = 0;
        soundingNote = -1;
        gateCountdown = -1;
        tiePending = false;
        numLegato = 0;
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
        tiePending = false;
    }

    void setRoot (int midiNote) { root = juce::jlimit (0, 127, midiNote); }

    // Quantised entry (Story 16.1): hold down the samples until PERC's pattern wraps, so a figure
    // started mid-bar joins the beat on its downbeat instead of running a fixed distance off it
    // forever. Set once by the processor at the moment the first key arrives; ignored (0) when no
    // drum pattern is running, which leaves 15.1's immediate start exactly as it was.
    void setStartDelay (int samples) { startDelay = juce::jmax (0, samples); }

    void processBlock (int numSamples, juce::MidiBuffer& out, int channel, bool anyKeyHeld)
    {
        numLegato = 0;   // 15.7: last block's takeovers were applied by the processor already

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
                const double g  = juce::jlimit (0.05, 1.0, gate);
                const int    sg = juce::jlimit (5, 102, sgate[(size_t) s]);
                if (on[(size_t) s])
                {
                    const int note = juce::jlimit (0, 127, root + pitch[(size_t) s]);
                    if (tiePending && soundingNote >= 0)
                    {
                        // 15.7 TIE/SLIDE takeover: the previous step held through this boundary,
                        // so this step does NOT retrigger. A different pitch is taken over by
                        // retuning the sounding voice (slide glides there, tie steps); the voice
                        // keeps its envelope and its MIDI identity (soundingNote), so the chain's
                        // eventual note-off still matches its note-on.
                        if (note != soundingPitch && numLegato < (int) legato.size())
                            legato[(size_t) numLegato++] = { soundingNote, note - soundingPitch, tieIsSlide };
                        soundingPitch = note;
                    }
                    else
                    {
                        // Same note as the one still sounding? Re-strike it: note-off first, or the
                        // synth would see two note-ons for one key and the note-off of the first
                        // would kill the second. A DIFFERENT note is left alone — legato overlap.
                        if (soundingNote == note)
                        {
                            out.addEvent (juce::MidiMessage::noteOff (channel, soundingNote), i);
                            soundingNote = -1;
                        }
                        out.addEvent (juce::MidiMessage::noteOn (channel, note,
                                                                 (juce::uint8) (accent[(size_t) s] ? 127 : 100)), i);
                        const int prev = soundingNote;   // still sounding => legato tail to close
                        soundingNote  = note;
                        soundingPitch = note;
                        if (prev >= 0)
                            out.addEvent (juce::MidiMessage::noteOff (channel, prev), i);
                    }
                    // The step's own gate decides how the note leaves it: a percent schedules the
                    // note-off (scaled by the global GATE, so default 100 is bit-exact pre-15.7),
                    // TIE/SLIDE schedules nothing — the note holds into the next boundary, where
                    // the takeover above (or a rest below) resolves it.
                    if (sg >= kGateTie)
                    {
                        gateCountdown = -1;
                        tiePending  = true;
                        tieIsSlide  = (sg == kGateSlide);
                    }
                    else
                    {
                        gateCountdown = juce::jmax (1, (int) (interval * g * (sg / 100.0)));
                        tiePending = false;
                    }
                }
                else
                {
                    if (soundingNote >= 0)
                    {
                        // A rest ends the note that is running, even mid-gate — and it ends a tie
                        // chain. That IS a hole in a legato pattern, which is what a rest is for.
                        out.addEvent (juce::MidiMessage::noteOff (channel, soundingNote), i);
                        soundingNote = -1;
                        gateCountdown = -1;
                    }
                    tiePending = false;
                }
                stepIndex = (stepIndex + 1) % steps;
            }

            if (++sampleCounter >= interval)
                sampleCounter = 0;
        }
    }

    // 15.7: the TIE/SLIDE takeovers this block produced — the processor applies each to the
    // voice playing `note` (SynthVoice::slideTo) right after this block's events are in.
    int numLegatoEvents() const noexcept { return numLegato; }
    const Legato& legatoEvent (int i) const noexcept { return legato[(size_t) i]; }

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
    int    soundingNote = -1; // the MIDI identity (note-on/off pair) — UNCHANGED across a tie chain
    int    soundingPitch = -1;// what it currently sounds like (15.7: moved by TIE/SLIDE takeovers)
    int    gateCountdown = -1;
    bool   tiePending = false;   // 15.7: the sounding step was TIE/SLIDE — next boundary takes over
    bool   tieIsSlide = false;   //       ...and the takeover glides instead of stepping
    std::array<Legato, 8> legato {};   // takeovers this block (fixed size — RT, one per boundary)
    int    numLegato = 0;
    int    startDelay = 0;   // samples still to wait before the first step (quantised entry, 16.1)
};
