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
    addAndMakeVisible(freqKnob);
    freqLabel.setText("FREQ", juce::dontSendNotification);
    freqLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(freqLabel);
    freqAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, prefix + "Freq", freqKnob);

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

void OscillatorPanel::resized()
{
    auto area = getLocalBounds().reduced(6);
    auto titleRow = area.removeFromTop(22);
    enableBtn.setBounds(titleRow.removeFromLeft(28));
    title.setBounds(titleRow);
    waveSelector.setBounds(area.removeFromTop(24).reduced(20, 0));
    area.removeFromTop(4);

    // 2×2 knob grid: FREQ | AMP (top), VOICES | DETUNE (bottom)
    auto topRow = area.removeFromTop(area.getHeight() / 2);
    auto botRow = area;

    auto tl = topRow.removeFromLeft(topRow.getWidth() / 2);
    freqLabel.setBounds(tl.removeFromTop(14));
    freqKnob.setBounds(tl);
    ampLabel.setBounds(topRow.removeFromTop(14));
    ampKnob.setBounds(topRow);

    auto bl = botRow.removeFromLeft(botRow.getWidth() / 2);
    uniVoicesLabel.setBounds(bl.removeFromTop(14));
    uniVoicesKnob.setBounds(bl);
    uniDetuneLabel.setBounds(botRow.removeFromTop(14));
    uniDetuneKnob.setBounds(botRow);
}

// --- EffectPanel ---

EffectPanel::EffectPanel(juce::AudioProcessorValueTreeState& apvts,
                          const juce::String& name, juce::Colour color,
                          const juce::String& onParam,
                          const juce::StringArray& knobParams,
                          const juce::StringArray& knobLabels)
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
                   {"FREQ", "AMP", "DAMP", "STR"})
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

    // Mix mode
    auto mixGold = juce::Colour(0xfffacc15);
    mixModeTitle.setText("MIX MODE", juce::dontSendNotification);
    mixModeTitle.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    mixModeTitle.setColour(juce::Label::textColourId, mixGold);
    mixModeTitle.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(mixModeTitle);

    mixModeSelector.addItemList({"Additive", "Ring Mod", "FM"}, 1);
    addAndMakeVisible(mixModeSelector);
    mixModeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        p.getAPVTS(), "mixMode", mixModeSelector);

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
        });
    };

    addAndMakeVisible(randomBtn);
    randomBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff6d28d9));
    randomBtn.onClick = [this] { processor.randomize(); };

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

    // setSize must be LAST so resized() sees all components
    setSize(820, 1200);
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

    drawSection(osc1.getBounds().expanded(2));
    drawSection(osc2.getBounds().expanded(2));
    drawSection(osc3.getBounds().expanded(2));

    // Modulation grid backgrounds
    drawSection(adsrBounds.expanded(2));
    drawSection(lfoBounds.expanded(2));
    drawSection(filterBounds.expanded(2));
    drawSection(distBounds.expanded(2));

    // Effect backgrounds
    drawSection(delayPanel.getBounds().expanded(2));
    drawSection(chorusPanel.getBounds().expanded(2));
    drawSection(reverbPanel.getBounds().expanded(2));

    // Generators
    drawSection(noiseBounds.expanded(2));
    drawSection(karplusPanel.getBounds().expanded(2));
    drawSection(wtBounds.expanded(2));
}

