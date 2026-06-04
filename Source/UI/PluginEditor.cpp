#include "PluginEditor.h"
#include "../DSP/WavetableBank.h"
#include "../Audio/PresetIO.h"

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
                          const juce::String& triggerText)
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
}

void EffectPanel::resized()
{
    auto area = getLocalBounds().reduced(4);
    auto top = area.removeFromTop(22);
    enableBtn.setBounds(top.removeFromLeft(50));
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

SynthyEditor::SynthyEditor(SynthyProcessor& p)
    : AudioProcessorEditor(&p), processor(p),
      osc1(p.getAPVTS(), 1, juce::Colour(0xff40c0ff)),
      osc2(p.getAPVTS(), 2, juce::Colour(0xffff6b9d)),
      osc3(p.getAPVTS(), 3, juce::Colour(0xffc084fc)),
      delayPanel(p.getAPVTS(), "DELAY", juce::Colour(0xff38bdf8),
                 "delayOn", {"delayTime", "delayFeedback", "delayMix"}, {"TIME", "FDBK", "MIX"}),
      chorusPanel(p.getAPVTS(), "CHORUS", juce::Colour(0xffa78bfa),
                  "chorusOn", {"chorusRate", "chorusDepth", "chorusMix"}, {"RATE", "DEPTH", "MIX"}),
      reverbPanel(p.getAPVTS(), "REVERB", juce::Colour(0xfffb7185),
                  "reverbOn", {"reverbRoom", "reverbDamp", "reverbMix"}, {"ROOM", "DAMP", "MIX"}),
      karplusPanel(p.getAPVTS(), "STRING (KARPLUS)", juce::Colour(0xff34d399),
                   "karplusOn", {"karplusFreq", "karplusAmp", "karplusDamping", "karplusStretch"},
                   {"FREQ", "AMP", "DAMP", "STR"}),
      wavefoldPanel(p.getAPVTS(), "WAVEFOLD", juce::Colour(0xfffbbf24),
                    "wavefoldOn", {"wavefoldDrive", "wavefoldSymmetry", "wavefoldMix"},
                    {"DRIVE", "SYM", "MIX"}),
      bitcrushPanel(p.getAPVTS(), "BITCRUSH", juce::Colour(0xff2dd4bf),
                    "bitcrushOn", {"bitcrushBits", "bitcrushRate", "bitcrushMix"},
                    {"BITS", "RATE", "MIX"})
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

    // Master
    auto gold = juce::Colour(0xffffd700);
    setupKnob(masterKnob, masterLabel, "MASTER").setColour(juce::Slider::thumbColourId, gold);
    masterKnob.setColour(juce::Slider::rotarySliderFillColourId, gold);
    masterAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.getAPVTS(), "masterVol", masterKnob);

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

    // On-screen keyboard (shares the processor's MidiKeyboardState → plays the
    // active generators with full ADSR per note, transposed relative to C4).
    keyboard = std::make_unique<juce::MidiKeyboardComponent>(
        processor.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard);
    keyboard->setAvailableRange(36, 96);   // C2 .. C7
    keyboard->setKeyWidth(20.0f);
    keyboard->setKeyPressBaseOctave(kbBaseOctave);
    keyboard->setMidiChannelsToDisplay(1);   // only highlight played (ch.1) notes, not the ch.16 drone
    // Allow playing via the computer keyboard (a, w, s, e, d, ... map to notes;
    // z / x shift the octave; the keyboard must have focus — grabbed on launch/click).
    keyboard->setWantsKeyboardFocus(true);
    addAndMakeVisible(*keyboard);
    juce::Component::SafePointer<juce::MidiKeyboardComponent> kbPtr(keyboard.get());
    juce::MessageManager::callAsync([kbPtr]() mutable { if (kbPtr) kbPtr->grabKeyboardFocus(); });

    // Keep keyboard focus available so z / x (octave shift) reach keyPressed.
    setWantsKeyboardFocus(true);

    // setSize must be LAST so resized() sees all components
    setSize(820, 1330);

    // Drive the OSC FREQ-knob display (played frequency).
    startTimerHz(30);
}

void SynthyEditor::timerCallback()
{
    double ratio = processor.getCurrentNoteRatio();
    osc1.setPlayedRatio(ratio);
    osc2.setPlayedRatio(ratio);
    osc3.setPlayedRatio(ratio);

    // The shared LiveState is re-loaded asynchronously after construction; keep
    // the header label in sync with the processor's (restored) preset name.
    if (auto pn = processor.getCurrentPresetName(); pn != shownPresetName)
    {
        shownPresetName = pn;
        presetNameLabel.setText("Preset: " + pn, juce::dontSendNotification);
    }
}

