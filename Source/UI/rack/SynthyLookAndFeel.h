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
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override;
};
