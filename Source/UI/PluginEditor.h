#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "SynthySlider.h"
#include "WaveformDisplay.h"
#include "SpectrumDisplay.h"

class SynthyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SynthyLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override;
};

class OscillatorPanel : public juce::Component
{
public:
    OscillatorPanel(juce::AudioProcessorValueTreeState& apvts, int oscIndex,
                    juce::Colour color);

private:
    juce::ToggleButton enableBtn;
    juce::Label title;
    juce::ComboBox waveSelector;
    SynthySlider freqKnob, ampKnob, uniVoicesKnob, uniDetuneKnob;
    juce::Label freqLabel, ampLabel, uniVoicesLabel, uniDetuneLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttach, ampAttach, uniVoicesAttach, uniDetuneAttach;

    void resized() override;
};

class EffectPanel : public juce::Component
{
public:
    EffectPanel(juce::AudioProcessorValueTreeState& apvts,
                const juce::String& name, juce::Colour color,
                const juce::String& onParam,
                const juce::StringArray& knobParams,
                const juce::StringArray& knobLabels);

private:
    juce::Label title;
    juce::ToggleButton enableBtn;
    juce::OwnedArray<SynthySlider> knobs;
    juce::OwnedArray<juce::Label> labels;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> btnAttach;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> knobAttachments;

    void resized() override;
};

class SynthyEditor : public juce::AudioProcessorEditor
{
public:
    explicit SynthyEditor(SynthyProcessor&);
    ~SynthyEditor() override { setLookAndFeel(nullptr); }

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    SynthyProcessor& processor;
    SynthyLookAndFeel lnf;

    OscillatorPanel osc1, osc2, osc3;

    // Mix mode
    juce::Label mixModeTitle;
    juce::ComboBox mixModeSelector;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mixModeAttach;

    // ADSR
    juce::Label adsrTitle;
    SynthySlider attackKnob, decayKnob, sustainKnob, releaseKnob;
    juce::Label atkLabel, decLabel, susLabel, relLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        atkAttach, decAttach, susAttach, relAttach;

    // Filter
    juce::Label filterTitle;
    juce::ComboBox filterType;
    SynthySlider cutoffKnob, resoKnob;
    juce::Label cutLabel, resoLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filterTypeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutAttach, resoAttach;

    // LFO
    juce::Label lfoTitle;
    juce::ComboBox lfoWaveSelector, lfoTargetSelector;
    SynthySlider lfoRateKnob, lfoDepthKnob;
    juce::Label lfoRateLabel, lfoDepthLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoWaveAttach, lfoTargetAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoRateAttach, lfoDepthAttach;

    // Distortion (inline: type ComboBox + DRIVE/MIX knobs)
    juce::Label distTitle;
    juce::ComboBox distTypeSelector;
    SynthySlider distDriveKnob, distMixKnob;
    juce::Label distDriveLabel, distMixLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> distTypeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> distDriveAttach, distMixAttach;

    // Effects
    EffectPanel delayPanel, chorusPanel, reverbPanel;

    // Karplus-Strong (reuses EffectPanel: ON toggle + knobs)
    EffectPanel karplusPanel;

    // Noise
    juce::Label noiseTitle;
    juce::ComboBox noiseTypeSelector;
    SynthySlider noiseAmpKnob;
    juce::Label noiseAmpLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> noiseTypeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> noiseAmpAttach;

    // Wavetable
    juce::Label wtTitle;
    juce::ToggleButton wtEnableBtn;
    juce::ComboBox wtBankSelector;          // manual sync (dynamic items via WAV load)
    juce::TextButton wtLoadBtn { "LOAD WAV" };
    SynthySlider wtPositionKnob, wtFreqKnob, wtAmpKnob, wtVoicesKnob, wtDetuneKnob;
    juce::Label wtPositionLabel, wtFreqLabel, wtAmpLabel, wtVoicesLabel, wtDetuneLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> wtEnableAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> wtPositionAttach, wtFreqAttach, wtAmpAttach, wtVoicesAttach, wtDetuneAttach;
    std::unique_ptr<juce::FileChooser> wtFileChooser;
    void refreshBankSelector();             // repopulate combo from the shared store

    // Master
    SynthySlider masterKnob;
    juce::Label masterLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAttach;

    // Preset save/load (shared .synthy JSON format)
    juce::TextButton saveBtn { "SAVE" }, loadBtn { "LOAD" };
    std::unique_ptr<juce::FileChooser> presetChooser;

    // Oscilloscope + Spectrum
    std::unique_ptr<WaveformDisplay> waveformDisplay;
    std::unique_ptr<SpectrumDisplay> spectrumDisplay;

    juce::Slider& setupKnob(juce::Slider& knob, juce::Label& label, const juce::String& text);

    // Layout bounds for paint()
    juce::Rectangle<int> g_titleBounds, adsrBounds, lfoBounds, filterBounds, distBounds, noiseBounds, wtBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthyEditor)
};
