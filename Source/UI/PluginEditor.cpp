#include "PluginEditor.h"
#include "../DSP/WavetableBank.h"
#include "../Audio/PresetIO.h"

namespace
{
    // Reset a set of parameters to their default values (used by the per-
    // generator "↺" buttons to restore one sound source to its factory state).
    void resetParamsToDefault(juce::AudioProcessorValueTreeState& apvts,
                              const juce::StringArray& ids)
    {
        for (const auto& id : ids)
            if (auto* p = apvts.getParameter(id))
                p->setValueNotifyingHost(p->getDefaultValue());
    }

    // Style a small "↺" reset button in the given accent colour.
    void styleResetButton(juce::TextButton& b, juce::Colour c)
    {
        b.setButtonText(juce::String::fromUTF8("\xE2\x86\xBA"));
        b.setTooltip("Reset this source to default");
        b.setColour(juce::TextButton::buttonColourId, c.withAlpha(0.25f));
        b.setColour(juce::TextButton::textColourOffId, c);
    }
}

// --- LookAndFeel ---

SynthyLookAndFeel::SynthyLookAndFeel()
{
    setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff40c0ff));
    setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2a2a2a));
    setColour(juce::Slider::thumbColourId, juce::Colour(0xff40c0ff));
    setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff2a2a4a));
    setColour(juce::ComboBox::textColourId, juce::Colours::white);
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

// --- OscillatorPanel ---

OscillatorPanel::OscillatorPanel(juce::AudioProcessorValueTreeState& apvts,
                                  int oscIndex, juce::Colour color)
    : apvts(apvts), freqId("osc" + juce::String(oscIndex) + "Freq")
{
    auto prefix = "osc" + juce::String(oscIndex);

    enableBtn.setColour(juce::ToggleButton::tickColourId, color);
    addAndMakeVisible(enableBtn);
    enableAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, prefix + "On", enableBtn);

    title.setText("OSC " + juce::String(oscIndex), juce::dontSendNotification);
    title.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, color);
    title.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(title);

    // Small "↺" button — restores this oscillator's sound parameters to
    // default on one click, without touching the other two. The On toggle is
    // deliberately EXCLUDED so a reset never switches the oscillator off.
    styleResetButton(resetBtn, color);
    juce::StringArray myParams { prefix + "Wave", prefix + "Freq",
                                 prefix + "Amp", prefix + "UniVoices", prefix + "UniDetune" };
    resetBtn.onClick = [this, myParams]
    {
        resetParamsToDefault(this->apvts, myParams);
        setPlayedRatio(playedRatio);   // refresh the FREQ knob display right away
    };
    addAndMakeVisible(resetBtn);

    waveSelector.addItemList({"Sine", "Sawtooth", "Square", "Triangle"}, 1);
    addAndMakeVisible(waveSelector);
    waveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, prefix + "Wave", waveSelector);

    freqKnob.setColour(juce::Slider::thumbColourId, color);
    freqKnob.setColour(juce::Slider::rotarySliderFillColourId, color);
    freqKnob.setTextValueSuffix(" Hz");
    // Decoupled from the parameter (no SliderAttachment) so it can display the
    // played frequency (base × note ratio). Range/skew mirror the param.
    freqKnob.setRange(20.0, 10000.0, 1.0);
    freqKnob.setSkewFactor(0.3);
    addAndMakeVisible(freqKnob);
    freqLabel.setText("FREQ", juce::dontSendNotification);
    freqLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(freqLabel);
    freqKnob.onValueChange = [this]
    {
        if (auto* p = this->apvts.getParameter(freqId))
        {
            double er = enableBtn.getToggleState() ? playedRatio : 1.0;
            double base = freqKnob.getValue() / juce::jmax(0.0001, er);
            p->setValueNotifyingHost(p->convertTo0to1((float) base));
        }
    };
    setPlayedRatio(1.0);   // initialise display from the current base value

    ampKnob.setColour(juce::Slider::thumbColourId, color);
    ampKnob.setColour(juce::Slider::rotarySliderFillColourId, color);
    addAndMakeVisible(ampKnob);
    ampLabel.setText("AMP", juce::dontSendNotification);
    ampLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(ampLabel);
    ampAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, prefix + "Amp", ampKnob);

    uniVoicesKnob.setColour(juce::Slider::thumbColourId, color);
    uniVoicesKnob.setColour(juce::Slider::rotarySliderFillColourId, color);
    addAndMakeVisible(uniVoicesKnob);
    uniVoicesLabel.setText("VOICES", juce::dontSendNotification);
    uniVoicesLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(uniVoicesLabel);
    uniVoicesAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, prefix + "UniVoices", uniVoicesKnob);

    uniDetuneKnob.setColour(juce::Slider::thumbColourId, color);
    uniDetuneKnob.setColour(juce::Slider::rotarySliderFillColourId, color);
    addAndMakeVisible(uniDetuneKnob);
    uniDetuneLabel.setText("DETUNE", juce::dontSendNotification);
    uniDetuneLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(uniDetuneLabel);
    uniDetuneAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, prefix + "UniDetune", uniDetuneKnob);

    // Four knobs in a narrow panel → Small
    for (auto* k : { &freqKnob, &ampKnob, &uniVoicesKnob, &uniDetuneKnob })
        k->setKnobDiameter(KnobSize::Small);
}

void OscillatorPanel::setPlayedRatio(double ratio)
{
    playedRatio = ratio;
    if (freqKnob.isMouseButtonDown())
        return;   // don't fight the user while they drag the knob
    // Only an ENABLED oscillator follows the played pitch; a disabled one isn't
    // sounding, so it keeps showing its own base frequency.
    double er = enableBtn.getToggleState() ? ratio : 1.0;
    double base = (double) *apvts.getRawParameterValue(freqId);
    freqKnob.setValue(base * er, juce::dontSendNotification);
}

void OscillatorPanel::resized()
{
    auto area = getLocalBounds().reduced(6);
    auto titleRow = area.removeFromTop(22);
    enableBtn.setBounds(titleRow.removeFromLeft(28));
    resetBtn.setBounds(titleRow.removeFromRight(24).reduced(1));
    title.setBounds(titleRow);
    waveSelector.setBounds(area.removeFromTop(24).reduced(20, 0));
    area.removeFromTop(4);

    // Single knob row: FREQ | AMP | VOICES | DETUNE
    int kw = area.getWidth() / 4;
    auto c1 = area.removeFromLeft(kw);
    freqLabel.setBounds(c1.removeFromTop(14));
    freqKnob.setBounds(c1);
    auto c2 = area.removeFromLeft(kw);
    ampLabel.setBounds(c2.removeFromTop(14));
    ampKnob.setBounds(c2);
    auto c3 = area.removeFromLeft(kw);
    uniVoicesLabel.setBounds(c3.removeFromTop(14));
    uniVoicesKnob.setBounds(c3);
    uniDetuneLabel.setBounds(area.removeFromTop(14));
    uniDetuneKnob.setBounds(area);
}

// --- EffectPanel ---

