#pragma once
#include <JuceHeader.h>
#include "../SynthySlider.h"

// The single shared LookAndFeel for the whole JASS editor (AD-7). It is set ONCE
// by the Rack so every ModuleFrame and control beneath it inherits the same look;
// no module installs its own. Moved out of PluginEditor into the rack framework so
// the framework owns the shared look. Kept as a top-level Synthy* class name
// (naming dualism — the product is JASS, the classes stay Synthy*).
class SynthyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SynthyLookAndFeel();

    // ONE UI text size for the whole rack so captions, knob value boxes and combo
    // boxes read uniformly (the module title is the only larger, bold exception).
    static constexpr float kUiFontSize = 13.0f;

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override;

    // Force the uniform size on every label (incl. each slider's value box),
    // combo box, and its dropdown — overriding the LookAndFeel_V4 defaults.
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
};
