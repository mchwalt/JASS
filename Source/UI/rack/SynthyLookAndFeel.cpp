#include "SynthyLookAndFeel.h"

SynthyLookAndFeel::SynthyLookAndFeel()
{
    setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff40c0ff));
    setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2a2a2a));
    setColour(juce::Slider::thumbColourId, juce::Colour(0xff40c0ff));
    setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff2a2a4a));
    setColour(juce::ComboBox::textColourId, juce::Colours::white);
}

juce::Font SynthyLookAndFeel::getLabelFont (juce::Label& label)
{
    // The module title keeps its own bold emphasis; every other label (captions,
    // slider value boxes) renders at the one uniform UI size.
    if (label.getComponentID() == "moduleTitle")
        return juce::Font (juce::FontOptions (kUiFontSize + 1.0f, juce::Font::bold));
    return juce::Font (juce::FontOptions (kUiFontSize));
}

juce::Font SynthyLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (kUiFontSize));
}

juce::Font SynthyLookAndFeel::getPopupMenuFont()
{
    return juce::Font (juce::FontOptions (kUiFontSize));
}

void SynthyLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float rotaryStartAngle,
                                          float rotaryEndAngle, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    // Cap to the slider's knob-size category so the whole UI uses just three sizes.
    if (auto* ss = dynamic_cast<SynthySlider*>(&slider))
        radius = juce::jmin(radius, ss->getKnobDiameter() / 2.0f);
    auto centreX = bounds.getCentreX();
    auto centreY = bounds.getCentreY();
    auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Outer ring
    g.setColour(juce::Colour(0xff555555));
    g.drawEllipse(centreX - radius, centreY - radius, radius * 2, radius * 2, 2.0f);

    // Inner circle
    g.setColour(juce::Colour(0xff3a3a3a));
    g.fillEllipse(centreX - radius * 0.8f, centreY - radius * 0.8f,
                  radius * 1.6f, radius * 1.6f);

    // Live modulation ring (Vital-style): an arc just outside the knob that
    // extends from the set value towards where the LFO is currently pushing it.
    if (auto* ss = dynamic_cast<SynthySlider*>(&slider))
    {
        float mod = ss->getModAmount();
        if (std::abs(mod) > 0.003f)
        {
            auto sweep = rotaryEndAngle - rotaryStartAngle;
            auto target = juce::jlimit(rotaryStartAngle, rotaryEndAngle, angle + mod * sweep);
            auto ringR = radius + 5.0f;
            juce::Path arc;
            arc.addCentredArc(centreX, centreY, ringR, ringR, 0.0f,
                              juce::jmin(angle, target), juce::jmax(angle, target), true);
            g.setColour(juce::Colour(0xff22d3ee).withAlpha(0.45f));   // MODULATION cyan (subtle)
            g.strokePath(arc, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        }
    }

    // Indicator line
    auto lineLength = radius * 0.6f;
    auto lineX = centreX + lineLength * std::sin(angle);
    auto lineY = centreY - lineLength * std::cos(angle);
    auto thumbColour = slider.findColour(juce::Slider::thumbColourId);
    g.setColour(thumbColour);
    g.drawLine(centreX, centreY, lineX, lineY, 3.0f);

    // Center dot
    g.fillEllipse(centreX - 4, centreY - 4, 8, 8);
}