EffectPanel::EffectPanel(juce::AudioProcessorValueTreeState& apvts,
                          const juce::String& name, juce::Colour color,
                          const juce::String& onParam,
                          const juce::StringArray& knobParams,
                          const juce::StringArray& knobLabels,
                          const juce::String& triggerText,
                          bool withReset)
{
    title.setText(name, juce::dontSendNotification);
    title.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, color);
    addAndMakeVisible(title);

    enableBtn.setButtonText("ON");
    enableBtn.setColour(juce::ToggleButton::tickColourId, color);
    addAndMakeVisible(enableBtn);
    btnAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, onParam, enableBtn);

    // Optional trigger button (e.g. PLUCK for the Karplus string).
    if (triggerText.isNotEmpty())
    {
        hasTrigger = true;
        triggerButton.setButtonText(triggerText);
        triggerButton.setColour(juce::TextButton::buttonColourId, color.darker(0.6f));
        triggerButton.onClick = [this] { if (onTrigger) onTrigger(); };
        addAndMakeVisible(triggerButton);
    }

    for (int i = 0; i < knobParams.size(); ++i)
    {
        auto* knob = new SynthySlider();
        knob->setColour(juce::Slider::thumbColourId, color);
        knob->setColour(juce::Slider::rotarySliderFillColourId, color);
        addAndMakeVisible(knob);
        knobs.add(knob);

        auto* label = new juce::Label();
        label->setText(knobLabels[i], juce::dontSendNotification);
        label->setJustificationType(juce::Justification::centred);
        label->setFont(juce::FontOptions(10.0f));
        addAndMakeVisible(label);
        labels.add(label);

        knobAttachments.add(new juce::AudioProcessorValueTreeState::SliderAttachment(
            apvts, knobParams[i], *knob));
    }

    // Optional "↺" button — resets this generator's knob params to default.
    // The on/enable param is deliberately excluded so a reset never disables it.
    if (withReset)
    {
        hasReset = true;
        styleResetButton(resetBtn, color);
        juce::StringArray ids = knobParams;
        auto* ap = &apvts;
        resetBtn.onClick = [ap, ids] { resetParamsToDefault(*ap, ids); };
        addAndMakeVisible(resetBtn);
    }
}

void EffectPanel::resized()
{
    auto area = getLocalBounds().reduced(4);
    auto top = area.removeFromTop(22);
    enableBtn.setBounds(top.removeFromLeft(50));
    if (hasReset)
        resetBtn.setBounds(top.removeFromRight(24).reduced(1));
    if (hasTrigger)
        triggerButton.setBounds(top.removeFromRight(64).reduced(2, 1));
    title.setBounds(top);

    int knobWidth = area.getWidth() / std::max(1, knobs.size());
    for (int i = 0; i < knobs.size(); ++i)
    {
        auto col = area.removeFromLeft(knobWidth);
        labels[i]->setBounds(col.removeFromTop(14));
        knobs[i]->setBounds(col);
    }
}

// --- SynthyEditor ---

juce::Slider& SynthyEditor::setupKnob(juce::Slider& knob, juce::Label& label,
                                        const juce::String& text)
{
    addAndMakeVisible(knob);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::FontOptions(10.0f));
    addAndMakeVisible(label);

    return knob;
}

void SynthyEditor::initResetButton(juce::TextButton& btn, juce::Colour colour,
                                   juce::StringArray paramIds, std::function<void()> afterReset)
{
    styleResetButton(btn, colour);
    btn.onClick = [this, paramIds, afterReset]
    {
        resetParamsToDefault(processor.getAPVTS(), paramIds);
        if (afterReset) afterReset();
    };
    addAndMakeVisible(btn);
}

