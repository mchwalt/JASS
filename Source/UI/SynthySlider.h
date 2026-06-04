#pragma once
#include <JuceHeader.h>

// The whole UI uses exactly three knob diameters so every panel looks
// consistent. The LookAndFeel caps each knob's drawn size to its category.
namespace KnobSize
{
    constexpr int Large  = 74;  // focal knobs (ADSR)
    constexpr int Medium = 56;  // the default for almost everything
    constexpr int Small  = 46;  // tight rows (Wavetable)
}

class SynthySlider : public juce::Slider
{
public:
    SynthySlider()
    {
        setSliderStyle(RotaryHorizontalVerticalDrag);
        setTextBoxStyle(TextBoxBelow, false, 55, 14);
        setVelocityBasedMode(true);
        setVelocityModeParameters(1.0, 1, 0.1, false);
    }

    // One of KnobSize::{Large,Medium,Small}; the LookAndFeel won't draw the
    // rotary larger than this (it stays centred in whatever cell it gets).
    void setKnobDiameter(int d) { knobDiameter = d; }
    int  getKnobDiameter() const { return knobDiameter; }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown())
        {
            // Enable text box editing on right-click
            setTextBoxStyle(TextBoxBelow, false, 55, 18);
            showTextBox();
            return;
        }
        Slider::mouseDown(e);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (e.mods.isShiftDown())
            setVelocityModeParameters(0.05, 1, 0.05, false);
        else
            setVelocityModeParameters(1.0, 1, 0.1, false);
        Slider::mouseDrag(e);
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        double range = getMaximum() - getMinimum();
        if (range <= 0.0) { juce::Slider::mouseWheelMove(e, wheel); return; }

        // Proportional steps so coarse + fine work the same on every knob
        // (the old code left coarse to the JUCE default, which barely moved
        //  wide-range knobs like Frequency).
        double interval = getInterval();
        double coarse = juce::jmax(range / 50.0,  interval);   // ~2% per notch
        double fine   = juce::jmax(range / 500.0, interval);   // finer with Shift
        double step   = e.mods.isShiftDown() ? fine : coarse;
        double dir    = wheel.deltaY >= 0.0f ? 1.0 : -1.0;
        setValue(getValue() + dir * step, juce::sendNotificationSync);
    }

private:
    int knobDiameter = KnobSize::Medium;
};
