#pragma once
#include <JuceHeader.h>

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
        if (e.mods.isShiftDown())
        {
            double step = getFineStep();
            double delta = wheel.deltaY > 0 ? step : -step;
            setValue(getValue() + delta, juce::sendNotificationSync);
            return;
        }
        Slider::mouseWheelMove(e, wheel);
    }

private:
    double getFineStep() const
    {
        auto suffix = getTextValueSuffix();
        if (suffix.containsIgnoreCase("Hz"))
            return 1.0;

        auto range = getRange();
        return (range.getEnd() - range.getStart()) / 100.0;
    }
};