SynthyEditor::SynthyEditor(SynthyProcessor& p)
    : AudioProcessorEditor(&p), processor(p),
      osc1(p.getAPVTS(), 1, juce::Colour(0xff40c0ff)),
      osc2(p.getAPVTS(), 2, juce::Colour(0xffff6b9d)),
      osc3(p.getAPVTS(), 3, juce::Colour(0xffc084fc)),
      delayPanel(p.getAPVTS(), "DELAY", juce::Colour(0xff38bdf8),
                 "delayOn", {"delayTime", "delayFeedback", "delayMix"}, {"TIME", "FDBK", "MIX"}, {}, true),
      chorusPanel(p.getAPVTS(), "CHORUS", juce::Colour(0xffa78bfa),
                  "chorusOn", {"chorusRate", "chorusDepth", "chorusMix"}, {"RATE", "DEPTH", "MIX"}, {}, true),
      reverbPanel(p.getAPVTS(), "REVERB", juce::Colour(0xfffb7185),
                  "reverbOn", {"reverbRoom", "reverbDamp", "reverbMix"}, {"ROOM", "DAMP", "MIX"}, {}, true),
      karplusPanel(p.getAPVTS(), "STRING (KARPLUS)", juce::Colour(0xff34d399),
                   "karplusOn", {"karplusFreq", "karplusAmp", "karplusDamping", "karplusStretch"},
                   {"FREQ", "AMP", "DAMP", "STR"}, {}, /*withReset*/ true),
      wavefoldPanel(p.getAPVTS(), "WAVEFOLD", juce::Colour(0xfffbbf24),
                    "wavefoldOn", {"wavefoldDrive", "wavefoldSymmetry", "wavefoldMix"},
                    {"DRIVE", "SYM", "MIX"}, {}, true),
      bitcrushPanel(p.getAPVTS(), "BITCRUSH", juce::Colour(0xff2dd4bf),
                    "bitcrushOn", {"bitcrushBits", "bitcrushRate", "bitcrushMix"},
                    {"BITS", "RATE", "MIX"}, {}, true)
{
    setLookAndFeel(&lnf);

    addAndMakeVisible(osc1);
    addAndMakeVisible(osc2);
    addAndMakeVisible(osc3);

    // Oscilloscope + Spectrum
    waveformDisplay = std::make_unique<WaveformDisplay>(p.getWaveformCapture());
    addAndMakeVisible(*waveformDisplay);

    spectrumDisplay = std::make_unique<SpectrumDisplay>(p.getWaveformCapture());
    addAndMakeVisible(*spectrumDisplay);

    // Mix mode — sits between OSC 1 and OSC 2; combines them, OSC 3 is added.
    auto mixGold = juce::Colour(0xfffacc15);
    mixModeTitle.setText("MIX", juce::dontSendNotification);
    mixModeTitle.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    mixModeTitle.setColour(juce::Label::textColourId, mixGold);
    mixModeTitle.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(mixModeTitle);

    mixModeSelector.addItemList({"Additive", "Ring Mod", "FM"}, 1);
    addAndMakeVisible(mixModeSelector);
    mixModeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        p.getAPVTS(), "mixMode", mixModeSelector);

    mixModeHint.setText("OSC 1 <-> 2", juce::dontSendNotification);
    mixModeHint.setFont(juce::FontOptions(9.0f));
    mixModeHint.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    mixModeHint.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(mixModeHint);

    // "+" between OSC 2 and OSC 3 (OSC 3 is always added).
    mixPlusLabel.setText("+", juce::dontSendNotification);
    mixPlusLabel.setFont(juce::FontOptions(26.0f, juce::Font::bold));
    mixPlusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff666666));
    mixPlusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(mixPlusLabel);

    // ADSR
    adsrTitle.setText("ENVELOPE (ADSR)", juce::dontSendNotification);
    adsrTitle.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    adsrTitle.setColour(juce::Label::textColourId, juce::Colour(0xff4ade80));
    adsrTitle.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(adsrTitle);

    auto green = juce::Colour(0xff4ade80);
    setupKnob(attackKnob, atkLabel, "ATK").setColour(juce::Slider::thumbColourId, green);
    attackKnob.setColour(juce::Slider::rotarySliderFillColourId, green);
    setupKnob(decayKnob, decLabel, "DEC").setColour(juce::Slider::thumbColourId, green);
    decayKnob.setColour(juce::Slider::rotarySliderFillColourId, green);
    setupKnob(sustainKnob, susLabel, "SUS").setColour(juce::Slider::thumbColourId, green);
    sustainKnob.setColour(juce::Slider::rotarySliderFillColourId, green);
    setupKnob(releaseKnob, relLabel, "REL").setColour(juce::Slider::thumbColourId, green);
    releaseKnob.setColour(juce::Slider::rotarySliderFillColourId, green);

    // ADSR are the focal knobs → Large
    attackKnob.setKnobDiameter(KnobSize::Large);
    decayKnob.setKnobDiameter(KnobSize::Large);
    sustainKnob.setKnobDiameter(KnobSize::Large);
    releaseKnob.setKnobDiameter(KnobSize::Large);

    // Time knobs have a 1 ms automation interval but want ~10 ms wheel steps
    // (fine 10 ms / coarse 100 ms) over their wide 0..5 s range.
    attackKnob.setWheelStep(0.01);
    decayKnob.setWheelStep(0.01);
    releaseKnob.setWheelStep(0.01);

    atkAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.getAPVTS(), "attack", attackKnob);
    decAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.getAPVTS(), "decay", decayKnob);
    susAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.getAPVTS(), "sustain", sustainKnob);
    relAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.getAPVTS(), "release", releaseKnob);

    initResetButton(adsrResetBtn, green, {"attack", "decay", "sustain", "release"});

    // Filter
    filterTitle.setText("FILTER", juce::dontSendNotification);
    filterTitle.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    filterTitle.setColour(juce::Label::textColourId, juce::Colour(0xfffb923c));
    filterTitle.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(filterTitle);

    filterType.addItemList({"Off", "Lowpass", "Highpass"}, 1);
    addAndMakeVisible(filterType);
    filterTypeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        p.getAPVTS(), "filterType", filterType);

    auto orange = juce::Colour(0xfffb923c);
    setupKnob(cutoffKnob, cutLabel, "CUTOFF").setColour(juce::Slider::thumbColourId, orange);
    cutoffKnob.setColour(juce::Slider::rotarySliderFillColourId, orange);
    cutoffKnob.setTextValueSuffix(" Hz");
    setupKnob(resoKnob, resoLabel, "RESO").setColour(juce::Slider::thumbColourId, orange);
    resoKnob.setColour(juce::Slider::rotarySliderFillColourId, orange);

    cutAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.getAPVTS(), "filterCutoff", cutoffKnob);
    resoAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.getAPVTS(), "filterReso", resoKnob);

    // filterType excluded (its "Off" = bypass) — reset must not disable the filter.
    initResetButton(filterResetBtn, orange, {"filterCutoff", "filterReso"});

    // Distortion (inline)
    auto distRed = juce::Colour(0xffef4444);
    distTitle.setText("DISTORTION", juce::dontSendNotification);
    distTitle.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    distTitle.setColour(juce::Label::textColourId, distRed);
    distTitle.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(distTitle);

    distTypeSelector.addItemList({"Off", "Soft Clip", "Hard Clip", "Foldback"}, 1);
    addAndMakeVisible(distTypeSelector);
    distTypeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        p.getAPVTS(), "distortionType", distTypeSelector);

    setupKnob(distDriveKnob, distDriveLabel, "DRIVE").setColour(juce::Slider::thumbColourId, distRed);
    distDriveKnob.setColour(juce::Slider::rotarySliderFillColourId, distRed);
    distDriveAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.getAPVTS(), "distortionDrive", distDriveKnob);

    setupKnob(distMixKnob, distMixLabel, "MIX").setColour(juce::Slider::thumbColourId, distRed);
    distMixKnob.setColour(juce::Slider::rotarySliderFillColourId, distRed);
    distMixAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.getAPVTS(), "distortionMix", distMixKnob);

    // distortionType excluded (its "Off" = bypass) — reset must not disable it.
    initResetButton(distResetBtn, distRed, {"distortionDrive", "distortionMix"});

    // Effects
    addAndMakeVisible(delayPanel);
    addAndMakeVisible(chorusPanel);
    addAndMakeVisible(reverbPanel);
    addAndMakeVisible(karplusPanel);
    addAndMakeVisible(wavefoldPanel);
    addAndMakeVisible(bitcrushPanel);

    // LFO
    auto cyan = juce::Colour(0xff22d3ee);
    lfoTitle.setText("LFO", juce::dontSendNotification);
    lfoTitle.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    lfoTitle.setColour(juce::Label::textColourId, cyan);
    lfoTitle.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(lfoTitle);

    lfoWaveSelector.addItemList({"Sine", "Triangle", "Square", "Sawtooth"}, 1);
    addAndMakeVisible(lfoWaveSelector);
    lfoWaveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        p.getAPVTS(), "lfoWave", lfoWaveSelector);

    lfoTargetSelector.addItemList({"Off", "Frequency", "Amplitude", "Filter Cutoff"}, 1);
    addAndMakeVisible(lfoTargetSelector);
    lfoTargetAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        p.getAPVTS(), "lfoTarget", lfoTargetSelector);

    setupKnob(lfoRateKnob, lfoRateLabel, "RATE").setColour(juce::Slider::thumbColourId, cyan);
    lfoRateKnob.setColour(juce::Slider::rotarySliderFillColourId, cyan);
    lfoRateKnob.setTextValueSuffix(" Hz");
    lfoRateAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.getAPVTS(), "lfoRate", lfoRateKnob);

    setupKnob(lfoDepthKnob, lfoDepthLabel, "DEPTH").setColour(juce::Slider::thumbColourId, cyan);
    lfoDepthKnob.setColour(juce::Slider::rotarySliderFillColourId, cyan);
    lfoDepthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.getAPVTS(), "lfoDepth", lfoDepthKnob);

    // lfoTarget excluded (its "Off" = no modulation) — reset keeps the routing.
    initResetButton(lfoResetBtn, cyan, {"lfoWave", "lfoRate", "lfoDepth"});

    // Arpeggiator (MODULATION zone)
    arpTitle.setText("ARPEGGIATOR", juce::dontSendNotification);
    arpTitle.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    arpTitle.setColour(juce::Label::textColourId, cyan);
    addAndMakeVisible(arpTitle);

    arpEnableBtn.setButtonText("ON");
    arpEnableBtn.setColour(juce::ToggleButton::tickColourId, cyan);
    addAndMakeVisible(arpEnableBtn);
    arpEnableAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.getAPVTS(), "arpOn", arpEnableBtn);

    arpModeSelector.addItemList({"Up", "Down", "Up/Down", "Random"}, 1);
    addAndMakeVisible(arpModeSelector);
    arpModeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        p.getAPVTS(), "arpMode", arpModeSelector);

    setupKnob(arpRateKnob, arpRateLabel, "RATE").setColour(juce::Slider::thumbColourId, cyan);
    arpRateKnob.setColour(juce::Slider::rotarySliderFillColourId, cyan);
    arpRateKnob.setTextValueSuffix(" /s");
    arpRateAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.getAPVTS(), "arpRate", arpRateKnob);

    setupKnob(arpOctavesKnob, arpOctavesLabel, "OCT").setColour(juce::Slider::thumbColourId, cyan);
    arpOctavesKnob.setColour(juce::Slider::rotarySliderFillColourId, cyan);
    arpOctavesAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.getAPVTS(), "arpOctaves", arpOctavesKnob);

    setupKnob(arpGateKnob, arpGateLabel, "GATE").setColour(juce::Slider::thumbColourId, cyan);
    arpGateKnob.setColour(juce::Slider::rotarySliderFillColourId, cyan);
    arpGateAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.getAPVTS(), "arpGate", arpGateKnob);

    // arpOn excluded — reset must not switch the arpeggiator off.
    initResetButton(arpResetBtn, cyan, {"arpRate", "arpMode", "arpOctaves", "arpGate"});

    // Master
    auto gold = juce::Colour(0xffffd700);
    setupKnob(masterKnob, masterLabel, "MASTER").setColour(juce::Slider::thumbColourId, gold);
    masterKnob.setColour(juce::Slider::rotarySliderFillColourId, gold);
    masterAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.getAPVTS(), "masterVol", masterKnob);

    initResetButton(masterResetBtn, gold, {"masterVol"});

    // Stereo width (inline in the header next to Master)
    auto stereoCol = juce::Colour(0xff818cf8);
    stereoTitle.setText("STEREO", juce::dontSendNotification);
    stereoTitle.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    stereoTitle.setColour(juce::Label::textColourId, stereoCol);
    stereoTitle.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(stereoTitle);

    stereoOnBtn.setButtonText("ON");
    stereoOnBtn.setColour(juce::ToggleButton::tickColourId, stereoCol);
    addAndMakeVisible(stereoOnBtn);
    stereoOnAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.getAPVTS(), "stereoOn", stereoOnBtn);

    for (auto* k : { &stereoWidthKnob, &stereoTimeKnob })
    {
        k->setColour(juce::Slider::thumbColourId, stereoCol);
        k->setColour(juce::Slider::rotarySliderFillColourId, stereoCol);
    }
    setupKnob(stereoWidthKnob, stereoWidthLabel, "WIDTH");
    setupKnob(stereoTimeKnob,  stereoTimeLabel,  "TIME");
    stereoWidthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.getAPVTS(), "stereoWidth", stereoWidthKnob);
    stereoTimeAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.getAPVTS(), "stereoTime", stereoTimeKnob);

    // stereoOn excluded — reset must not switch stereo off.
    initResetButton(stereoResetBtn, stereoCol, {"stereoWidth", "stereoTime"});

    // Preset Save / Load (shared .synthy JSON)
    addAndMakeVisible(saveBtn);
    addAndMakeVisible(loadBtn);
    saveBtn.onClick = [this]
    {
        presetChooser = std::make_unique<juce::FileChooser>(
            "Save preset", PresetIO::presetsFolder(), "*.synthy");
        auto flags = juce::FileBrowserComponent::saveMode
                   | juce::FileBrowserComponent::canSelectFiles
                   | juce::FileBrowserComponent::warnAboutOverwriting;
        presetChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File{}) return;
            if (! f.hasFileExtension("synthy")) f = f.withFileExtension("synthy");
            PresetIO::saveToFile(processor.getAPVTS(), f, f.getFileNameWithoutExtension());
            processor.markPresetClean();   // current state now matches the saved file
            setPresetName(f.getFileNameWithoutExtension());
        });
    };
    loadBtn.onClick = [this]
    {
        presetChooser = std::make_unique<juce::FileChooser>(
            "Load preset", PresetIO::presetsFolder(), "*.synthy");
        auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        presetChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File{}) return;
            PresetIO::loadFromFile(processor.getAPVTS(), f);
            processor.markPresetClean();   // current state now matches the loaded file
            setPresetName(f.getFileNameWithoutExtension());
        });
    };

    addAndMakeVisible(randomBtn);
    randomBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff6d28d9));
    randomBtn.onClick = [this] { processor.randomize(); setPresetName("Random"); };

    addAndMakeVisible(resetBtn);
    resetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff475569));
    resetBtn.onClick = [this] { processor.resetToDefault(); setPresetName("Init"); };

    // Current-preset display
    presetNameLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    presetNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    presetNameLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(presetNameLabel);
    setPresetName(processor.getCurrentPresetName());   // restored from LiveState

    // Noise
    auto noiseGrey = juce::Colour(0xff9ca3af);
    noiseTitle.setText("NOISE", juce::dontSendNotification);
    noiseTitle.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    noiseTitle.setColour(juce::Label::textColourId, noiseGrey);
    noiseTitle.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(noiseTitle);

    noiseTypeSelector.addItemList({"Off", "White", "Pink"}, 1);
    addAndMakeVisible(noiseTypeSelector);
    noiseTypeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        p.getAPVTS(), "noiseType", noiseTypeSelector);

    setupKnob(noiseAmpKnob, noiseAmpLabel, "AMP").setColour(juce::Slider::thumbColourId, noiseGrey);
    noiseAmpKnob.setColour(juce::Slider::rotarySliderFillColourId, noiseGrey);
    noiseAmpAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.getAPVTS(), "noiseAmp", noiseAmpKnob);

    // noiseType doubles as the on/off (its "Off" entry), so it's excluded —
    // a reset must not silence the noise source; only AMP is reset.
    initResetButton(noiseResetBtn, noiseGrey, {"noiseAmp"});

    // Sub oscillator (tracks OSC 1 pitch, octave(s) down)
    auto subBlue = juce::Colour(0xff60a5fa);
    subTitle.setText("SUB OSC", juce::dontSendNotification);
    subTitle.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    subTitle.setColour(juce::Label::textColourId, subBlue);
    subTitle.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(subTitle);

    subEnableBtn.setButtonText("ON");
    subEnableBtn.setColour(juce::ToggleButton::tickColourId, subBlue);
    addAndMakeVisible(subEnableBtn);
    subEnableAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.getAPVTS(), "subOn", subEnableBtn);

    subWaveSelector.addItemList({"Sine", "Square"}, 1);
    addAndMakeVisible(subWaveSelector);
    subWaveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        p.getAPVTS(), "subWave", subWaveSelector);

    subOctaveSelector.addItemList({"-1 Oct", "-2 Oct"}, 1);
    addAndMakeVisible(subOctaveSelector);
    subOctaveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        p.getAPVTS(), "subOctave", subOctaveSelector);

    setupKnob(subLevelKnob, subLevelLabel, "LEVEL").setColour(juce::Slider::thumbColourId, subBlue);
    subLevelKnob.setColour(juce::Slider::rotarySliderFillColourId, subBlue);
    subLevelAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.getAPVTS(), "subLevel", subLevelKnob);

    // subOn excluded — reset must not switch it off.
    initResetButton(subResetBtn, subBlue, {"subWave", "subOctave", "subLevel"});

    // Wavetable
    auto wtPink = juce::Colour(0xfff472b6);
    wtTitle.setText("WAVETABLE", juce::dontSendNotification);
    wtTitle.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    wtTitle.setColour(juce::Label::textColourId, wtPink);
    wtTitle.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(wtTitle);

    wtEnableBtn.setButtonText("ON");
    wtEnableBtn.setColour(juce::ToggleButton::tickColourId, wtPink);
    addAndMakeVisible(wtEnableBtn);
    wtEnableAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.getAPVTS(), "wavetableOn", wtEnableBtn);

    addAndMakeVisible(wtBankSelector);
    refreshBankSelector();
    wtBankSelector.onChange = [this]
    {
        int idx = wtBankSelector.getSelectedId() - 1;
        if (idx < 0) return;
        if (auto* param = processor.getAPVTS().getParameter("wavetableBank"))
            param->setValueNotifyingHost(param->convertTo0to1((float) idx));
    };

    wtLoadBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a2a3a));
    addAndMakeVisible(wtLoadBtn);
    wtLoadBtn.onClick = [this]
    {
        wtFileChooser = std::make_unique<juce::FileChooser>(
            "Select a WAV file to load as wavetable", juce::File{}, "*.wav");
        auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        wtFileChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File{}) return;
            int idx = WavetableBankStore::instance().loadWav(file);
            if (idx < 0) return;
            refreshBankSelector();
            wtBankSelector.setSelectedId(idx + 1, juce::sendNotification);
        });
    };

    setupKnob(wtPositionKnob, wtPositionLabel, "POS").setColour(juce::Slider::thumbColourId, wtPink);
    wtPositionKnob.setColour(juce::Slider::rotarySliderFillColourId, wtPink);
    wtPositionAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.getAPVTS(), "wavetablePosition", wtPositionKnob);

    setupKnob(wtFreqKnob, wtFreqLabel, "FREQ").setColour(juce::Slider::thumbColourId, wtPink);
    wtFreqKnob.setColour(juce::Slider::rotarySliderFillColourId, wtPink);
    wtFreqKnob.setTextValueSuffix(" Hz");
    wtFreqAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.getAPVTS(), "wavetableFreq", wtFreqKnob);

    setupKnob(wtAmpKnob, wtAmpLabel, "AMP").setColour(juce::Slider::thumbColourId, wtPink);
    wtAmpKnob.setColour(juce::Slider::rotarySliderFillColourId, wtPink);
    wtAmpAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.getAPVTS(), "wavetableAmp", wtAmpKnob);

    setupKnob(wtVoicesKnob, wtVoicesLabel, "VOICES").setColour(juce::Slider::thumbColourId, wtPink);
    wtVoicesKnob.setColour(juce::Slider::rotarySliderFillColourId, wtPink);
    wtVoicesAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.getAPVTS(), "wavetableUniVoices", wtVoicesKnob);

    setupKnob(wtDetuneKnob, wtDetuneLabel, "DETUNE").setColour(juce::Slider::thumbColourId, wtPink);
    wtDetuneKnob.setColour(juce::Slider::rotarySliderFillColourId, wtPink);
    wtDetuneAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.getAPVTS(), "wavetableUniDetune", wtDetuneKnob);

    // Wavetable packs five knobs into one row → Small
    for (auto* k : { &wtPositionKnob, &wtFreqKnob, &wtAmpKnob, &wtVoicesKnob, &wtDetuneKnob })
        k->setKnobDiameter(KnobSize::Small);

    // wavetableOn excluded — reset must not switch it off. The bank combo is
    // manually synced (no attachment), so refresh it after the reset.
    initResetButton(wtResetBtn, wtPink,
        {"wavetableBank", "wavetablePosition", "wavetableFreq",
         "wavetableAmp", "wavetableUniVoices", "wavetableUniDetune"},
        [this] { refreshBankSelector(); });

    // On-screen keyboard (shares the processor's MidiKeyboardState → plays the
    // active generators with full ADSR per note, transposed relative to C4).
    keyboard = std::make_unique<juce::MidiKeyboardComponent>(
        processor.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard);
    keyboard->setAvailableRange(21, 108);  // A0 .. C8 (full 88-key piano)
    keyboard->setKeyWidth(20.0f);          // overridden in resized() to fill the row width
    keyboard->setKeyPressBaseOctave(kbBaseOctave);
    keyboard->setMidiChannelsToDisplay(1);   // only highlight played (ch.1) notes, not the ch.16 drone
    // Allow playing via the computer keyboard (a, w, s, e, d, ... map to notes;
    // z / x shift the octave; the keyboard must have focus — grabbed on launch/click).
    keyboard->setWantsKeyboardFocus(true);
    addAndMakeVisible(*keyboard);
    juce::Component::SafePointer<juce::MidiKeyboardComponent> kbPtr(keyboard.get());
    juce::MessageManager::callAsync([kbPtr]() mutable { if (kbPtr) kbPtr->grabKeyboardFocus(); });

    // The editor itself must NOT grab keyboard focus either (a click on the empty
    // background would otherwise steal it from the keyboard). z / x still reach our
    // keyPressed via event bubbling up from the focused keyboard component.
    setWantsKeyboardFocus(false);

    // Stop every knob/toggle/combo from grabbing keyboard focus when clicked, so
    // the on-screen keyboard keeps focus and the computer keys keep playing notes
    // even WHILE the user is tweaking parameters. (The keyboard keeps its focus;
    // a slider's right-click value box still grabs focus on demand for typing.)
    std::function<void(juce::Component&)> dropFocus = [&](juce::Component& parent)
    {
        for (auto* child : parent.getChildren())
        {
            if (child == keyboard.get())
                continue;   // the keyboard MUST keep keyboard focus
            child->setWantsKeyboardFocus(false);
            dropFocus(*child);
        }
    };
    dropFocus(*this);

    // setSize must be LAST so resized() sees all components. The two-column
    // body fits a 1520x945 design canvas (see resized()).
    constexpr int kDesignW = 1520, kDesignH = 945;
    setSize(kDesignW, kDesignH);

    // --- Auto-fit ---------------------------------------------------------
    // Backup for displays whose usable area is still smaller than the design
    // canvas (e.g. 1366x768 laptops): scale the WHOLE editor down via a
    // transform. The standalone window sizes itself from
    // getLocalArea(editor, ...) which honours the transform, so the window
    // shrinks to match. Proportions stay intact; we never scale above 1.0.
    if (auto* disp = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto ua = disp->userBounds;        // excludes the taskbar
        const double chrome = 90.0;              // title bar + a little breathing room
        const double sH = (ua.getHeight() - chrome) / (double) kDesignH;
        const double sW =  ua.getWidth()           / (double) kDesignW;
        const double scale = juce::jlimit(0.5, 1.0, juce::jmin(sH, sW));
        if (scale < 0.999)
            setTransform(juce::AffineTransform::scale((float) scale));
    }

    // Drive the OSC FREQ-knob display (played frequency).
    startTimerHz(30);
}

