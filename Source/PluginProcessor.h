#pragma once
#include <JuceHeader.h>
#include "Audio/SynthVoice.h"
#include "Audio/SynthSound.h"
#include "Audio/Parameters.h"
#include "DSP/WaveformCapture.h"

class SynthyProcessor : public juce::AudioProcessor,
                        private juce::Timer,
                        private juce::ValueTree::Listener
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

private:
    juce::Synthesiser synth;
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    WaveformCapture waveformCapture { 512 };
    bool autoNoteOn = false;

    // Shared live-state persistence (see PresetIO::liveStateFile)
    std::atomic<bool> liveDirty { false };
    void timerCallback() override;
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override { liveDirty = true; }
    void saveLiveState();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthyProcessor)
};
