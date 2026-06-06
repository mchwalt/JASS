#pragma once
#include <JuceHeader.h>

// Every module uses ONE knob diameter (Small) so the whole UI is consistent
// and the compact half-height modules fit. The LookAndFeel caps each knob's
// drawn size to this; the larger values are kept only for reference.
namespace KnobSize
{
    constexpr int Large  = 74;
    constexpr int Medium = 56;
    constexpr int Small  = 46;  // the single standard size for all modules
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

    // Live modulation amount (-1..+1) for the animated ring the LookAndFeel draws
    // around the knob (Vital-style). 0 = no ring. Set by the editor's timer from
    // the current LFO value; repaints only on a meaningful change.
    void setModAmount(float a)
    {
        if (std::abs(a - modAmount) > 0.003f) { modAmount = a; repaint(); }
    }
    float getModAmount() const { return modAmount; }

    // Overrides the per-notch fine step for the mouse wheel. Use when the
    // parameter's automation granularity is much finer than is useful for the
    // wheel (e.g. ADSR times have a 1 ms interval but want ~10 ms wheel steps).
    void setWheelStep(double s) { wheelStep = s; }

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
        double interval = wheelStep > 0.0 ? wheelStep : getInterval();
        double range    = getMaximum() - getMinimum();
        if (interval <= 0.0 || range <= 0.0) { juce::Slider::mouseWheelMove(e, wheel); return; }

        double dir = wheel.deltaY >= 0.0f ? 1.0 : -1.0;

        // Small discrete ranges (e.g. unison voices 1..7): one step per notch.
        if (range / interval <= 24.0)
        {
            setValue(getValue() + dir * interval, juce::sendNotificationSync);
            return;
        }

        // Fine (Shift) = one granularity step; coarse = ten steps, but at least
        // ~range/400 so wide knobs move usefully:
        //   0..1   -> fine 0.01, coarse 0.1
        //   0..100 -> fine 1,    coarse 10
        //   0..10000 Hz -> fine 1 Hz, coarse ~25 Hz
        double fine   = interval;
        double coarse = juce::jmax(interval * 10.0, range / 400.0);
        double step   = e.mods.isShiftDown() ? fine : coarse;
        setValue(getValue() + dir * step, juce::sendNotificationSync);
    }

private:
    int knobDiameter = KnobSize::Small;   // single standard size for all modules
    double wheelStep = 0.0;   // 0 = use the parameter's own interval
    float modAmount = 0.0f;   // live LFO modulation for the ring (-1..+1)
};