void SynthyEditor::timerCallback()
{
    double ratio = processor.getCurrentNoteRatio();
    osc1.setPlayedRatio(ratio);
    osc2.setPlayedRatio(ratio);
    osc3.setPlayedRatio(ratio);

    // Live modulation rings: route the current LFO value to whichever knob(s) the
    // LFO targets (0 Off, 1 Frequency, 2 Amplitude, 3 FilterCutoff); 0 elsewhere.
    float lfo = processor.getLfoDisplayValue();
    int target = (int) *processor.getAPVTS().getRawParameterValue("lfoTarget");
    float freqMod = (target == 1) ? lfo : 0.0f;
    float ampMod  = (target == 2) ? lfo : 0.0f;
    osc1.setFreqMod(freqMod); osc2.setFreqMod(freqMod); osc3.setFreqMod(freqMod);
    osc1.setAmpMod(ampMod);   osc2.setAmpMod(ampMod);   osc3.setAmpMod(ampMod);
    cutoffKnob.setModAmount((target == 3) ? lfo : 0.0f);

    // Keep the header label in sync: it reacts both to the (async-restored)
    // preset name and to live edits flipping the "modified" flag.
    updatePresetLabel();
}

// A loaded-and-untouched preset shows its name; once any parameter changes
// (and until the user saves) it's an unsaved working state → "Current State".
void SynthyEditor::updatePresetLabel()
{
    auto text = processor.isPresetModified()
                  ? juce::String("Current State")
                  : ("Preset: " + processor.getCurrentPresetName());
    if (text != shownLabel)
    {
        shownLabel = text;
        presetNameLabel.setText(text, juce::dontSendNotification);
    }
}

