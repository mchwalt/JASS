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

    // Live LFO modulation rings (set by the editor timer; 0 = no ring).
    void setFreqMod(float a) { freqKnob.setModAmount(a); }
    void setAmpMod(float a)  { ampKnob.setModAmount(a); }

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::String freqId;
    double playedRatio = 1.0;

    juce::ToggleButton enableBtn;
    juce::Label title;
    juce::TextButton resetBtn;       // ↺ — restore all of this OSC's params to default
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
                const juce::String& triggerText = {},
                bool withReset = false);

    // Set by the owner; called when the optional trigger button (e.g. "PLUCK") is clicked.
    std::function<void()> onTrigger;

private:
    juce::Label title;
    juce::ToggleButton enableBtn;
    juce::TextButton triggerButton;     // optional (only shown if triggerText given)
    juce::TextButton resetBtn;          // optional ↺ (reset all params to default)
    bool hasTrigger = false;
    bool hasReset = false;
    juce::OwnedArray<SynthySlider> knobs;
    juce::OwnedArray<juce::Label> labels;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> btnAttach;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> knobAttachments;

    void resized() override;
};

// A compact ADSR curve preview: attack ramp → decay to the sustain level →
// sustain hold → release tail. The A/D/R segment widths are drawn proportional
// to their durations (always filling the strip), the sustain segment is a fixed
// hold, and the sustain knob sets the hold height. Self-contained: it polls the
// four envelope params and only repaints when one of them moves.
class EnvelopeDisplay : public juce::Component, private juce::Timer
{
public:
    EnvelopeDisplay(juce::AudioProcessorValueTreeState& apvts, juce::Colour colour)
        : col(colour)
    {
        pA = apvts.getRawParameterValue("attack");
        pD = apvts.getRawParameterValue("decay");
        pS = apvts.getRawParameterValue("sustain");
        pR = apvts.getRawParameterValue("release");
        startTimerHz(20);
    }
    ~EnvelopeDisplay() override { stopTimer(); }

    void paint(juce::Graphics&) override;

private:
    void timerCallback() override
    {
        const float a = pA->load(), d = pD->load(), s = pS->load(), r = pR->load();
        if (a != lA || d != lD || s != lS || r != lR)
        {
            lA = a; lD = d; lS = s; lR = r;
            repaint();
        }
    }

    std::atomic<float> *pA = nullptr, *pD = nullptr, *pS = nullptr, *pR = nullptr;
    float lA = -1, lD = -1, lS = -1, lR = -1;
    juce::Colour col;
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
    EnvelopeDisplay adsrEnvDisplay;
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

    // Arpeggiator (MODULATION zone)
    juce::Label arpTitle;
    juce::ToggleButton arpEnableBtn;
    juce::ComboBox arpModeSelector;
    SynthySlider arpRateKnob, arpOctavesKnob, arpGateKnob;
    juce::Label arpRateLabel, arpOctavesLabel, arpGateLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> arpEnableAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> arpModeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> arpRateAttach, arpOctavesAttach, arpGateAttach;

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

    // Per-generator "↺" reset buttons for the inline sources (the OSC and
    // Karplus panels carry their own; these cover Noise / Sub / Wavetable).
    juce::TextButton noiseResetBtn, subResetBtn, wtResetBtn;

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

    // Stereo width (header, inline next to Master). Built inline rather than as an
    // EffectPanel so its WIDTH/TIME knobs get the full header height and render at
    // the same Medium size as Master (the EffectPanel's title row squashed them).
    juce::Label stereoTitle;
    juce::ToggleButton stereoOnBtn;
    SynthySlider stereoWidthKnob, stereoTimeKnob;
    juce::Label stereoWidthLabel, stereoTimeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> stereoOnAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> stereoWidthAttach, stereoTimeAttach;

    // Preset save/load (shared .synthy JSON format) + randomize + reset
    juce::TextButton saveBtn { "SAVE" }, loadBtn { "LOAD" }, randomBtn { "RANDOM" }, resetBtn { "RESET" };
    std::unique_ptr<juce::FileChooser> presetChooser;
    juce::Label presetNameLabel;                 // shows the currently loaded preset
    juce::String shownLabel;                     // last text pushed to the label (change-detect)
    void setPresetName(const juce::String& name);
    void updatePresetLabel();                    // composes "Preset: X" / "Current State"

    // Oscilloscope + Spectrum
    std::unique_ptr<WaveformDisplay> waveformDisplay;
    std::unique_ptr<SpectrumDisplay> spectrumDisplay;

    // On-screen keyboard (auto-play drone is handled automatically by the processor)
    std::unique_ptr<juce::MidiKeyboardComponent> keyboard;
    int kbBaseOctave = 4;   // computer-keyboard octave (z / x shift it)

    juce::Slider& setupKnob(juce::Slider& knob, juce::Label& label, const juce::String& text);

    // Per-module "↺" reset buttons for the inline modules (OSC and EffectPanel
    // components carry their own). initResetButton wires + styles + shows one in
    // a single line — the shared mechanism behind every inline module reset.
    void initResetButton(juce::TextButton& btn, juce::Colour colour,
                         juce::StringArray paramIds, std::function<void()> afterReset = {});
    juce::TextButton adsrResetBtn, lfoResetBtn, arpResetBtn,
                     filterResetBtn, distResetBtn, stereoResetBtn, masterResetBtn;

    // Layout bounds for paint()
    juce::Rectangle<int> g_titleBounds, adsrBounds, lfoBounds, filterBounds, distBounds, noiseBounds, wtBounds, subBounds, stereoBounds, arpBounds;
    // Zone separator headers (GENERATORS / MODULATION / PROCESSING)
    juce::Rectangle<int> genHeaderBounds, modHeaderBounds, procHeaderBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthyEditor)
};
