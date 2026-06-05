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

    // Listen for keypresses so the auto-play drone can step aside when played.
    keyboardState.addListener(this);

    // The shared LiveState bridges the two standalone apps (C# <-> C++). In a
    // plugin host (e.g. REAPER) the host owns project state, so leave it alone.
    if (wrapperType == wrapperType_Standalone)
    {
        PresetIO::loadFromFile(apvts, PresetIO::liveStateFile());
        if (auto n = PresetIO::nameFromFile(PresetIO::liveStateFile()); n.isNotEmpty())
            currentPresetName = n;
        restoreModifiedState(PresetIO::modifiedFromFile(PresetIO::liveStateFile()));
        apvts.state.addListener(this);
        startTimer(1500);

        // The standalone wrapper restores its OWN saved state right after
        // construction; re-load the shared LiveState afterwards so it wins.
        juce::MessageManager::callAsync([this]
        {
            PresetIO::loadFromFile(apvts, PresetIO::liveStateFile());
            if (auto n = PresetIO::nameFromFile(PresetIO::liveStateFile()); n.isNotEmpty())
                currentPresetName = n;
            restoreModifiedState(PresetIO::modifiedFromFile(PresetIO::liveStateFile()));
        });
    }
}

SynthyProcessor::~SynthyProcessor()
{
    keyboardState.removeListener(this);
    if (wrapperType == wrapperType_Standalone)
    {
        stopTimer();
        apvts.state.removeListener(this);
        saveLiveState();
    }
}

void SynthyProcessor::handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNote, float)
{
    if (midiChannel == kDroneChannel)
    {
        currentNoteRatio.store(1.0f);   // our drone note (C4) → FREQ knobs show base
        return;
    }

    // A real keypress silences the auto-play drone and, while held, drives the
    // FREQ-knob display to the played frequency (it reverts to base on release).
    autoPlayEnabled.store(false);
    if (midiNote >= 0 && midiNote < 128)
        (midiNote < 64 ? heldNotesLo : heldNotesHi).fetch_or(1ULL << (midiNote & 63));
    currentNoteRatio.store((float) (juce::MidiMessage::getMidiNoteInHertz(midiNote)
                                  / juce::MidiMessage::getMidiNoteInHertz(60)));
}

void SynthyProcessor::handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNote, float)
{
    if (midiChannel == kDroneChannel)
        return;
    if (midiNote >= 0 && midiNote < 128)
        (midiNote < 64 ? heldNotesLo : heldNotesHi).fetch_and(~(1ULL << (midiNote & 63)));
    if (heldNotesLo.load() == 0 && heldNotesHi.load() == 0)
        currentNoteRatio.store(1.0f);   // nothing held → FREQ knobs back to base
}

void SynthyProcessor::timerCallback()
{
    if (liveDirty.exchange(false))
        saveLiveState();
}

void SynthyProcessor::saveLiveState()
{
    // Persist the active preset name + modified flag so the patch (and whether it
    // was an unsaved working state) come back on restart.
    PresetIO::saveToFile(apvts, PresetIO::liveStateFile(), currentPresetName, isPresetModified());
}

void SynthyProcessor::randomize()
{
    using namespace Parameters;
    auto& rng = juce::Random::getSystemRandom();

    // STEREO and MASTER VOLUME are global "mastering" choices, not part of the
    // sound design → RANDOM must leave them untouched. Snapshot now, restore after.
    const float keepStereoOn    = *apvts.getRawParameterValue(ID::stereoOn);
    const float keepStereoWidth = *apvts.getRawParameterValue(ID::stereoWidth);
    const float keepStereoTime  = *apvts.getRawParameterValue(ID::stereoTime);
    const float keepMasterVol   = *apvts.getRawParameterValue(ID::masterVol);

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

    // Keep a sane sustain (drone is heard) and a not-too-slow attack.
    set(ID::sustain,   0.6f + rng.nextFloat() * 0.4f);
    set(ID::attack,    rng.nextFloat() * 0.5f);

    // Pick a valid built-in wavetable bank (0..5), not an empty WAV slot.
    set(ID::wavetableBank, (float) rng.nextInt(juce::Range<int>(0, 6)));

    // Keep the newer effects/sources tasteful so random patches stay musical
    // (their On/Off stays random; only the extreme ranges are reined in).
    set(ID::wavefoldDrive, rng.nextFloat() * 0.6f);
    set(ID::bitcrushBits, (float) rng.nextInt(juce::Range<int>(4, 13)));  // 4..12 bit
    set(ID::bitcrushRate, (float) rng.nextInt(juce::Range<int>(1, 9)));   // 1..8x
    set(ID::subLevel,     0.3f + rng.nextFloat() * 0.4f);                 // 0.3..0.7
    set(ID::subOctave,    (float) rng.nextInt(juce::Range<int>(0, 2)));   // -1/-2 only

    // Restore the global settings the dice roll overwrote (see snapshot above).
    set(ID::stereoOn,    keepStereoOn);
    set(ID::stereoWidth, keepStereoWidth);
    set(ID::stereoTime,  keepStereoTime);
    set(ID::masterVol,   keepMasterVol);

    currentPresetName = "Random";
    markPresetClean();   // a fresh random patch is its own "clean" state
}

void SynthyProcessor::resetToDefault()
{
    // Reset every parameter to its default, then enable only OSC 1 so the
    // auto-play drone has exactly one (half-volume) source.
    for (auto* p : getParameters())
        p->setValueNotifyingHost(p->getDefaultValue());

    if (auto* osc1On = apvts.getParameter(Parameters::ID::oscOn(1)))
        osc1On->setValueNotifyingHost(1.0f);

    autoPlayEnabled.store(true);
    currentPresetName = "Init";
    markPresetClean();
}

