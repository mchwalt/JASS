#pragma once
#include <JuceHeader.h>
#include "Audio/SynthVoice.h"
#include "Audio/SynthSound.h"
#include "Audio/Parameters.h"
#include "DSP/WaveformCapture.h"
#include "DSP/StereoWidth.h"

class SynthyProcessor : public juce::AudioProcessor,
                        private juce::Timer,
                        private juce::ValueTree::Listener,
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

    const juce::String getName() const override { return "Synthy"; }
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
    float getLfoDisplayValue() const { return lfoDisplayValue.load(); }

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
    WaveformCapture waveformCapture { 512 };
    StereoWidth stereoWidth;   // final pseudo-stereo stage (mono engine -> stereo)
    LFO uiLfo;                 // display-only LFO mirroring the patch LFO (for the rings)
    std::atomic<float> lfoDisplayValue { 0.0f };
    bool autoNoteOn = false;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthyProcessor)
};