void SynthyEditor::setPresetName(const juce::String& name)
{
    processor.setCurrentPresetName(name);   // keep the processor (LiveState) in sync
    shownPresetName = name;
    presetNameLabel.setText("Preset: " + name, juce::dontSendNotification);
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

    // Title
    g.setFont(juce::FontOptions(28.0f, juce::Font::bold));
    g.setColour(juce::Colour(0xff40c0ff));
    g.drawText("S Y N T H Y", g_titleBounds, juce::Justification::centred);

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
    masterLabel.setBounds(masterArea.removeFromTop(14));
    masterKnob.setBounds(masterArea);
    // Center: SYNTHY title (top) + current-preset name (below)
    auto centerArea = headerRow;
    g_titleBounds = centerArea.removeFromTop(centerArea.getHeight() - 18);
    presetNameLabel.setBounds(centerArea);
    area.removeFromTop(4);

    // ============================================================
    // ZONE 1: GENERATORS (sound sources)
    // ============================================================
    genHeaderBounds = area.removeFromTop(24);
    area.removeFromTop(2);

    // Oscillators with MIX MODE wired between OSC 1 & 2, and "+" before OSC 3.
    auto oscRow = area.removeFromTop(165);
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
    area.removeFromTop(6);

    // Noise | Sub Osc | Karplus (String)
    auto genRow = area.removeFromTop(150);
    int genW = genRow.getWidth() / 4;

    auto noiseArea = genRow.removeFromLeft(genW).reduced(3);
    noiseBounds = noiseArea;
    noiseTitle.setBounds(noiseArea.removeFromTop(20));
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
    subTitle.setBounds(subTitleRow);
    subWaveSelector.setBounds(subArea.removeFromTop(24).reduced(10, 0));
    subOctaveSelector.setBounds(subArea.removeFromTop(24).reduced(10, 0));
    subArea.removeFromTop(2);
    subLevelLabel.setBounds(subArea.removeFromTop(14));
    subLevelKnob.setBounds(subArea.withSizeKeepingCentre(
        juce::jmin(80, subArea.getWidth()), subArea.getHeight()));

    karplusPanel.setBounds(genRow.reduced(3)); // remaining two columns (4 knobs)
    area.removeFromTop(6);

    // Wavetable (full width)
    auto wtRow = area.removeFromTop(124).reduced(3, 0);
    wtBounds = wtRow;
    auto wtTop = wtRow.removeFromTop(22);
    wtEnableBtn.setBounds(wtTop.removeFromLeft(50));
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
    area.removeFromTop(10);

    // ============================================================
    // ZONE 2: MODULATION (ADSR + LFO)
    // ============================================================
    modHeaderBounds = area.removeFromTop(24);
    area.removeFromTop(2);

    auto modRow = area.removeFromTop(150);
    auto adsrArea = modRow.removeFromLeft(modRow.getWidth() / 2).reduced(3);
    adsrBounds = adsrArea;
    adsrTitle.setBounds(adsrArea.removeFromTop(20));
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
    lfoTitle.setBounds(lfoArea.removeFromTop(20));
    auto lfoSelectors = lfoArea.removeFromTop(28);
    lfoWaveSelector.setBounds(lfoSelectors.removeFromLeft(lfoSelectors.getWidth() / 2).reduced(4, 2));
    lfoTargetSelector.setBounds(lfoSelectors.reduced(4, 2));
    lfoArea.removeFromTop(2);
    auto lfoKnob1 = lfoArea.removeFromLeft(lfoArea.getWidth() / 2);
    lfoRateLabel.setBounds(lfoKnob1.removeFromTop(14));
    lfoRateKnob.setBounds(lfoKnob1);
    lfoDepthLabel.setBounds(lfoArea.removeFromTop(14));
    lfoDepthKnob.setBounds(lfoArea);
    area.removeFromTop(10);

    // ============================================================
    // ZONE 3: PROCESSING (filter + shapers + effects)
    // ============================================================
    procHeaderBounds = area.removeFromTop(24);
    area.removeFromTop(2);

    // Filter | Distortion
    auto fdRow = area.removeFromTop(145);
    auto filterArea = fdRow.removeFromLeft(fdRow.getWidth() / 2).reduced(3);
    filterBounds = filterArea;
    filterTitle.setBounds(filterArea.removeFromTop(20));
    filterType.setBounds(filterArea.removeFromTop(24).reduced(20, 0));
    filterArea.removeFromTop(4);
    auto fLeft = filterArea.removeFromLeft(filterArea.getWidth() / 2);
    cutLabel.setBounds(fLeft.removeFromTop(14));
    cutoffKnob.setBounds(fLeft);
    resoLabel.setBounds(filterArea.removeFromTop(14));
    resoKnob.setBounds(filterArea);

    auto distArea = fdRow.reduced(3);
    distBounds = distArea;
    distTitle.setBounds(distArea.removeFromTop(20));
    distTypeSelector.setBounds(distArea.removeFromTop(24).reduced(20, 0));
    distArea.removeFromTop(4);
    auto dLeft = distArea.removeFromLeft(distArea.getWidth() / 2);
    distDriveLabel.setBounds(dLeft.removeFromTop(14));
    distDriveKnob.setBounds(dLeft);
    distMixLabel.setBounds(distArea.removeFromTop(14));
    distMixKnob.setBounds(distArea);
    area.removeFromTop(6);

    // Wavefold | Bitcrush | Delay | Chorus | Reverb
    auto fxRow = area.removeFromTop(120);
    int fxW = fxRow.getWidth() / 5;
    wavefoldPanel.setBounds(fxRow.removeFromLeft(fxW).reduced(3));
    bitcrushPanel.setBounds(fxRow.removeFromLeft(fxW).reduced(3));
    delayPanel.setBounds(fxRow.removeFromLeft(fxW).reduced(3));
    chorusPanel.setBounds(fxRow.removeFromLeft(fxW).reduced(3));
    reverbPanel.setBounds(fxRow.reduced(3));
    area.removeFromTop(8);

    // ===== Visualization — Oscilloscope | Spectrum =====
    auto vizRow = area.removeFromTop(150).reduced(3, 0);
    if (waveformDisplay)
        waveformDisplay->setBounds(vizRow.removeFromLeft(vizRow.getWidth() / 2).withTrimmedRight(3));
    if (spectrumDisplay)
        spectrumDisplay->setBounds(vizRow.withTrimmedLeft(3));

    area.removeFromTop(6);

    // ===== Keyboard (full width) =====
    auto kbRow = area.removeFromTop(72).reduced(3, 0);
    if (keyboard)
        keyboard->setBounds(kbRow.reduced(2));
}
