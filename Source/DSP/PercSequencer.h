#pragma once
#include <JuceHeader.h>
#include <array>
#include "SamplePlayer.h"
#include "SampleBank.h"

// PERC (Story 16.1) — four percussion tracks on a step grid, rendered straight into the master bus.
//
// This is NOT a second note sequencer, and the difference is the whole design. JASS is monotimbral:
// SynthVoice::startNote starts every enabled generator for every note, and the effect chain (filter,
// distortion, delay, reverb, …) lives PER VOICE. A drum note emitted as MIDI would therefore come
// out as sawtooth-plus-sample through the bass's resonant lowpass. So PERC never enters a voice: it
// owns four SamplePlayers of its own and mixes them into the buffer after the synth has rendered.
// "Dry" is the construction, not a setting — there is no chain here to be dry about.
//
// The maintainer's framing (2026-08-10) is the one to keep in mind while reading this: it is a
// SECOND SAMPLER INSTANCE living at processor level, which today happens to be pointed at a drum
// kit. That is why the playback is the stock SamplePlayer rather than bespoke drum code — sets,
// velocity layers, SFZ parsing and background loading all come along for free, and layer B can
// later carry any sampled instrument.
//
// Timing is sample-accurate off a step interval the processor resolves per block (tempo-synced via
// SyncDivision, or the free-running RATE). Allocation-free on the audio thread: fixed arrays, and
// the players hold their own buffers.
class PercSequencer
{
public:
    static constexpr int kLanes    = 4;
    // 16.3: pattern capacity, same reasoning as StepSequencer — a generous fixed cap (the APVTS
    // parameter list cannot grow at runtime), shown one 48-step page at a time in the UI.
    // 16 pages since 2026-09-02, in step with the note sequencer: drums follow the song.
    static constexpr int kMaxSteps  = 768;   // was 192 (16.3), 48 (16.2), 32 before that
    static constexpr int kPageSteps = 48;    // one UI page of the grid (16.2 geometry)

    bool   enabled = false;
    double stepSeconds = 0.125;                 // one step; resolved per block by the processor
    int    length = 16;                         // 1..32 steps before the pattern repeats

    std::array<std::array<bool, kMaxSteps>, kLanes> on {};    // the grid
    std::array<int, kLanes>    note  { 36, 38, 42, 46 };      // which instrument of the kit a lane fires
    std::array<double, kLanes> level { 0.8, 0.8, 0.8, 0.8 };
    std::array<double, kLanes> pan   { 0.0, 0.0, 0.0, 0.0 };  // -1 = left, +1 = right
    // The kit's master, 0..1 like every level knob in the rack. The headroom lives in kAmpScale
    // rather than in the knob's range: a sampled kit peaks well below a synth's raw output, so
    // full scale here is +12 dB over the recording and unity sits at a quarter turn.
    static constexpr double kAmpScale = 4.0;
    double amp = 0.5;