void SynthyEditor::setPresetName(const juce::String& name)
{
    processor.setCurrentPresetName(name);   // keep the processor (LiveState) in sync
    updatePresetLabel();
}

void SynthyEditor::refreshBankSelector()
{
    wtBankSelector.clear(juce::dontSendNotification);
    auto names = WavetableBankStore::instance().getNames();
    for (int i = 0; i < names.size(); ++i)
        wtBankSelector.addItem(names[i], i + 1); // itemId = index + 1

    int current = (int) *processor.getAPVTS().getRawParameterValue("wavetableBank");
    current = juce::jlimit(0, juce::jmax(0, names.size() - 1), current);
    wtBankSelector.setSelectedId(current + 1, juce::dontSendNotification);
}

bool SynthyEditor::keyPressed(const juce::KeyPress& key)
{
    // z / x shift the computer-keyboard octave (these keys aren't note keys).
    auto c = key.getTextCharacter();
    if (c == 'z' || c == 'Z' || c == 'x' || c == 'X')
    {
        int dir = (c == 'z' || c == 'Z') ? -1 : 1;
        kbBaseOctave = juce::jlimit(1, 7, kbBaseOctave + dir);
        if (keyboard)
            keyboard->setKeyPressBaseOctave(kbBaseOctave);
        return true;
    }
    return false;
}

void SynthyEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));

    // Title: big "J A S S" with the full name as a small subtitle beneath it.
    {
        auto titleArea = g_titleBounds;
        auto subArea = titleArea.removeFromBottom(13);
        g.setFont(juce::FontOptions(28.0f, juce::Font::bold));
        g.setColour(juce::Colour(0xff40c0ff));
        g.drawText("J A S S", titleArea, juce::Justification::centred);
        g.setFont(juce::FontOptions(10.0f));
        g.setColour(juce::Colour(0xff8899aa));
        g.drawText("Just Another Simple Synthesizer", subArea, juce::Justification::centred);
    }

    // Section backgrounds
    auto drawSection = [&](juce::Rectangle<int> bounds) {
        g.setColour(juce::Colour(0xff22223a));
        g.fillRoundedRectangle(bounds.toFloat(), 10.0f);
    };

    // --- Zone 1: Tonerzeuger ---
    drawSection(osc1.getBounds().expanded(2));
    drawSection(osc2.getBounds().expanded(2));
    drawSection(osc3.getBounds().expanded(2));
    drawSection(noiseBounds.expanded(2));
    drawSection(subBounds.expanded(2));
    drawSection(karplusPanel.getBounds().expanded(2));
    drawSection(wtBounds.expanded(2));

    // --- Zone 2: Modulation ---
    drawSection(adsrBounds.expanded(2));
    drawSection(lfoBounds.expanded(2));
    drawSection(arpBounds.expanded(2));

    // --- Zone 3: Soundverarbeitung ---
    drawSection(filterBounds.expanded(2));
    drawSection(distBounds.expanded(2));
    drawSection(wavefoldPanel.getBounds().expanded(2));
    drawSection(bitcrushPanel.getBounds().expanded(2));
    drawSection(delayPanel.getBounds().expanded(2));
    drawSection(chorusPanel.getBounds().expanded(2));
    drawSection(reverbPanel.getBounds().expanded(2));

    // Stereo width controls live in the header (left of Master), framed like the rest.
    drawSection(stereoBounds.expanded(2));

    // Zone separator headers: bold label on the left + a divider rule across.
    auto drawZoneHeader = [&](juce::Rectangle<int> bounds, const juce::String& text, juce::Colour col)
    {
        if (bounds.isEmpty()) return;
        g.setColour(col);
        g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
        auto label = bounds.removeFromLeft(220);
        g.drawText(text, label, juce::Justification::centredLeft);
        auto yMid = (float) bounds.getCentreY();
        g.setColour(col.withAlpha(0.35f));
        g.drawLine((float) bounds.getX(), yMid, (float) bounds.getRight() - 4.0f, yMid, 1.5f);
    };
    drawZoneHeader(genHeaderBounds,  "GENERATORS", juce::Colour(0xff40c0ff));
    drawZoneHeader(modHeaderBounds,  "MODULATION", juce::Colour(0xff22d3ee));
    drawZoneHeader(procHeaderBounds, "PROCESSING", juce::Colour(0xfffb923c));
}

