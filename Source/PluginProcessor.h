#pragma once
#include <JuceHeader.h>
#include "Audio/SynthVoice.h"
#include "Audio/SynthSound.h"
#include "Audio/Parameters.h"
#include "DSP/WaveformCapture.h"

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

    // Name of the currently loaded/active preset (shown in the header). It is
    // persisted into the LiveState file so it survives a restart. Touched only
    // on the message thread (editor + LiveState save).
    juce::String getCurrentPresetName() const { return currentPresetName; }
    void setCurrentPresetName(const juce::String& n) { currentPresetName = n; }

private:
    juce::Synthesiser synth;
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    WaveformCapture waveformCapture { 512 };
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
    void timerCallback() override;
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override { liveDirty = true; }
    void saveLiveState();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthyProcessor)
};