void SynthyEditor::resized()
{
    auto area = getLocalBounds().reduced(12);

    // ===== Row 0: Header — Save/Load left, Title center, Master right =====
    auto headerRow = area.removeFromTop(80);

    // Save/Load (top row) + Randomize (below) on the left
    auto leftBtns = headerRow.removeFromLeft(120);
    auto topBtns = leftBtns.removeFromTop(34);
    saveBtn.setBounds(topBtns.removeFromLeft(topBtns.getWidth() / 2).reduced(3, 3));
    loadBtn.setBounds(topBtns.reduced(3, 3));
    randomBtn.setBounds(leftBtns.removeFromTop(34).reduced(3, 3));

    // Master on the right
    auto masterArea = headerRow.removeFromRight(100);
    masterLabel.setBounds(masterArea.removeFromTop(14));
    masterKnob.setBounds(masterArea);

    // Title centered in the remaining middle
    g_titleBounds = headerRow;

    area.removeFromTop(6);

    // ===== Row 1: Oscillators (each: wave + FREQ/AMP/VOICES/DETUNE) =====
    auto oscRow = area.removeFromTop(210);
    int oscW = oscRow.getWidth() / 3;
    osc1.setBounds(oscRow.removeFromLeft(oscW).reduced(3));
    osc2.setBounds(oscRow.removeFromLeft(oscW).reduced(3));
    osc3.setBounds(oscRow.reduced(3));

    area.removeFromTop(6);

    // ===== Row 2: Mix Mode (centered) =====
    auto mixRow = area.removeFromTop(54).reduced(3, 0);
    mixModeTitle.setBounds(mixRow.removeFromTop(20));
    mixModeSelector.setBounds(mixRow.withSizeKeepingCentre(220, 26));

    area.removeFromTop(6);

    // ===== Row 3: Modulation grid — ADSR+LFO left | Filter+Distortion right =====
    auto modRow = area.removeFromTop(260);
    auto modLeft = modRow.removeFromLeft(modRow.getWidth() / 2);
    auto modRight = modRow;

    // --- Left column: ADSR (top) + LFO (bottom) ---
    auto adsrArea = modLeft.removeFromTop(modLeft.getHeight() / 2).reduced(3);
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

    auto lfoArea = modLeft.reduced(3);
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

    // --- Right column: Filter (top) + Distortion (bottom) ---
    auto filterArea = modRight.removeFromTop(modRight.getHeight() / 2).reduced(3);
    filterBounds = filterArea;

    filterTitle.setBounds(filterArea.removeFromTop(20));
    filterType.setBounds(filterArea.removeFromTop(24).reduced(20, 0));
    filterArea.removeFromTop(4);
    auto fLeft = filterArea.removeFromLeft(filterArea.getWidth() / 2);
    cutLabel.setBounds(fLeft.removeFromTop(14));
    cutoffKnob.setBounds(fLeft);
    resoLabel.setBounds(filterArea.removeFromTop(14));
    resoKnob.setBounds(filterArea);

    // Distortion (inline) — bottom of the right modulation column
    auto distArea = modRight.reduced(3);
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

    // ===== Row 4: Effects — Delay | Chorus | Reverb =====
    auto fxRow = area.removeFromTop(120);
    int fxW = fxRow.getWidth() / 3;
    delayPanel.setBounds(fxRow.removeFromLeft(fxW).reduced(3));
    chorusPanel.setBounds(fxRow.removeFromLeft(fxW).reduced(3));
    reverbPanel.setBounds(fxRow.reduced(3));

    area.removeFromTop(6);

    // ===== Row 4.5: Generators — Noise | Karplus | Wavetable =====
    auto genRow = area.removeFromTop(100);
    int genW = genRow.getWidth() / 3;

    auto noiseArea = genRow.removeFromLeft(genW).reduced(3);
    noiseBounds = noiseArea;
    noiseTitle.setBounds(noiseArea.removeFromTop(20));
    noiseTypeSelector.setBounds(noiseArea.removeFromTop(24).reduced(20, 0));
    noiseArea.removeFromTop(4);
    noiseAmpLabel.setBounds(noiseArea.removeFromTop(14));
    noiseAmpKnob.setBounds(noiseArea.withSizeKeepingCentre(60, noiseArea.getHeight()));

    // Karplus-Strong takes the remaining two columns (it has 4 knobs)
    karplusPanel.setBounds(genRow.reduced(3));

    area.removeFromTop(6);

    // ===== Row 4.6: Wavetable =====
    auto wtRow = area.removeFromTop(110).reduced(3, 0);
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

    area.removeFromTop(6);

    // ===== Row 5: Visualization — Oscilloscope | Spectrum =====
    auto vizRow = area.removeFromTop(150).reduced(3, 0);
    if (waveformDisplay)
        waveformDisplay->setBounds(vizRow.removeFromLeft(vizRow.getWidth() / 2).withTrimmedRight(3));
    if (spectrumDisplay)
        spectrumDisplay->setBounds(vizRow.withTrimmedLeft(3));
}