void SynthyEditor::resized()
{
    auto area = getLocalBounds().reduced(12);

    // ===== Header — Save/Load/Random/Reset left, Title+Preset center, Master right =====
    auto headerRow = area.removeFromTop(88);
    auto leftBtns = headerRow.removeFromLeft(150);
    auto row1 = leftBtns.removeFromTop(30);
    saveBtn.setBounds(row1.removeFromLeft(row1.getWidth() / 2).reduced(3, 2));
    loadBtn.setBounds(row1.reduced(3, 2));
    auto row2 = leftBtns.removeFromTop(30);
    randomBtn.setBounds(row2.removeFromLeft(row2.getWidth() / 2).reduced(3, 2));
    resetBtn.setBounds(row2.reduced(3, 2));
    auto masterArea = headerRow.removeFromRight(100);
    auto masterLabelRow = masterArea.removeFromTop(14);
    masterResetBtn.setBounds(masterLabelRow.removeFromRight(20));
    masterLabel.setBounds(masterLabelRow);
    masterKnob.setBounds(masterArea);
    // STEREO inline left of Master: title + ON stacked on the left, then the
    // WIDTH/TIME knobs at full header height so they match the Master knob size.
    auto stereoArea = headerRow.removeFromRight(200);
    stereoBounds = stereoArea;
    auto stTitleCol = stereoArea.removeFromLeft(56);
    stereoTitle.setBounds(stTitleCol.removeFromTop(20));
    stTitleCol.removeFromTop(3);
    stereoOnBtn.setBounds(stTitleCol.removeFromTop(22).reduced(6, 0));
    stTitleCol.removeFromTop(2);
    stereoResetBtn.setBounds(stTitleCol.removeFromTop(18).reduced(14, 0));
    auto stKnobW = stereoArea.getWidth() / 2;
    auto stWCol = stereoArea.removeFromLeft(stKnobW);
    stereoWidthLabel.setBounds(stWCol.removeFromTop(14));
    stereoWidthKnob.setBounds(stWCol);
    stereoTimeLabel.setBounds(stereoArea.removeFromTop(14));
    stereoTimeKnob.setBounds(stereoArea);
    // Center: SYNTHY title (top) + current-preset name (below)
    auto centerArea = headerRow;
    g_titleBounds = centerArea.removeFromTop(centerArea.getHeight() - 18);
    presetNameLabel.setBounds(centerArea);
    area.removeFromTop(8);

    // ============================================================
    // Two-column body: the header (above) plus the visualisation and
    // keyboard (below) stay full width; the three zones split into a
    // GENERATORS column on the left and a MODULATION + PROCESSING
    // column on the right. This trades the tall single stack for a
    // wider, shorter window that fits short displays (e.g. 1920x1200).
    // ============================================================

    // Reserve the full-width footer (visualisation + keyboard) first so
    // the columns only consume the middle band.
    auto kbRow  = area.removeFromBottom(72).reduced(3, 0);
    area.removeFromBottom(6);
    auto vizRow = area.removeFromBottom(150).reduced(3, 0);
    area.removeFromBottom(8);

    const int colGap = 16;
    auto leftCol  = area.removeFromLeft((area.getWidth() - colGap) / 2);
    area.removeFromLeft(colGap);
    auto rightCol = area;

    // ============================================================
    // LEFT COLUMN — ZONE 1: GENERATORS (sound sources)
    // ============================================================
    genHeaderBounds = leftCol.removeFromTop(24);
    leftCol.removeFromTop(2);

    // Oscillators with MIX MODE wired between OSC 1 & 2, and "+" before OSC 3.
    auto oscRow = leftCol.removeFromTop(165);
    const int mixW = 90, plusW = 34;
    int oscW = (oscRow.getWidth() - mixW - plusW) / 3;
    osc1.setBounds(oscRow.removeFromLeft(oscW).reduced(3));

    auto mixCol = oscRow.removeFromLeft(mixW).withSizeKeepingCentre(mixW, 66);
    mixModeTitle.setBounds(mixCol.removeFromTop(16));
    mixModeSelector.setBounds(mixCol.removeFromTop(26).reduced(4, 0));
    mixModeHint.setBounds(mixCol.removeFromTop(16));

    osc2.setBounds(oscRow.removeFromLeft(oscW).reduced(3));
    mixPlusLabel.setBounds(oscRow.removeFromLeft(plusW));
    osc3.setBounds(oscRow.reduced(3));
    leftCol.removeFromTop(6);

    // Noise | Sub Osc | Karplus (String)
    auto genRow = leftCol.removeFromTop(150);
    int genW = genRow.getWidth() / 4;

    auto noiseArea = genRow.removeFromLeft(genW).reduced(3);
    noiseBounds = noiseArea;
    auto noiseTitleRow = noiseArea.removeFromTop(20);
    noiseResetBtn.setBounds(noiseTitleRow.removeFromRight(22).reduced(1));
    noiseTitle.setBounds(noiseTitleRow);
    noiseTypeSelector.setBounds(noiseArea.removeFromTop(24).reduced(12, 0));
    noiseArea.removeFromTop(6);
    noiseAmpLabel.setBounds(noiseArea.removeFromTop(14));
    noiseAmpKnob.setBounds(noiseArea.withSizeKeepingCentre(
        juce::jmin(90, noiseArea.getWidth()), noiseArea.getHeight()));

    // Sub oscillator
    auto subArea = genRow.removeFromLeft(genW).reduced(3);
    subBounds = subArea;
    auto subTitleRow = subArea.removeFromTop(22);
    subEnableBtn.setBounds(subTitleRow.removeFromLeft(44));
    subResetBtn.setBounds(subTitleRow.removeFromRight(22).reduced(1));
    subTitle.setBounds(subTitleRow);
    subWaveSelector.setBounds(subArea.removeFromTop(24).reduced(10, 0));
    subOctaveSelector.setBounds(subArea.removeFromTop(24).reduced(10, 0));
    subArea.removeFromTop(2);
    subLevelLabel.setBounds(subArea.removeFromTop(14));
    subLevelKnob.setBounds(subArea.withSizeKeepingCentre(
        juce::jmin(80, subArea.getWidth()), subArea.getHeight()));

    karplusPanel.setBounds(genRow.reduced(3)); // remaining two columns (4 knobs)
    leftCol.removeFromTop(6);

    // Wavetable (full column width)
    auto wtRow = leftCol.removeFromTop(124).reduced(3, 0);
    wtBounds = wtRow;
    auto wtTop = wtRow.removeFromTop(22);
    wtEnableBtn.setBounds(wtTop.removeFromLeft(50));
    wtResetBtn.setBounds(wtTop.removeFromRight(22).reduced(1));
    wtTitle.setBounds(wtTop);
    auto wtControls = wtRow.removeFromTop(26);
    wtLoadBtn.setBounds(wtControls.removeFromRight(90).reduced(2, 2));
    wtBankSelector.setBounds(wtControls.reduced(4, 2));
    wtRow.removeFromTop(2);
    int wtKnobW = wtRow.getWidth() / 5;
    auto wk1 = wtRow.removeFromLeft(wtKnobW);
    wtPositionLabel.setBounds(wk1.removeFromTop(14));
    wtPositionKnob.setBounds(wk1);
    auto wk2 = wtRow.removeFromLeft(wtKnobW);
    wtFreqLabel.setBounds(wk2.removeFromTop(14));
    wtFreqKnob.setBounds(wk2);
    auto wk3 = wtRow.removeFromLeft(wtKnobW);
    wtAmpLabel.setBounds(wk3.removeFromTop(14));
    wtAmpKnob.setBounds(wk3);
    auto wk4 = wtRow.removeFromLeft(wtKnobW);
    wtVoicesLabel.setBounds(wk4.removeFromTop(14));
    wtVoicesKnob.setBounds(wk4);
    wtDetuneLabel.setBounds(wtRow.removeFromTop(14));
    wtDetuneKnob.setBounds(wtRow);

    // ============================================================
    // RIGHT COLUMN — ZONE 2: MODULATION (ADSR + LFO + Arp)
    // ============================================================
    modHeaderBounds = rightCol.removeFromTop(24);
    rightCol.removeFromTop(2);

    auto modRow = rightCol.removeFromTop(150);
    auto adsrArea = modRow.removeFromLeft(modRow.getWidth() / 2).reduced(3);
    adsrBounds = adsrArea;
    auto adsrTitleRow = adsrArea.removeFromTop(20);
    adsrResetBtn.setBounds(adsrTitleRow.removeFromRight(22).reduced(1));
    adsrTitle.setBounds(adsrTitleRow);
    int knobW = adsrArea.getWidth() / 4;
    auto a1 = adsrArea.removeFromLeft(knobW);
    atkLabel.setBounds(a1.removeFromTop(14));
    attackKnob.setBounds(a1);
    auto a2 = adsrArea.removeFromLeft(knobW);
    decLabel.setBounds(a2.removeFromTop(14));
    decayKnob.setBounds(a2);
    auto a3 = adsrArea.removeFromLeft(knobW);
    susLabel.setBounds(a3.removeFromTop(14));
    sustainKnob.setBounds(a3);
    relLabel.setBounds(adsrArea.removeFromTop(14));
    releaseKnob.setBounds(adsrArea);

    auto lfoArea = modRow.reduced(3);
    lfoBounds = lfoArea;
    auto lfoTitleRow = lfoArea.removeFromTop(20);
    lfoResetBtn.setBounds(lfoTitleRow.removeFromRight(22).reduced(1));
    lfoTitle.setBounds(lfoTitleRow);
    auto lfoSelectors = lfoArea.removeFromTop(28);
    lfoWaveSelector.setBounds(lfoSelectors.removeFromLeft(lfoSelectors.getWidth() / 2).reduced(4, 2));
    lfoTargetSelector.setBounds(lfoSelectors.reduced(4, 2));
    lfoArea.removeFromTop(2);
    auto lfoKnob1 = lfoArea.removeFromLeft(lfoArea.getWidth() / 2);
    lfoRateLabel.setBounds(lfoKnob1.removeFromTop(14));
    lfoRateKnob.setBounds(lfoKnob1);
    lfoDepthLabel.setBounds(lfoArea.removeFromTop(14));
    lfoDepthKnob.setBounds(lfoArea);
    rightCol.removeFromTop(6);

    // Arpeggiator row: [ON + title | MODE] left, then RATE / OCT / GATE knobs.
    auto arpArea = rightCol.removeFromTop(96).reduced(3);
    arpBounds = arpArea;
    auto arpTop = arpArea.removeFromTop(22);
    arpEnableBtn.setBounds(arpTop.removeFromLeft(50));
    arpResetBtn.setBounds(arpTop.removeFromRight(22).reduced(1));
    arpTitle.setBounds(arpTop.removeFromLeft(150));
    arpArea.removeFromTop(2);
    // Left: MODE selector; right: three knobs.
    int arpColW = arpArea.getWidth() / 4;
    auto modeCol = arpArea.removeFromLeft(arpColW + 30);
    arpModeSelector.setBounds(modeCol.withSizeKeepingCentre(modeCol.getWidth() - 8, 26));
    auto arpKnobW = arpArea.getWidth() / 3;
    auto ak1 = arpArea.removeFromLeft(arpKnobW);
    arpRateLabel.setBounds(ak1.removeFromTop(14));
    arpRateKnob.setBounds(ak1);
    auto ak2 = arpArea.removeFromLeft(arpKnobW);
    arpOctavesLabel.setBounds(ak2.removeFromTop(14));
    arpOctavesKnob.setBounds(ak2);
    arpGateLabel.setBounds(arpArea.removeFromTop(14));
    arpGateKnob.setBounds(arpArea);
    rightCol.removeFromTop(10);

    // ============================================================
    // RIGHT COLUMN — ZONE 3: PROCESSING (filter + shapers + effects)
    // ============================================================
    procHeaderBounds = rightCol.removeFromTop(24);
    rightCol.removeFromTop(2);

    // Filter | Distortion
    auto fdRow = rightCol.removeFromTop(145);
    auto filterArea = fdRow.removeFromLeft(fdRow.getWidth() / 2).reduced(3);
    filterBounds = filterArea;
    auto filterTitleRow = filterArea.removeFromTop(20);
    filterResetBtn.setBounds(filterTitleRow.removeFromRight(22).reduced(1));
    filterTitle.setBounds(filterTitleRow);
    filterType.setBounds(filterArea.removeFromTop(24).reduced(20, 0));
    filterArea.removeFromTop(4);
    auto fLeft = filterArea.removeFromLeft(filterArea.getWidth() / 2);
    cutLabel.setBounds(fLeft.removeFromTop(14));
    cutoffKnob.setBounds(fLeft);
    resoLabel.setBounds(filterArea.removeFromTop(14));
    resoKnob.setBounds(filterArea);

    auto distArea = fdRow.reduced(3);
    distBounds = distArea;
    auto distTitleRow = distArea.removeFromTop(20);
    distResetBtn.setBounds(distTitleRow.removeFromRight(22).reduced(1));
    distTitle.setBounds(distTitleRow);
    distTypeSelector.setBounds(distArea.removeFromTop(24).reduced(20, 0));
    distArea.removeFromTop(4);
    auto dLeft = distArea.removeFromLeft(distArea.getWidth() / 2);
    distDriveLabel.setBounds(dLeft.removeFromTop(14));
    distDriveKnob.setBounds(dLeft);
    distMixLabel.setBounds(distArea.removeFromTop(14));
    distMixKnob.setBounds(distArea);
    rightCol.removeFromTop(6);

    // Wavefold | Bitcrush | Delay | Chorus | Reverb
    auto fxRow = rightCol.removeFromTop(120);
    int fxW = fxRow.getWidth() / 5;
    wavefoldPanel.setBounds(fxRow.removeFromLeft(fxW).reduced(3));
    bitcrushPanel.setBounds(fxRow.removeFromLeft(fxW).reduced(3));
    delayPanel.setBounds(fxRow.removeFromLeft(fxW).reduced(3));
    chorusPanel.setBounds(fxRow.removeFromLeft(fxW).reduced(3));
    reverbPanel.setBounds(fxRow.reduced(3));

    // ===== Footer: Visualisation (scope | spectrum) + Keyboard, full width =====
    if (waveformDisplay)
        waveformDisplay->setBounds(vizRow.removeFromLeft(vizRow.getWidth() / 2).withTrimmedRight(3));
    if (spectrumDisplay)
        spectrumDisplay->setBounds(vizRow.withTrimmedLeft(3));

    if (keyboard)
    {
        keyboard->setBounds(kbRow.reduced(2));
        // Spread the configured range across the full row width instead of
        // leaving blank space to the right: size each key so all white keys
        // in the range exactly fill the keyboard.
        int whiteKeys = 0;
        for (int n = keyboard->getRangeStart(); n <= keyboard->getRangeEnd(); ++n)
            if (! juce::MidiMessage::isMidiNoteBlack(n)) ++whiteKeys;
        if (whiteKeys > 0)
            keyboard->setKeyWidth((float) keyboard->getWidth() / (float) whiteKeys);
    }
}
