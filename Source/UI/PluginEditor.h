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

    // Make the FREQ knob show the actually-played frequency: display = base × ratio.
    // The underlying parameter stays the C4 base value (turning the knob writes
    // display ÷ ratio back as the new base).
    void setPlayedRatio(double ratio);

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::String freqId;
    double playedRatio = 1.0;

    juce::ToggleButton enableBtn;
    juce::Label title;
    juce::ComboBox waveSelector;
    SynthySlider freqKnob, ampKnob, uniVoicesKnob, uniDetuneKnob;
    juce::Label freqLabel, ampLabel, uniVoicesLabel, uniDetuneLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ampAttach, uniVoicesAttach, uniDetuneAttach;

    void resized() override;
};

class EffectPanel : public juce::Component
{
public:
    EffectPanel(juce::AudioProcessorValueTreeState& apvts,
                const juce::String& name, juce::Colour color,
                const juce::String& onParam,
                const juce::StringArray& knobParams,
                const juce::StringArray& knobLabels,
                const juce::String& triggerText = {});

    // Set by the owner; called when the optional trigger button (e.g. "PLUCK") is clicked.
    std::function<void()> onTrigger;

private:
    juce::Label title;
    juce::ToggleButton enableBtn;
    juce::TextButton triggerButton;     // optional (only shown if triggerText given)
    bool hasTrigger = false;
    juce::OwnedArray<SynthySlider> knobs;
    juce::OwnedArray<juce::Label> labels;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> btnAttach;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> knobAttachments;

    void resized() override;
};

class SynthyEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit SynthyEditor(SynthyProcessor&);
    ~SynthyEditor() override { stopTimer(); setLookAndFeel(nullptr); }

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void timerCallback() override;

private:
    SynthyProcessor& processor;
    SynthyLookAndFeel lnf;

    OscillatorPanel osc1, osc2, osc3;

    // Mix mode (placed inline between OSC 1 and OSC 2; "+" sits before OSC 3)
    juce::Label mixModeTitle;
    juce::Label mixModeHint;   // "OSC 1 <-> 2" sub-caption under the selector
    juce::Label mixPlusLabel;  // "+" between OSC 2 and OSC 3
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

    // Wavefolder (reuses EffectPanel: ON toggle + DRIVE/SYM/MIX knobs)
    EffectPanel wavefoldPanel;

    // Bitcrusher (reuses EffectPanel: ON toggle + BITS/RATE/MIX knobs)
    EffectPanel bitcrushPanel;

    // Karplus-Strong (reuses EffectPanel: ON toggle + knobs)
    EffectPanel karplusPanel;

    // Noise
    juce::Label noiseTitle;
    juce::ComboBox noiseTypeSelector;
    SynthySlider noiseAmpKnob;
    juce::Label noiseAmpLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> noiseTypeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> noiseAmpAttach;

    // Sub oscillator (inline: ON + wave/octave combos + LEVEL knob)
    juce::Label subTitle;
    juce::ToggleButton subEnableBtn;
    juce::ComboBox subWaveSelector, subOctaveSelector;
    SynthySlider subLevelKnob;
    juce::Label subLevelLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> subEnableAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> subWaveAttach, subOctaveAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> subLevelAttach;

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

    // Preset save/load (shared .synthy JSON format) + randomize
    juce::TextButton saveBtn { "SAVE" }, loadBtn { "LOAD" }, randomBtn { "RANDOM" };
    std::unique_ptr<juce::FileChooser> presetChooser;

    // Oscilloscope + Spectrum
    std::unique_ptr<WaveformDisplay> waveformDisplay;
    std::unique_ptr<SpectrumDisplay> spectrumDisplay;

    // On-screen keyboard (auto-play drone is handled automatically by the processor)
    std::unique_ptr<juce::MidiKeyboardComponent> keyboard;
    int kbBaseOctave = 4;   // computer-keyboard octave (z / x shift it)

    juce::Slider& setupKnob(juce::Slider& knob, juce::Label& label, const juce::String& text);

    // Layout bounds for paint()
    juce::Rectangle<int> g_titleBounds, adsrBounds, lfoBounds, filterBounds, distBounds, noiseBounds, wtBounds, subBounds;
    // Zone separator headers (TONERZEUGER / MODULATION / SOUNDVERARBEITUNG)
    juce::Rectangle<int> genHeaderBounds, modHeaderBounds, procHeaderBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthyEditor)
};
