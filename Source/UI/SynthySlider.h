#pragma once
#include <JuceHeader.h>

// Every module uses ONE knob diameter (Small) so the whole UI is consistent
// and the compact half-height modules fit. The LookAndFeel caps each knob's
// drawn size to this; the larger values are kept only for reference.
namespace KnobSize
{
    constexpr int Large  = 74;
    constexpr int Medium = 56;
    constexpr int Small  = 46;
    // THE size, the way kComboW is the one combo width: every rotary in the rack is 40 px, and a
    // module's row height is derived from it rather than the other way round. 40 is not a taste,
    // it is the measured ceiling — SAMPLER packs its row tightest and offers a 48 px cell, which
    // hosts exactly 40 once drawRotarySlider has taken its 4 px per side. Anything larger would
    // mean widening a module to keep the rack uniform.
    constexpr int Standard = 40;
    constexpr int Minimum  = 22;   // below this a rotary cannot be aimed; a cell that tight is a
                                   // layout bug, so the knob stops shrinking and lets it show.
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

    // Hover text for knobs with a named read-out (Knob::tooltipFromValue, else textFromValue).
    // Owned here so refreshNamedReadouts can re-derive it without reaching back to the descriptor.
    std::function<juce::String (double value)> tooltipFromValue;
    void refreshTooltip()
    {
        if (tooltipFromValue)
            setTooltip (tooltipFromValue (getValue()));
    }

    // Live modulation amount (-1..+1) for the animated ring the LookAndFeel draws
    // around the knob (Vital-style). 0 = no ring. Set by the editor's timer from
    // the current LFO value; repaints only on a meaningful change.
    void setModAmount(float a)
    {
        if (std::abs(a - modAmount) > 0.003f) { modAmount = a; repaint(); }
    }
    float getModAmount() const { return modAmount; }

    // Double-click restores the value the LOADED preset gave this knob (maintainer 2026-08-26).
    // Deliberately NOT JUCE's double-click-to-default (disabled where the attachment is made):
    // the factory default wipes a patch value, the preset baseline merely un-does the player's
    // own twist — and while the knob is untouched it is a no-op, so it cannot fire by accident.
    // Unset, or returning NaN (no clean baseline), a double-click does nothing.
    std::function<double()> presetBaseline;

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        if (presetBaseline)
        {
            const double v = presetBaseline();
            if (! std::isnan(v))
                setValue(v, juce::sendNotificationSync);
            return;
        }
        juce::Slider::mouseDoubleClick(e);
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

    // Coarse quantum for very long integer ranges (LEN across 768 steps): without Shift the knob
    // moves in multiples of this, Shift keeps single-interval fine control (maintainer 2026-09-02:
    // "ohne Shift meinetwegen in 8er Schritten, Shift+LEN weiterhin in Einzelschritten").
    void setCoarseStep(int quanta) { coarseStep = quanta; }

    // Drag path: snapValue sees every drag-derived value, so the quantising lives here — the
    // text box (dragMode == notDragging) stays exact, as does any programmatic setValue.
    double snapValue(double attemptedValue, juce::Slider::DragMode dragMode) override
    {
        if (coarseStep > 0 && dragMode != juce::Slider::notDragging
            && ! juce::ModifierKeys::currentModifiers.isShiftDown())
        {
            const double iv = getInterval() > 0.0 ? getInterval() : 1.0;
            const double q  = coarseStep * iv;
            return juce::jlimit(getMinimum(), getMaximum(), std::round(attemptedValue / q) * q);
        }
        return juce::Slider::snapValue(attemptedValue, dragMode);
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        const double range    = getMaximum() - getMinimum();
        const double interval = getInterval();
        if (range <= 0.0) { juce::Slider::mouseWheelMove(e, wheel); return; }

        const double dir = wheel.deltaY >= 0.0f ? 1.0 : -1.0;

        // Coarse-quantum knobs (LEN, 768 positions): a notch jumps to the NEXT multiple of the
        // quantum, Shift moves one interval — mirrors the drag behaviour above.
        if (coarseStep > 0)
        {
            const double iv = interval > 0.0 ? interval : 1.0;
            double nv;
            if (e.mods.isShiftDown())
                nv = getValue() + dir * iv;
            else
            {
                const double q = coarseStep * iv;
                const double m = std::floor(getValue() / q + 1e-9);
                nv = dir > 0 ? (m + 1) * q
                             : (getValue() - m * q < 1e-9 ? (m - 1) * q : m * q);
            }
            setValue(juce::jlimit(getMinimum(), getMaximum(), nv), juce::sendNotificationSync);
            return;
        }

        // Discrete ranges (unison voices 1..7, octaves, a STEP SEQ step at ±24 semitones): exactly
        // one interval per notch, and modifiers change nothing — there is nothing finer to reach.
        // The bound was 24 positions until the sequencer arrived: at 49 positions a step knob fell
        // through to the proportional branch below and moved TWO semitones per notch, which is not
        // a pitch you can aim at (user 2026-08-10). A knob whose interval is a semitone is discrete
        // however wide it is; only genuinely long integer ranges (SAMPLER ROOT, 24..96) still need
        // the proportional feel, or crossing them would take a hundred notches.
        if (interval > 0.0 && range / interval <= 48.0)
        {
            setValue(getValue() + dir * interval, juce::sendNotificationSync);
            return;
        }

        // Otherwise step in NORMALISED (knob-rotation) space, NOT raw value, so the feel is even
        // regardless of the parameter's skew — short-time knobs (ADSR: 1 ms interval, skewed) no
        // longer crawl in 1 ms steps. Coarse = ~4% of the knob's travel per notch, fine (Shift) =
        // ~0.5%. Guarantee at least one interval of movement so a notch never does nothing.
        const double prop = valueToProportionOfLength(getValue());
        const double step = e.mods.isShiftDown() ? 0.005 : 0.04;
        double newVal = proportionOfLengthToValue(juce::jlimit(0.0, 1.0, prop + dir * step));
        if (interval > 0.0 && std::abs(newVal - getValue()) < interval)
            newVal = getValue() + dir * interval;
        setValue(newVal, juce::sendNotificationSync);
    }

private:
    int knobDiameter = KnobSize::Small;   // single standard size for all modules
    float modAmount = 0.0f;   // live LFO modulation for the ring (-1..+1)
    int coarseStep = 0;       // >0: un-shifted moves quantise to multiples of this many intervals
};