// --- Preset "modified" tracking (value-compare against a clean snapshot) ---

void SynthyProcessor::markPresetClean()
{
    cleanSnapshot.clear();
    for (auto* p : getParameters())
        cleanSnapshot.push_back(p->getValue());
}

bool SynthyProcessor::isPresetModified() const
{
    auto& params = getParameters();
    if (cleanSnapshot.size() != (size_t) params.size())
        return true;   // no baseline (e.g. restored as a modified working state)
    for (int i = 0; i < params.size(); ++i)
        if (std::abs(params[i]->getValue() - cleanSnapshot[(size_t) i]) > 1.0e-6f)
            return true;
    return false;
}

void SynthyProcessor::restoreModifiedState(bool modified)
{
    // On LiveState load: a clean state gets a matching baseline; a modified
    // working state leaves the baseline empty so isPresetModified() stays true.
    if (modified)
        cleanSnapshot.clear();
    else
        markPresetClean();
}

void SynthyProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate(sampleRate);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<SynthVoice*>(synth.getVoice(i)))
            voice->prepareToPlay(sampleRate, samplesPerBlock);

    stereoWidth.prepare(sampleRate);
    uiLfo.setSampleRate(sampleRate);
}

void SynthyProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // Which sound generators are currently enabled (one bit each).
    unsigned mask = 0;
    for (int i = 1; i <= 3; ++i)
        if (*apvts.getRawParameterValue(Parameters::ID::oscOn(i)) > 0.5f) mask |= (1u << i);
    if (*apvts.getRawParameterValue(Parameters::ID::karplusOn)  > 0.5f) mask |= (1u << 4);
    if (*apvts.getRawParameterValue(Parameters::ID::noiseType)  > 0.5f) mask |= (1u << 5);
    if (*apvts.getRawParameterValue(Parameters::ID::wavetableOn) > 0.5f) mask |= (1u << 6);
    if (*apvts.getRawParameterValue(Parameters::ID::subOn)      > 0.5f) mask |= (1u << 7);

    // A newly enabled generator re-arms the auto-play drone (rising edge).
    if ((mask & ~prevSourcesMask) != 0)
        autoPlayEnabled.store(true);
    prevSourcesMask = mask;

    // Drone note 60 (C4) while auto-play is armed and a source is on. The user
    // playing a key clears autoPlayEnabled (see handleNoteOn) → drone steps aside.
    bool wantDrone = autoPlayEnabled.load() && (mask != 0);
    bool droneJustTriggered = false;
    if (wantDrone && !autoNoteOn)
    {
        keyboardState.noteOn(kDroneChannel, kDroneNote, 0.8f);
        autoNoteOn = true;
        droneJustTriggered = true;
    }
    else if (!wantDrone && autoNoteOn)
    {
        keyboardState.noteOff(kDroneChannel, kDroneNote, 0.0f);
        autoNoteOn = false;
    }

    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);

    // Update all voice parameters
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<SynthVoice*>(synth.getVoice(i)))
            Parameters::applyToVoice(apvts, voice->getOscillators(),
                                     voice->getEnvelope(), voice->getFilter(),
                                     voice->getDistortion(), voice->getWavefolder(),
                                     voice->getBitcrusher(),
                                     voice->getDelay(),
                                     voice->getChorus(), voice->getReverb(),
                                     voice->getLFO(), voice->getNoise(),
                                     voice->getKarplus(), voice->getWavetable(),
                                     voice->getMixMode(),
                                     voice->getSubOsc(), voice->getSubOctaveRef());

    // Don't let the auto-play drone pluck the Karplus string (it's played via
    // the keyboard). Suppress the pluck only for the drone's own note-on.
    if (droneJustTriggered)
        for (int i = 0; i < synth.getNumVoices(); ++i)
            if (auto* v = dynamic_cast<SynthVoice*>(synth.getVoice(i)))
                v->setPluckEnabled(false);

    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());

    if (droneJustTriggered)
        for (int i = 0; i < synth.getNumVoices(); ++i)
            if (auto* v = dynamic_cast<SynthVoice*>(synth.getVoice(i)))
                v->setPluckEnabled(true);

    // Advance the display-only LFO so the editor can draw live modulation rings.
    // Mirrors the patch LFO params; runs regardless of whether a note sounds.
    {
        using namespace Parameters;
        uiLfo.setRate(*apvts.getRawParameterValue(ID::lfoRate));
        uiLfo.setDepth(*apvts.getRawParameterValue(ID::lfoDepth));
        uiLfo.setWaveform((LFOWaveform)(int) *apvts.getRawParameterValue(ID::lfoWave));
        uiLfo.setTarget((LFOTarget)(int) *apvts.getRawParameterValue(ID::lfoTarget));
        float v = 0.0f;
        for (int i = 0, n = buffer.getNumSamples(); i < n; ++i)
            v = uiLfo.process();
        lfoDisplayValue.store(v);
    }

    // Capture waveform before master volume (still mono content -> the scope
    // shows the dry mono mix, unaffected by the stereo stage below).
    auto* channelData = buffer.getReadPointer(0);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        waveformCapture.writeSample(channelData[i]);

    // Final pseudo-stereo stage: turns the mono mix into a wide stereo image.
    stereoWidth.enabled = *apvts.getRawParameterValue(Parameters::ID::stereoOn) > 0.5f;
    stereoWidth.width   = *apvts.getRawParameterValue(Parameters::ID::stereoWidth);
    stereoWidth.timeMs  = *apvts.getRawParameterValue(Parameters::ID::stereoTime);
    stereoWidth.process(buffer);

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
