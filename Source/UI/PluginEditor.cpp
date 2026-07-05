#include "PluginEditor.h"
#include "../DSP/WavetableBank.h"
#include "../Audio/PresetIO.h"
#include "../Audio/Parameters.h"   // Parameters::ID for the Story-1.3 sample rack

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

    // TEMP (Story 1.3): a stand-in for a real graphical display, used so the sample
    // rack can exercise the Display body-element path. Removed with the sample
    // population in Story 1.5.
    struct SampleDisplayPlaceholder : juce::Component
    {
        explicit SampleDisplayPlaceholder(juce::String t) : text(std::move(t)) {}
        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour(0xff070809));
            g.setColour(juce::Colour(0xff3a414c));
            g.drawRect(getLocalBounds());
            g.setColour(juce::Colour(0xff7e8794));
            g.setFont(juce::FontOptions(11.0f));
            g.drawText(text, getLocalBounds(), juce::Justification::centred);
        }
        juce::String text;
    };
}

// SynthyLookAndFeel now lives in Source/UI/rack/SynthyLookAndFeel.{h,cpp} (AD-7) —
// the rack framework owns the single shared look.

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

// --- EnvelopeDisplay ---

void EnvelopeDisplay::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(2.0f);
    const float left = b.getX(), right = b.getRight();
    const float top = b.getY(), bottom = b.getBottom();
    const float w = b.getWidth(), h = b.getHeight();

    const float a = pA ? pA->load() : 0.0f;
    const float d = pD ? pD->load() : 0.0f;
    const float s = juce::jlimit(0.0f, 1.0f, pS ? pS->load() : 0.0f);
    const float r = pR ? pR->load() : 0.0f;

    // A fixed-width sustain hold; the rest of the width is split between A/D/R
    // proportional to their durations (so the SHAPE always fills the strip).
    const float sustainW = w * 0.22f;
    const float adrW = w - sustainW;
    const float sum = a + d + r;
    const float aw = (sum < 1.0e-4f) ? adrW / 3.0f : adrW * (a / sum);
    const float dw = (sum < 1.0e-4f) ? adrW / 3.0f : adrW * (d / sum);
    const float rw = (sum < 1.0e-4f) ? adrW / 3.0f : adrW * (r / sum);

    const float susY = bottom - s * h;
    const float xPeak = left + aw;
    const float xSusStart = xPeak + dw;
    const float xSusEnd = xSusStart + sustainW;
    const float xEnd = xSusEnd + rw;

    juce::Path curve;
    curve.startNewSubPath(left, bottom);   // note-on at zero
    curve.lineTo(xPeak, top);              // attack → peak
    curve.lineTo(xSusStart, susY);         // decay → sustain level
    curve.lineTo(xSusEnd, susY);           // sustain hold
    curve.lineTo(xEnd, bottom);            // release → zero

    // Soft fill under the curve, then the stroked line on top.
    juce::Path fill = curve;
    fill.lineTo(left, bottom);
    fill.closeSubPath();
    g.setColour(col.withAlpha(0.14f));
    g.fillPath(fill);

    g.setColour(col.withAlpha(0.9f));
    g.strokePath(curve, juce::PathStrokeType(1.6f));

    // Baseline.
    g.setColour(col.withAlpha(0.25f));
    g.drawLine(left, bottom, right, bottom, 1.0f);
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
      adsrEnvDisplay(p.getAPVTS(), juce::Colour(0xff4ade80)),
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
    addAndMakeVisible(adsrEnvDisplay);   // ADSR curve preview (above the knobs)

    auto green = juce::Colour(0xff4ade80);
    setupKnob(attackKnob, atkLabel, "ATK").setColour(juce::Slider::thumbColourId, green);
    attackKnob.setColour(juce::Slider::rotarySliderFillColourId, green);
    setupKnob(decayKnob, decLabel, "DEC").setColour(juce::Slider::thumbColourId, green);
    decayKnob.setColour(juce::Slider::rotarySliderFillColourId, green);
    setupKnob(sustainKnob, susLabel, "SUS").setColour(juce::Slider::thumbColourId, green);
    sustainKnob.setColour(juce::Slider::rotarySliderFillColourId, green);
    setupKnob(releaseKnob, relLabel, "REL").setColour(juce::Slider::thumbColourId, green);
    releaseKnob.setColour(juce::Slider::rotarySliderFillColourId, green);

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

    filterType.addItemList({"Lowpass", "Highpass"}, 1);   // "Off" is now the separate filterOn enable
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
    // The cutoff default is TYPE-DEPENDENT: a Lowpass wants a moderate cutoff
    // (550 Hz = warm, body kept) while a Highpass wants a low one (150 Hz = just
    // de-rumble, sound kept). Reso resets normally; cutoff is set in the afterFn.
    initResetButton(filterResetBtn, orange, {"filterReso"}, [this]
    {
        auto& a = processor.getAPVTS();
        const bool highpass = (int) *a.getRawParameterValue("filterType") == 1;  // 0 LP, 1 HP
        if (auto* p = a.getParameter("filterCutoff"))
            p->setValueNotifyingHost(p->convertTo0to1(highpass ? 150.0f : 550.0f));
    });

    // Distortion (inline)
    auto distRed = juce::Colour(0xffef4444);
    distTitle.setText("DISTORTION", juce::dontSendNotification);
    distTitle.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    distTitle.setColour(juce::Label::textColourId, distRed);
    distTitle.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(distTitle);

    distTypeSelector.addItemList({"Soft Clip", "Hard Clip", "Foldback"}, 1);   // "Off" is now the separate distortionOn enable
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

    lfoTargetSelector.addItemList({"Frequency", "Amplitude", "Filter Cutoff"}, 1);   // "Off" is now the separate lfoOn enable
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

    // Master + Stereo are no longer header chrome: they live as rack modules in the
    // MASTER BUS zone (see buildSampleRack), so the old inline header controls were
    // removed (PROTOTYPE for FR14 → rack modules; to be formalised via correct-course).

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
    presetNameLabel.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    presetNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaab3c0));
    presetNameLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(presetNameLabel);
    setPresetName(processor.getCurrentPresetName());   // restored from LiveState

    // Noise
    auto noiseGrey = juce::Colour(0xff9ca3af);
    noiseTitle.setText("NOISE", juce::dontSendNotification);
    noiseTitle.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    noiseTitle.setColour(juce::Label::textColourId, noiseGrey);
    noiseTitle.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(noiseTitle);

    noiseTypeSelector.addItemList({"White", "Pink"}, 1);   // "Off" is now the separate noiseOn enable
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
    // Story 1.3: stand up the sample rack BEFORE dropFocus so its controls are also
    // excluded from grabbing keyboard focus.
    buildSampleRack();

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

    // setSize must be LAST so resized() sees all components. Story 1.3: width stays at
    // the original 1520; the HEIGHT is derived from the rack's actual content so the
    // full rack (incl. the Scope + Spectrum L displays at the bottom) always fits
    // without scrolling. The auto-fit below scales the whole editor down on smaller
    // displays. (kBodyTop/kBodyBottom mirror the bands reserved in resized().)
    constexpr int kDesignW   = 1520;
    constexpr int kBodyTop    = 72;   // header row + gap (matches resized())
    constexpr int kBodyBottom = 72;   // keyboard band (matches resized())
    constexpr int kMargin     = 12;   // getLocalBounds().reduced(12)
    const int rackW = kDesignW - 2 * kMargin;
    const int rackH = sampleRack ? sampleRack->preferredHeight(rackW) : 800;
    const int kDesignH = juce::jmax(1015, rackH + kBodyTop + kBodyBottom + 2 * kMargin);
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
    // LFO targets (0 Frequency, 1 Amplitude, 2 FilterCutoff), but only when the LFO is
    // enabled (lfoOn). Only ANIMATE knobs whose module is actually active — a disabled
    // OSC or a bypassed filter isn't sounding, so showing a moving ring there is misleading.
    auto& apvts = processor.getAPVTS();
    float lfo = processor.getLfoDisplayValue();
    bool lfoActive = *apvts.getRawParameterValue("lfoOn") > 0.5f;
    int target = (int) *apvts.getRawParameterValue("lfoTarget");
    bool freqT = lfoActive && (target == 0), ampT = lfoActive && (target == 1);

    auto applyOsc = [&](OscillatorPanel& osc, const char* onId)
    {
        bool on = *apvts.getRawParameterValue(onId) > 0.5f;
        osc.setFreqMod((freqT && on) ? lfo : 0.0f);
        osc.setAmpMod ((ampT  && on) ? lfo : 0.0f);
    };
    applyOsc(osc1, "osc1On");
    applyOsc(osc2, "osc2On");
    applyOsc(osc3, "osc3On");

    bool filterOn = *apvts.getRawParameterValue("filterOn") > 0.5f;
    cutoffKnob.setModAmount((lfoActive && target == 2 && filterOn) ? lfo : 0.0f);

    // Same live feed drives the new rack (AD-8): ONE timer, rack fans out to its frames.
    // ModTarget has a +1 offset vs the raw lfoTarget (ModTarget::None = 0; raw 0 = Frequency).
    if (sampleRack)
    {
        const rack::ModTarget activeT = lfoActive ? static_cast<rack::ModTarget>(target + 1)
                                                  : rack::ModTarget::None;
        sampleRack->updateLiveFeed(lfoActive, activeT, lfo, ratio);
    }

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
    // Spacebar re-plucks the Karplus string. Trigger the actual PLUCK button so it
    // visibly presses too (its onClick calls pluckString); fall back to the direct call.
    if (key == juce::KeyPress::spaceKey)
    {
        if (auto* f = sampleRack ? sampleRack->moduleById("stringkarplus") : nullptr)
            f->clickFirstAction();
        else
            processor.pluckString();
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
        auto subArea = titleArea.removeFromBottom(20);
        g.setFont(juce::FontOptions(26.0f, juce::Font::bold));
        g.setColour(juce::Colour(0xff40c0ff));
        g.drawText("J A S S", titleArea, juce::Justification::centred);
        g.setFont(juce::FontOptions(13.0f));
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

void SynthyEditor::buildSampleRack()
{
    // TEMP (Story 1.3): a throwaway population to verify the grid engine, zone headers
    // and shared look at the fixed 1920×1200 target. It mirrors the mockup census
    // (≈10×S, 6×M, 4×L) and binds REAL Parameters::ID values so the frames' APVTS
    // attachments resolve. Story 1.5 replaces this with the real module descriptors.
    using namespace rack;
    auto& apvts = processor.getAPVTS();
    // MASTER BUS is the top row (first zone), then the three main zones below it.
    sampleRack = std::make_unique<Rack>(apvts, Rack::kDefaultCols,
        std::vector<Rack::Zone>{ Rack::Zone::MasterBus, Rack::Zone::Generators,
                                 Rack::Zone::Modulation, Rack::Zone::Processing });

    namespace P = Parameters::ID;

    // small builders to keep the descriptor list readable
    auto K = [](juce::String id, juce::String lbl) { return Knob{ std::move(id), std::move(lbl) }; };
    auto C = [](juce::String id, juce::String lbl, juce::StringArray items)
             { return Combo{ std::move(id), std::move(lbl), std::move(items) }; };
    // Story 1.4 (verification wiring; folded into the real descriptors in 1.5/2.2):
    // a knob tagged as an LFO ring target …
    auto Kmod = [](juce::String id, juce::String lbl, ModTarget mt)
             { Knob k{ std::move(id), std::move(lbl) }; k.modTarget = mt; return k; };
    // … and a FREQ knob with the played-frequency display transform (base × ratio).
    auto Kfreq = [](juce::String id, juce::String lbl)
             {
                 Knob k{ std::move(id), std::move(lbl) };
                 k.modTarget    = ModTarget::Frequency;
                 k.toDisplay    = [](double base,  double ratio) { return base  * ratio; };
                 k.fromDisplay  = [](double shown, double ratio) { return shown / ratio; };
                 return k;
             };

    auto add = [&](Rack::Zone zone, SizeClass sc, ModuleType type, juce::String title,
                   juce::String enableParam, std::vector<BodyElement> body)
    {
        ModuleDescriptor d;
        d.sizeClass = sc; d.type = type;
        // Stable slug from the title (e.g. "OSC 1" -> "osc1") — the future layout key for
        // show/hide + drag-drop (ARCHITECTURE Deferred). Derived once here so every module gets one.
        d.id = title.toLowerCase().retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789");
        d.title = std::move(title);
        d.enableParam = std::move(enableParam); d.body = std::move(body);
        sampleRack->addModule(zone, std::move(d));
    };
    auto display = [&](juce::String label, int slots)
    {
        auto* c = sampleOwned.add(new SampleDisplayPlaceholder(std::move(label)));
        return Display{ c, slots };
    };

    // OSC WAVE items MUST match the oscWave param's choice ORDER — the ComboBoxAttachment
    // maps by index, so a different order mislabels every waveform (same class of bug as the
    // LFO-WAVE fix in Story 2.1). oscWave = { Sine, Sawtooth, Square, Triangle } (Parameters.h).
    const juce::StringArray waves { "Sine", "Sawtooth", "Square", "Triangle" };

    // ---- MASTER BUS (top row; PROTOTYPE: decisions A+B) ----
    // Stereo becomes a normal module whose Enable IS stereoOn (no special-case header
    // chrome); Master is the new XS class (a single knob). Demonstrates "everything is a
    // module" before we formalise FR14 / the XS size class via correct-course.
    add(Rack::Zone::MasterBus, SizeClass::XS, ModuleType::Processor, "STEREO", P::stereoOn,
        { K(P::stereoWidth, "WIDTH"), K(P::stereoTime, "TIME") });
    add(Rack::Zone::MasterBus, SizeClass::XXS, ModuleType::Processor, "MASTER", P::masterOn,
        { K(P::masterVol, "VOL") });

    // ---- GENERATORS ----
    auto addOsc = [&](int i)
    {
        add(Rack::Zone::Generators, SizeClass::S, ModuleType::Generator,
            "OSC " + juce::String(i), P::oscOn(i),
            { C(P::oscWave(i), "WAVE", waves), Kfreq(P::oscFreq(i), "FREQ"),
              Kmod(P::oscAmp(i), "AMP", ModTarget::Amplitude), K(P::oscUniVoices(i), "VOICES"),
              K(P::oscUniDetune(i), "DETUNE") });
    };
    // MIX MODE (XS, half-width) sits BETWEEN OSC 1 and OSC 2 — it couples OSC 1<->2, so it
    // reads as the connector between them. Row-major packing then puts OSC 3 on row 2 and
    // (after Sub+Noise) Karplus on row 3.
    addOsc(1);
    {
        // MIX MODE couples OSC1<->OSC2 (it sits between them). It is only meaningful when
        // BOTH are on, so it shows as active/lit only then and dims otherwise. The derived
        // enable reads the shared osc1On/osc2On params (AD-9) — it holds NO reference to the
        // OSC modules; the atomics are grabbed once (stable for the APVTS lifetime).
        auto* o1 = apvts.getRawParameterValue (P::oscOn (1));
        auto* o2 = apvts.getRawParameterValue (P::oscOn (2));
        ModuleDescriptor mix;
        mix.sizeClass = SizeClass::XXS; mix.type = ModuleType::Generator;
        mix.id = "mixmode"; mix.title = "MIX MODE";
        mix.enableParam = P::mixModeOn;   // real user enable (off => additive, Story 2.4)
        mix.body = { C(P::mixMode, "MODE", { "Additive", "RingMod", "FM" }) };
        // Effective lit = mixModeOn AND (osc1 && osc2): the interactive toggle is the user's
        // enable; the predicate additionally dims when the coupling is meaningless (a UI cue,
        // not an audio gate — the audio additive-fallback keys off mixModeOn only).
        mix.enabledWhen = [o1, o2] { return o1->load() >= 0.5f && o2->load() >= 0.5f; };
        sampleRack->addModule(Rack::Zone::Generators, std::move(mix));
    }
    addOsc(2);
    addOsc(3);

    add(Rack::Zone::Generators, SizeClass::S, ModuleType::Generator, "SUB", P::subOn,
        { C(P::subWave, "WAVE", { "Sine", "Square" }), K(P::subLevel, "LEVEL") });
    add(Rack::Zone::Generators, SizeClass::S, ModuleType::Generator, "NOISE", P::noiseOn,
        { C(P::noiseType, "TYPE", { "White", "Pink" }), K(P::noiseAmp, "AMP") });
    add(Rack::Zone::Generators, SizeClass::M, ModuleType::Generator, "STRING - KARPLUS", P::karplusOn,
        { Action{ "PLUCK", [this] { processor.pluckString(); }, {} },
          K(P::karplusFreq, "FREQ"), K(P::karplusAmp, "AMP"),
          K(P::karplusDamping, "DAMP"), K(P::karplusStretch, "STR") });
    add(Rack::Zone::Generators, SizeClass::M, ModuleType::Generator, "WAVETABLE", P::wavetableOn,
        { Combo{ P::wavetableBank, "BANK",
                 std::function<juce::StringArray()>([] { return WavetableBankStore::instance().getNames(); }) },
          FileAction{ "LOAD WAV",
                      [this] (juce::File f)
                      {
                          int idx = WavetableBankStore::instance().loadWav(f);
                          if (idx >= 0)
                              if (auto* pr = processor.getAPVTS().getParameter(P::wavetableBank))
                                  pr->setValueNotifyingHost(pr->convertTo0to1((float) idx));
                      },
                      { juce::String(P::wavetableBank) } },   // refresh the BANK combo after load
          K(P::wavetablePosition, "POS"), K(P::wavetableFreq, "FREQ"), K(P::wavetableAmp, "AMP"),
          K(P::wavetableUniVoices, "VOICES"), K(P::wavetableUniDetune, "DETUNE") });

    // ---- MODULATION ----
    // ADSR: the second unit-row is the REAL EnvelopeDisplay (attack→decay→sustain→release
    // curve), a Display body element (AD-5). Owned by sampleOwned like the placeholders so
    // its lifetime matches the existing pattern; a separate instance from the legacy
    // adsrEnvDisplay (a Component has only one parent, and the legacy panel still owns that).
    add(Rack::Zone::Modulation, SizeClass::L, ModuleType::Modulator, "ENVELOPE - ADSR", P::adsrOn,
        { K(P::attack, "ATK"), K(P::decay, "DEC"), K(P::sustain, "SUS"), K(P::release, "REL"),
          Display{ sampleOwned.add(new EnvelopeDisplay(apvts, juce::Colour(0xff22d3ee))), 4 } });
    // LFO WAVE must list the lfoWave param's OWN choices in order — the ComboBoxAttachment
    // maps by index, so the shared `waves` array (a different order) would mislabel every
    // waveform (Story 2.1 AC3).
    add(Rack::Zone::Modulation, SizeClass::M, ModuleType::Modulator, "LFO", P::lfoOn,
        { C(P::lfoWave, "WAVE", { "Sine", "Triangle", "Square", "Sawtooth" }),
          C(P::lfoTarget, "TARGET", { "Frequency", "Amplitude", "Filter Cutoff" }),
          K(P::lfoRate, "RATE"), K(P::lfoDepth, "DEPTH") });
    add(Rack::Zone::Modulation, SizeClass::M, ModuleType::Modulator, "ARPEGGIATOR", P::arpOn,
        { C(P::arpMode, "MODE", { "Up", "Down", "UpDown", "Random" }),
          K(P::arpRate, "RATE"), K(P::arpOctaves, "OCT"), K(P::arpGate, "GATE") });

    // ---- PROCESSING ----
    // FILTER: TYPE combo + CUTOFF + RESO (= 4 slots, like DISTORTION) → M (4 cols) so the
    // combo isn't cramped. (Exact width tuning deferred to next session.)
    add(Rack::Zone::Processing, SizeClass::M, ModuleType::Processor, "FILTER", P::filterOn,
        { C(P::filterType, "TYPE", { "Lowpass", "Highpass" }),
          Kmod(P::filterCutoff, "CUTOFF", ModTarget::FilterCutoff), K(P::filterReso, "RESO") });
    // M-class so the TYPE combo (2 slots) fits alongside DRIVE + MIX.
    // DISTORTION TYPE: display text is cosmetic ("Soft Clip"/"Hard Clip" read better) — the
    // ComboBoxAttachment maps by INDEX, so the canonical param/.synthy strings stay
    // "SoftClip"/"HardClip" (project-context: UI display may differ from the interop string).
    // Order/count MUST match distortionType's choices exactly, or the index mapping breaks.
    add(Rack::Zone::Processing, SizeClass::M, ModuleType::Processor, "DISTORTION", P::distortionOn,
        { C(P::distortionType, "TYPE", { "Soft Clip", "Hard Clip", "Foldback" }),
          K(P::distortionDrive, "DRIVE"), K(P::distortionMix, "MIX") });
    add(Rack::Zone::Processing, SizeClass::S, ModuleType::Processor, "WAVEFOLD", P::wavefoldOn,
        { K(P::wavefoldDrive, "DRIVE"), K(P::wavefoldSymmetry, "SYM"), K(P::wavefoldMix, "MIX") });
    add(Rack::Zone::Processing, SizeClass::S, ModuleType::Processor, "BITCRUSH", P::bitcrushOn,
        { K(P::bitcrushBits, "BITS"), K(P::bitcrushRate, "RATE"), K(P::bitcrushMix, "MIX") });
    add(Rack::Zone::Processing, SizeClass::S, ModuleType::Processor, "CHORUS", P::chorusOn,
        { K(P::chorusRate, "RATE"), K(P::chorusDepth, "DEPTH"), K(P::chorusMix, "MIX") });
    add(Rack::Zone::Processing, SizeClass::S, ModuleType::Processor, "DELAY", P::delayOn,
        { K(P::delayTime, "TIME"), K(P::delayFeedback, "FB"), K(P::delayMix, "MIX") });
    add(Rack::Zone::Processing, SizeClass::S, ModuleType::Processor, "REVERB", P::reverbOn,
        { K(P::reverbRoom, "ROOM"), K(P::reverbDamp, "DAMP"), K(P::reverbMix, "MIX") });
    add(Rack::Zone::Processing, SizeClass::XL, ModuleType::Processor, "OSCILLOSCOPE", {},
        { display("SCOPE", 12) });
    add(Rack::Zone::Processing, SizeClass::XL, ModuleType::Processor, "SPECTRUM", {},
        { display("SPECTRUM", 12) });

    // Added LAST so the opaque rack covers the legacy body; the header chrome and
    // keyboard sit in their own bands and stay live.
    addAndMakeVisible(*sampleRack);
}

void SynthyEditor::resized()
{
    auto area = getLocalBounds().reduced(12);

    // ===== Header — Save/Load/Random/Reset left, Title + Current State center =====
    // (Master + Stereo moved out of the header into the MASTER BUS rack zone.) The header
    // is kept compact; the freed space gives the title + preset name room to breathe.
    auto headerRow = area.removeFromTop(64);
    // The title is centred over the FULL header width so "J A S S" sits in the true middle
    // of the window; the left cluster only overlays the left edge, clear of the centred text.
    g_titleBounds = headerRow;
    // Left cluster: the Save/Load/Random/Reset buttons AND the current-preset name belong
    // together (the preset name is about what was loaded/saved). Buttons in a 2x2 block
    // with "Current State" beside them.
    auto leftGroup = headerRow.removeFromLeft(340);
    auto leftBtns = leftGroup.removeFromLeft(150);
    auto row1 = leftBtns.removeFromTop(30);
    saveBtn.setBounds(row1.removeFromLeft(row1.getWidth() / 2).reduced(3, 2));
    loadBtn.setBounds(row1.reduced(3, 2));
    auto row2 = leftBtns.removeFromTop(30);
    randomBtn.setBounds(row2.removeFromLeft(row2.getWidth() / 2).reduced(3, 2));
    resetBtn.setBounds(row2.reduced(3, 2));
    leftGroup.removeFromLeft(10);
    presetNameLabel.setBounds(leftGroup);   // grouped with the load/save controls
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

    // Rack grid: each tower stacks "inserts" whose height is a multiple of one
    // rack unit U. Normal module = 1U, focal (OSC row, ADSR) = 1.5U.
    const int U   = 130;        // one rack unit (Bauhöhe)
    const int U1  = U;          // 1   U
    const int U15 = (U * 3) / 2; // 1.5 U = 195

    // ============================================================
    // LEFT COLUMN — ZONE 1: GENERATORS (sound sources)
    // ============================================================
    genHeaderBounds = leftCol.removeFromTop(24);
    leftCol.removeFromTop(2);

    // Oscillators with MIX MODE wired between OSC 1 & 2, and "+" before OSC 3.
    auto oscRow = leftCol.removeFromTop(U15);   // 1.5 U (focal)
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
    auto genRow = leftCol.removeFromTop(U1);    // 1 U
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
    auto wtRow = leftCol.removeFromTop(U1).reduced(3, 0);   // 1 U
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

    auto modRow = rightCol.removeFromTop(U15);   // 1.5 U (focal: ADSR + LFO)

    // ADSR (left half): title + curve preview + 4 knobs.
    auto adsrArea = modRow.removeFromLeft(modRow.getWidth() / 2).reduced(3);
    adsrBounds = adsrArea;
    auto adsrTitleRow = adsrArea.removeFromTop(20);
    adsrResetBtn.setBounds(adsrTitleRow.removeFromRight(22).reduced(1));
    adsrTitle.setBounds(adsrTitleRow);
    adsrEnvDisplay.setBounds(adsrArea.removeFromTop(40).reduced(2, 1));
    adsrArea.removeFromTop(2);
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

    // LFO (right half, full height) — knobs at the standard size.
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

    // Arpeggiator: a single (half-width) field; the right half stays free.
    auto arpRow = rightCol.removeFromTop(U1);    // 1 U
    auto arpArea = arpRow.removeFromLeft(arpRow.getWidth() / 2).reduced(3);
    arpBounds = arpArea;
    auto arpTop = arpArea.removeFromTop(22);
    arpEnableBtn.setBounds(arpTop.removeFromLeft(50));
    arpResetBtn.setBounds(arpTop.removeFromRight(22).reduced(1));
    arpTitle.setBounds(arpTop.removeFromLeft(150));
    arpArea.removeFromTop(2);
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

    // Filter | Distortion — knobs at the standard size.
    auto fdRow = rightCol.removeFromTop(U1);     // 1 U
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
    auto fxRow = rightCol.removeFromTop(U1);     // 1 U
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

    // Story 1.3: the sample rack covers the body band (below the header row, above the
    // keyboard). It is opaque, so it hides the legacy panels beneath while the header
    // chrome and keyboard keep their own bands. (Real descriptors replace it in 1.5.)
    if (sampleRack)
    {
        auto rb = getLocalBounds().reduced(12);
        rb.removeFromTop(64 + 8);    // header row + gap (mirrors the header band above)
        rb.removeFromBottom(72);     // keyboard band
        sampleRack->setBounds(rb);
    }
}
