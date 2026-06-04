#include "PluginProcessor.h"
#include "UI/PluginEditor.h"
#include "Audio/PresetIO.h"

SynthyProcessor::SynthyProcessor()
    : AudioProcessor(BusesProperties()
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", Parameters::createLayout())
{
    for (int i = 0; i < 8; ++i)
        synth.addVoice(new SynthVoice());
    synth.addSound(new SynthSound());

    // The shared LiveState bridges the two standalone apps (C# <-> C++). In a
    // plugin host (e.g. REAPER) the host owns project state, so leave it alone.
    if (wrapperType == wrapperType_Standalone)
    {
        PresetIO::loadFromFile(apvts, PresetIO::liveStateFile());
        apvts.state.addListener(this);
        startTimer(1500);

        // The standalone wrapper restores its OWN saved state right after
        // construction; re-load the shared LiveState afterwards so it wins.
        juce::MessageManager::callAsync([this]
        {
            PresetIO::loadFromFile(apvts, PresetIO::liveStateFile());
        });
    }
}

SynthyProcessor::~SynthyProcessor()
{
    if (wrapperType == wrapperType_Standalone)
    {
        stopTimer();
        apvts.state.removeListener(this);
        saveLiveState();
    }
}

void SynthyProcessor::timerCallback()
{
    if (liveDirty.exchange(false))
        saveLiveState();
}

void SynthyProcessor::saveLiveState()
{
    PresetIO::saveToFile(apvts, PresetIO::liveStateFile(), "LiveState");
}

void SynthyProcessor::randomize()
{
    using namespace Parameters;
    auto& rng = juce::Random::getSystemRandom();

    // Random value for every parameter...
    for (auto* p : getParameters())
        p->setValueNotifyingHost(rng.nextFloat());

    auto set = [this](const juce::String& id, float raw)
    {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(raw));
    };

    // ...then guards so the patch is actually audible & playable.
    bool anySource = *apvts.getRawParameterValue(ID::oscOn(1)) > 0.5f
                   || *apvts.getRawParameterValue(ID::oscOn(2)) > 0.5f
                   || *apvts.getRawParameterValue(ID::oscOn(3)) > 0.5f
                   || *apvts.getRawParameterValue(ID::wavetableOn) > 0.5f;
    if (! anySource)
        set(ID::oscOn(1), 1.0f);

    if (*apvts.getRawParameterValue(ID::oscAmp(1)) < 0.2f)
        set(ID::oscAmp(1), 0.3f + rng.nextFloat() * 0.5f);

    // Keep a sane master volume, sustain (drone is heard) and a not-too-slow attack.
    set(ID::masterVol, 0.4f + rng.nextFloat() * 0.35f);
    set(ID::sustain,   0.6f + rng.nextFloat() * 0.4f);
    set(ID::attack,    rng.nextFloat() * 0.5f);

    // Pick a valid built-in wavetable bank (0..5), not an empty WAV slot.
    set(ID::wavetableBank, (float) rng.nextInt(juce::Range<int>(0, 6)));
}

void SynthyProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate(sampleRate);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<SynthVoice*>(synth.getVoice(i)))
            voice->prepareToPlay(sampleRate, samplesPerBlock);
}

void SynthyProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // Auto-play: trigger note when any sound source is enabled
    bool anyOscOn = false;
    for (int i = 1; i <= 3; ++i)
        if (*apvts.getRawParameterValue(Parameters::ID::oscOn(i)) > 0.5f)
            anyOscOn = true;

    if (*apvts.getRawParameterValue(Parameters::ID::karplusOn) > 0.5f)
        anyOscOn = true;
    if (*apvts.getRawParameterValue(Parameters::ID::noiseType) > 0.5f)
        anyOscOn = true;
    if (*apvts.getRawParameterValue(Parameters::ID::wavetableOn) > 0.5f)
        anyOscOn = true;

    if (anyOscOn && !autoNoteOn)
    {
        keyboardState.noteOn(1, 60, 0.8f);
        autoNoteOn = true;
    }
    else if (!anyOscOn && autoNoteOn)
    {
        keyboardState.noteOff(1, 60, 0.0f);
        autoNoteOn = false;
    }

    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);

    // Update all voice parameters
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<SynthVoice*>(synth.getVoice(i)))
            Parameters::applyToVoice(apvts, voice->getOscillators(),
                                     voice->getEnvelope(), voice->getFilter(),
                                     voice->getDistortion(), voice->getWavefolder(),
                                     voice->getDelay(),
                                     voice->getChorus(), voice->getReverb(),
                                     voice->getLFO(), voice->getNoise(),
                                     voice->getKarplus(), voice->getWavetable(),
                                     voice->getMixMode());

    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());

    // Capture waveform before master volume
    auto* channelData = buffer.getReadPointer(0);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        waveformCapture.writeSample(channelData[i]);

    buffer.applyGain(*apvts.getRawParameterValue(Parameters::ID::masterVol));
}

void SynthyProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SynthyProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* SynthyProcessor::createEditor()
{
    return new SynthyEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SynthyProcessor();
}
