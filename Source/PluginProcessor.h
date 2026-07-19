#pragma once
#include <JuceHeader.h>
#include "Audio/SynthVoice.h"
#include "Audio/SynthSound.h"
#include "Audio/Parameters.h"
#include "DSP/WaveformCapture.h"
#include "DSP/StereoWidth.h"
#include "DSP/Compressor.h"
#include "DSP/Arpeggiator.h"
#include <vector>
#include <map>

class SynthyProcessor : public juce::AudioProcessor,
                        private juce::Timer,
                        private juce::ValueTree::Listener,
                        private juce::AudioProcessorValueTreeState::Listener,
                        private juce::MidiKeyboardState::Listener
{
public:
    SynthyProcessor();
    ~SynthyProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "JASS"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    juce::MidiKeyboardState& getKeyboardState() { return keyboardState; }
    WaveformCapture& getWaveformCapture() { return waveformCapture; }

    // Re-pluck the Karplus string on every voice (PLUCK button / spacebar). The flag
    // is consumed on the audio thread in processBlock — RT-safe (no direct voice touch
    // from the message thread).
    void pluckString()
    {
        pluckRequested = true;
        // A pluck only re-excites ACTIVE voices; after the keyboard was played the auto-play
        // drone is off and no voice renders, so the pluck would be silent. If nothing is held,
        // re-arm the drone so a voice is active and the pluck is actually heard.
        if (heldNotesLo.load() == 0 && heldNotesHi.load() == 0)
            autoPlayEnabled = true;
    }

    // Randomize all parameters (with guards so the result stays audible).
    void randomize();

    // Reset everything to defaults, then enable only OSC 1 (auto-play drone).
    void resetToDefault();

    // Pitch ratio of the note currently being played (relative to C4 = note 60);
    // 1.0 when only the drone or nothing sounds. Used by the editor so the OSC
    // FREQ knobs can display the actually-played frequency.
    float getCurrentNoteRatio() const { return currentNoteRatio.load(); }

    // Current LFO oscillation value (-1..+1, already scaled by depth) for the
    // editor's live modulation rings. Driven by a dedicated display LFO that
    // mirrors the patch's LFO params and runs even when no note sounds.
    float getLfoDisplayValue(int i) const { return lfoDisplayValues[i].load(); }   // per-LFO (for the rings)

    // Name of the currently loaded/active preset (shown in the header). It is
    // persisted into the LiveState file so it survives a restart. Touched only
    // on the message thread (editor + LiveState save).
    juce::String getCurrentPresetName() const { return currentPresetName; }
    void setCurrentPresetName(const juce::String& n) { currentPresetName = n; }

    // "Modified since last load/save?" → the header shows "Current State" instead
    // of the (now stale) preset name. Determined by comparing the live parameter
    // values against a snapshot taken at the last clean point (load/save/random/
    // reset). Done by value-compare rather than a change-listener because APVTS
    // delivers value-tree change callbacks asynchronously, which would race the
    // "clear after load" and leave the flag permanently set.
    bool isPresetModified() const;
    void markPresetClean();                       // snapshot current values as the clean baseline
    void restoreModifiedState(bool modified);     // on LiveState load: clean baseline, or force "modified"

private:
    juce::Synthesiser synth;
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    // Snapshot length = the LONGEST scope window (100 ms) at the highest sample rate we
    // expect (96 kHz): 0.100 s × 96000 = 9600. The scope shows only the first N samples of
    // this for the selected window, so a too-small buffer would clamp the long windows to
    // identical traces. Sized once here (never reallocated) so the GUI's updateSnapshot never
    // races a resize. (Spectrum reads at most fftSize=1024 of it, zero-padding when shorter.)
    WaveformCapture waveformCapture { 9600 };
    StereoWidth stereoWidth;   // final pseudo-stereo stage (mono engine -> stereo)
    Compressor  compressor;    // master-bus compressor (runs on the summed mix, before width)
    LFO uiLfos[kNumLFOs];      // display-only LFOs mirroring each patch LFO (for the rings)
    std::atomic<float> lfoDisplayValues[kNumLFOs] {};
    Arpeggiator arp;
    std::vector<int> arpHeldScratch;   // reused per block (no RT realloc)

    // Poly-glide (portamento): the processor assigns each newly-started note a predecessor
    // pitch to glide FROM (pitch-sorted against the previous chord) and shares it with all
    // voices via glideInfo. Held-note tracking excludes the auto-play drone.
    GlideInfo glideInfo;
    std::vector<int> glideHeld;        // notes currently held down
    std::vector<int> glideLastChord;   // last non-empty held set (glide source after a gap)
    std::vector<int> glideNewNotes, glideOffNotes;   // per-block scratch (no RT realloc)
    int monoSounding = -1;             // Mono glide: the note currently sounding (last-note priority)
    bool autoNoteOn = false;
    std::atomic<bool> pluckRequested { false };   // set by pluckString(), consumed in processBlock

    // Auto-play drone is automatic now: ON until the user plays a key, back ON
    // when a sound generator is (re)activated. No user-facing parameter. The drone
    // lives on its own MIDI channel so it never collides with played notes.
    static constexpr int kDroneChannel = 16;
    static constexpr int kDroneNote = 60;        // C4 → transpose ratio 1.0
    std::atomic<bool> autoPlayEnabled { true };
    unsigned prevSourcesMask = 0;                // for rising-edge "generator enabled" detection

    std::atomic<float> currentNoteRatio { 1.0f };           // played note vs C4 (FREQ display); 1.0 = base
    std::atomic<std::uint64_t> heldNotesLo { 0 }, heldNotesHi { 0 };  // bitset of held user notes

    void handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNote, float) override;
    void handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNote, float) override;

    // Shared live-state persistence (see PresetIO::liveStateFile)
    juce::String currentPresetName { "Init" };   // restored from LiveState on start
    std::atomic<bool> liveDirty { false };
    std::vector<float> cleanSnapshot;             // param values at last load/save (empty = "modified")
    void timerCallback() override;
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override { liveDirty = true; }
    void saveLiveState();

    // Epic 5: keep the two MIX MODE source selectors distinct (a==b would be a no-op / needs
    // self-FM which is a separate future feature). When one is set equal to the other, bump the
    // other to a free OSC. Registered for mixSrcA/mixSrcB only.
    void parameterChanged(const juce::String& paramId, float newValue) override;
    std::atomic<bool> fixingMixSrc { false };   // reentrancy guard

    // Keep the matrix's SOURCE and TARGET modules in step with the routing: an active slot
    // (DEST != Off) needs both its source module (LFO/Envelope) AND its target module (e.g. the
    // FILTER for Cutoff/Resonance, FORMANT for Vowel, …) enabled, else the modulation is
    // inaudible. Turn each claimed module ON, and turn it back OFF when the LAST slot using it
    // drops — but only if WE auto-enabled it (it was off before and isn't wired elsewhere). A
    // module on for any other reason (user toggle, ADSR default) is never auto-disabled.
    // Targets Pitch/Amplitude are global (no module). Re-evaluated on every slot SRC/DEST change.
    void updateMatrixModuleEnables();
    std::map<juce::String, bool> matrixAutoEnabled;   // enable-param id -> true if auto-enable turned it on

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthyProcessor)
};