    void prepare (double sr)
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        for (auto& p : lanes)
        {
            p.setSampleRate (sampleRate);
            p.setEnabled (true);
            p.setMode (SamplePlayer::Mode::OneShot);   // a drum hit is a one-shot, always
            p.setRegion (0.0, 1.0);
            p.setSpeed (1.0);
            p.setStretchMode (false);                  // tape mode: a kit is played at its own pitch
            p.setReleaseFallback (0.0);                // let the sample ring out; nothing gates it
        }
        reset();
    }

    void reset()
    {
        sampleCounter = 0;
        stepIndex = 0;
        litStep   = -1;
    }

    // The kit. Handed in per block like the SAMPLER's set, so switching kits (or one arriving from
    // the background loader) simply takes effect — SamplePlayer::setSource is a no-op when unchanged.
    void setSource (const SampleSet* s)
    {
        for (auto& p : lanes)
            p.setSource (s);
        haveSet = (s != nullptr);
    }

    // Samples from the START OF THE NEXT BLOCK until this pattern wraps to step 0. The STEP SEQ uses
    // it to enter on the downbeat instead of wherever the key happened to fall (Story 16.1 AC6):
    // the drums are the clock everything else joins, which is the one thing a beat cannot negotiate.
    int samplesToPatternStart() const
    {
        const int steps    = juce::jlimit (1, kMaxSteps, length);
        const int interval = stepInterval();
        const int toBoundary   = (sampleCounter == 0) ? 0 : interval - sampleCounter;
        const int stepsToWrap  = (steps - stepIndex % steps) % steps;
        return toBoundary + stepsToWrap * interval;
    }

    int currentStep() const noexcept { return stepIndex; }
    // What is sounding NOW. `stepIndex` has already been advanced to the next step by the time the
    // grid paints, so using it put the playhead one cell ahead of the beat you hear (fixed
    // 2026-08-11, when STEP SEQ got a playhead of its own and the two had to agree).
    int playingStep() const noexcept { return litStep; }

    // Advance the clock and ADD the four tracks into the buffer. Called after the synth has
    // rendered and before the compressor, so PERC gets the master glue and the MASTER level and
    // nothing else.
    // Sound one lane once, now — the grid plays a step as you place it (maintainer 2026-08-11:
    // "links klick setzt und spielt die Note"). Called from the audio thread via an atomic the
    // processor consumes, so no lock and no allocation.
    void triggerLane (int lane)
    {
        if (! haveSet || lane < 0 || lane >= kLanes)
            return;
        const int n = juce::jlimit (0, 127, note[(size_t) lane]);
        lanes[(size_t) lane].setLevel (level[(size_t) lane] * amp * kAmpScale);
        lanes[(size_t) lane].trigger (ratioFor (n), n, 127);
        chokeFrom (lane);
    }

    // Choke groups across the four tracks (Story 12.7). A lane retriggering itself already cuts its
    // own tail inside SamplePlayer, so what this adds is the case a kit actually needs: the CLOSED
    // hat on one lane silencing the OPEN hat ringing on another. The kit declares it — `off_by=N`
    // on the closed hat, `group=N` on the open one — so nothing here knows anything about hi-hats.
    void chokeFrom (int lane)
    {
        const auto* z = lanes[(size_t) lane].currentZone();
        if (z == nullptr || z->offBy == 0)
            return;
        for (int o = 0; o < kLanes; ++o)
        {
            if (o == lane)
                continue;   // never choke the hit that just started
            if (const auto* oz = lanes[(size_t) o].currentZone(); oz != nullptr && oz->group == z->offBy)
                lanes[(size_t) o].chokeOff();
        }
    }

    void processBlock (juce::AudioBuffer<float>& buffer, int numSamples)
    {
        if (! haveSet)
            return;   // no kit: nothing to render, and nothing to preview either

        const int steps    = juce::jlimit (1, kMaxSteps, length);
        const int interval = stepInterval();
        float* outL = buffer.getWritePointer (0);
        float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            // The clock only runs while the module is on (its switch IS the transport). The render
            // below runs either way, so a hit previewed from the grid — or the tail of one still
            // ringing when the module was switched off — is still heard.
            if (enabled && sampleCounter == 0)
            {
                const int s = stepIndex % steps;
                litStep = s;
                for (int l = 0; l < kLanes; ++l)
                    if (on[(size_t) l][(size_t) s])
                    {
                        const int n = juce::jlimit (0, 127, note[(size_t) l]);
                        lanes[(size_t) l].setLevel (level[(size_t) l] * amp * kAmpScale);
                        // Velocity 127, not the sequencer's 100: an SFZ tracks velocity per the spec
                        // default, so 100 costs 4.2 dB for nothing (the `Drum Pattern` preset had to
                        // compensate exactly that). Balance belongs on the LEVEL knob, where one can
                        // see it.
                        lanes[(size_t) l].trigger (ratioFor (n), n, 127);
                        chokeFrom (l);   // 12.7: a closed hat silences the open one on its own lane
                    }
                stepIndex = (stepIndex + 1) % steps;
            }
            if (++sampleCounter >= interval)
                sampleCounter = 0;

            for (int l = 0; l < kLanes; ++l)
            {
                const auto o = lanes[(size_t) l].nextSample();
                // Constant power (sin/cos), so moving a lane off centre does not change how loud
                // it is — the same law the voice panner uses. PERC has to do this itself: it never
                // reaches the STEREO stage.
                const double a = (juce::jlimit (-1.0, 1.0, pan[(size_t) l]) + 1.0) * 0.25 * juce::MathConstants<double>::pi;
                outL[i] += o.l * (float) std::cos (a);
                if (outR != nullptr) outR[i] += o.r * (float) std::sin (a);
            }
        }
    }

private:
    int stepInterval() const noexcept
    {
        return juce::jmax (1, (int) (sampleRate * juce::jmax (0.01, stepSeconds)));
    }

    // Same transposition rule as a voice: the zone's own root then plays it at its natural speed,
    // so a chromatically mapped kit sounds as recorded.
    static double ratioFor (int midiNote)
    {
        return juce::MidiMessage::getMidiNoteInHertz (midiNote)
             / juce::MidiMessage::getMidiNoteInHertz (60);
    }

    std::array<SamplePlayer, kLanes> lanes;
    double sampleRate = 44100.0;
    int    sampleCounter = 0;
    int    stepIndex = 0;
    int    litStep = -1;   // step sounding right now (playhead); stepIndex is already the next one
    bool   haveSet = false;
};
