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

    // Epic 5: keep the MIX MODE source selectors distinct (both standalone + plugin).
    apvts.addParameterListener(Parameters::ID::mixSrcA, this);
    apvts.addParameterListener(Parameters::ID::mixSrcB, this);

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
    apvts.removeParameterListener(Parameters::ID::mixSrcA, this);
    apvts.removeParameterListener(Parameters::ID::mixSrcB, this);
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

void SynthyProcessor::parameterChanged(const juce::String& paramId, float newValue)
{
    using namespace Parameters;
    // Keep the two MIX MODE source selectors distinct (Epic 5). Setting one equal to the other
    // bumps the OTHER to a free OSC. The guard stops the bump from re-triggering us.
    if (fixingMixSrc.exchange(true)) { return; }

    const int v = juce::jlimit(0, 2, (int) newValue);
    auto bumpOther = [this](const char* otherId, int avoid)
    {
        if (auto* p = apvts.getParameter(otherId))
            p->setValueNotifyingHost(p->convertTo0to1((float) (avoid == 0 ? 1 : 0)));   // first OSC != avoid
    };

    if (paramId == ID::mixSrcA && v == (int) *apvts.getRawParameterValue(ID::mixSrcB))
        bumpOther(ID::mixSrcB, v);
    else if (paramId == ID::mixSrcB && v == (int) *apvts.getRawParameterValue(ID::mixSrcA))
        bumpOther(ID::mixSrcA, v);

    fixingMixSrc = false;
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
    const float keepArpOn       = *apvts.getRawParameterValue(ID::arpOn);   // arp = performance, not sound design
    const float keepGlideOn     = *apvts.getRawParameterValue(ID::glideOn); // glide = performance, not sound design
    const float keepKeyboardOn  = *apvts.getRawParameterValue(ID::keyboardOn); // keyboard = input surface, not sound design

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

    // Pitch envelope: keep the On/Off random but rein in the amount so random patches
    // don't warble wildly on every note (a subtle ±6-semitone sweep at most).
    set(ID::pitchEnvAmount, -6.0f + rng.nextFloat() * 12.0f);             // -6..+6 semitones
    set(ID::pitchEnvTime,   0.05f + rng.nextFloat() * 0.35f);             // 0.05..0.4 s

    // Restore the global settings the dice roll overwrote (see snapshot above).
    set(ID::stereoOn,    keepStereoOn);
    set(ID::stereoWidth, keepStereoWidth);
    set(ID::stereoTime,  keepStereoTime);
    set(ID::masterVol,   keepMasterVol);
    set(ID::arpOn,       keepArpOn);
    set(ID::glideOn,     keepGlideOn);
    set(ID::keyboardOn,  keepKeyboardOn);

    currentPresetName = "Random";
    markPresetClean();   // a fresh random patch is its own "clean" state
}

void SynthyProcessor::resetToDefault()
{
    // Reset every parameter to its default, then enable all three oscillators
    // so the octave-stack default tuning (C3/C4/C5) sounds full immediately —
    // the auto-play drone now drives the whole stack.
    for (auto* p : getParameters())
        p->setValueNotifyingHost(p->getDefaultValue());

    for (int i = 1; i <= 3; ++i)
        if (auto* oscOn = apvts.getParameter(Parameters::ID::oscOn(i)))
            oscOn->setValueNotifyingHost(1.0f);

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
        {
            voice->prepareToPlay(sampleRate, samplesPerBlock);
            voice->setGlideInfo(&glideInfo);   // poly-glide: shared per-block source ratios
        }

    // Pre-size the glide note lists so the audio thread never reallocates.
    glideHeld.reserve(128); glideLastChord.reserve(128);
    glideNewNotes.reserve(128); glideOffNotes.reserve(128);

    stereoWidth.prepare(sampleRate);
    uiLfo.setSampleRate(sampleRate);
    arp.prepare(sampleRate);
    arpHeldScratch.reserve(128);

    // Feed the real sample rate to the scope/spectrum displays (ms-window + bin→Hz).
    waveformCapture.setSampleRate(sampleRate);
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
    if (*apvts.getRawParameterValue(Parameters::ID::noiseOn)    > 0.5f) mask |= (1u << 5);
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

    // Tempo-Sync (Feature): resolve the effective LFO rate + delay time ONCE per block.
    // BPM = the host's tempo when hosted (VST3/DAW), else the internal Sync Tempo knob
    // (Standalone). A division of "Free" (0) keeps the module's own free-running knob.
    double syncBpm = *apvts.getRawParameterValue(Parameters::ID::syncTempo);
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto hostBpm = pos->getBpm())
                syncBpm = *hostBpm;

    const int lfoDiv = (int) *apvts.getRawParameterValue(Parameters::ID::lfoSyncDiv);
    const double lfoRateHz = SyncDivision::isSynced(lfoDiv)
                                 ? SyncDivision::lfoRateHz(syncBpm, lfoDiv)
                                 : (double) *apvts.getRawParameterValue(Parameters::ID::lfoRate);

    const int delayDiv = (int) *apvts.getRawParameterValue(Parameters::ID::delaySyncDiv);
    const double delayTimeSec = SyncDivision::isSynced(delayDiv)
                                    ? SyncDivision::delaySeconds(syncBpm, delayDiv)
                                    : (double) *apvts.getRawParameterValue(Parameters::ID::delayTime);

    // Update all voice parameters
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<SynthVoice*>(synth.getVoice(i)))
            Parameters::applyToVoice(apvts, voice->getOscillators(),
                                     voice->getEnvelope(), voice->getFilter(),
                                     voice->getDistortion(), voice->getWavefolder(),
                                     voice->getBitcrusher(),
                                     voice->getPhaser(),
                                     voice->getDelay(),
                                     voice->getChorus(), voice->getReverb(),
                                     voice->getFormant(),
                                     voice->getLFO(), voice->getNoise(),
                                     voice->getKarplus(), voice->getWavetable(),
                                     voice->getMixMode(),
                                     voice->getSubOsc(), voice->getSubOctaveRef(),
                                     voice->getAdsrOnRef(), voice->getMixModeOnRef(),
                                     voice->getMixSrcARef(), voice->getMixSrcBRef(),
                                     voice->getPitchEnv(), voice->getPitchEnvAmountRef(),
                                     voice->getPitchEnvOnRef(),
                                     lfoRateHz, delayTimeSec);

    // Arpeggiator: replace the raw held chord with an automatic note sequence.
    {
        using namespace Parameters;
        bool arpOn = *apvts.getRawParameterValue(ID::arpOn) > 0.5f;
        if (arpOn)
        {
            arp.enabled = true;
            arp.rateHz  = *apvts.getRawParameterValue(ID::arpRate);
            arp.mode    = (Arpeggiator::Mode)(int) *apvts.getRawParameterValue(ID::arpMode);
            arp.octaves = (int) *apvts.getRawParameterValue(ID::arpOctaves);
            arp.gate    = *apvts.getRawParameterValue(ID::arpGate);

            // Held chord = the channel-1 notes currently down (keyboardState was
            // just updated above). The drone lives on channel 16, so it's excluded.
            arpHeldScratch.clear();
            for (int n = 0; n < 128; ++n)
                if (keyboardState.isNoteOn(1, n)) arpHeldScratch.push_back(n);
            arp.setHeldNotes(arpHeldScratch);

            // Drop the raw chord (channel-1 note on/off) so only the arp sounds;
            // keep everything else (e.g. the channel-16 auto-play drone).
            juce::MidiBuffer kept;
            for (const auto meta : midiMessages)
            {
                auto m = meta.getMessage();
                if ((m.isNoteOn() || m.isNoteOff()) && m.getChannel() == 1)
                    continue;
                kept.addEvent(m, meta.samplePosition);
            }
            arp.processBlock(buffer.getNumSamples(), kept, 1);
            midiMessages.swapWith(kept);
        }
        else if (arp.enabled)
        {
            arp.enabled = false;           // just switched off → release its note
            arp.releaseAll(midiMessages, 1);
            arp.reset();
        }
    }

    // Don't let the auto-play drone pluck the Karplus string (it's played via
    // the keyboard). Suppress the pluck only for the drone's own note-on.
    if (droneJustTriggered)
        for (int i = 0; i < synth.getNumVoices(); ++i)
            if (auto* v = dynamic_cast<SynthVoice*>(synth.getVoice(i)))
                v->setPluckEnabled(false);

    // Manual PLUCK (button / spacebar): re-excite the Karplus string on every voice.
    // RT-safe — the atomic flag is set on the message thread and consumed here.
    if (pluckRequested.exchange(false))
        for (int i = 0; i < synth.getNumVoices(); ++i)
            if (auto* v = dynamic_cast<SynthVoice*>(synth.getVoice(i)))
                v->pluckKarplus();

    // Poly-glide (portamento): assign each newly-started note a predecessor pitch to glide
    // FROM. Runs on the FINAL midi buffer (after the arpeggiator) so it glides arp steps too.
    {
        using namespace Parameters;
        glideInfo.enabled = *apvts.getRawParameterValue(ID::glideOn) > 0.5f;
        glideInfo.timeSec = *apvts.getRawParameterValue(ID::glideTime);
        glideInfo.startRatio.fill(-1.0f);

        glideNewNotes.clear();
        glideOffNotes.clear();
        for (const auto meta : midiMessages)
        {
            const auto m = meta.getMessage();
            if (m.getChannel() == kDroneChannel) continue;   // never glide the auto-play drone
            if (m.isNoteOn())       glideNewNotes.push_back(m.getNoteNumber());
            else if (m.isNoteOff()) glideOffNotes.push_back(m.getNoteNumber());
        }

        if (glideInfo.enabled && ! glideNewNotes.empty())
        {
            // Source = chord held before this block; if nothing is held (a gap), glide from
            // the last chord that WAS held. Pitch-sort both and map position-wise (i-th new
            // glides from i-th old); surplus new notes glide from the highest old note.
            const std::vector<int>& src0 = ! glideHeld.empty() ? glideHeld : glideLastChord;
            if (! src0.empty())
            {
                std::vector<int> src = src0;
                std::sort(src.begin(), src.end());
                std::sort(glideNewNotes.begin(), glideNewNotes.end());
                const double c4 = juce::MidiMessage::getMidiNoteInHertz(60);
                for (int i = 0; i < (int) glideNewNotes.size(); ++i)
                {
                    const int from = src[(size_t) juce::jmin(i, (int) src.size() - 1)];
                    const int note = glideNewNotes[(size_t) i];
                    if (from != note && note >= 0 && note < 128)
                        glideInfo.startRatio[(size_t) note] =
                            (float) (juce::MidiMessage::getMidiNoteInHertz(from) / c4);
                }
            }
        }

        // Maintain the held-note set for the next block (always, so it is correct the moment
        // glide is toggled on): drop note-offs, add note-ons (no duplicates).
        for (int off : glideOffNotes)
            glideHeld.erase(std::remove(glideHeld.begin(), glideHeld.end(), off), glideHeld.end());
        for (int n : glideNewNotes)
            if (std::find(glideHeld.begin(), glideHeld.end(), n) == glideHeld.end())
                glideHeld.push_back(n);
        if (! glideHeld.empty())
            glideLastChord = glideHeld;

        // Mono glide (default): monophonic last-note priority. When a new note starts, emit a
        // REGULAR note-off for the previously sounding note, so the synth releases that voice
        // itself (its normal envelope release — no click, no stuck notes, no fighting the voice
        // manager). The new note still glides from the previous pitch (startRatio set above).
        // Best for sequential playing; simultaneous chords in Mono collapse to last-note.
        const bool glideMono = (int) *apvts.getRawParameterValue(ID::glideMode) == 0;
        if (glideInfo.enabled && glideMono)
        {
            juce::MidiBuffer rebuilt;
            for (const auto meta : midiMessages)
            {
                const auto m = meta.getMessage();
                const int  sp = meta.samplePosition;
                if (m.isNoteOn() && m.getChannel() != kDroneChannel)
                {
                    if (monoSounding >= 0 && monoSounding != m.getNoteNumber())
                        rebuilt.addEvent (juce::MidiMessage::noteOff (1, monoSounding), sp);
                    monoSounding = m.getNoteNumber();
                    rebuilt.addEvent (m, sp);
                }
                else if (m.isNoteOff() && m.getChannel() != kDroneChannel)
                {
                    if (m.getNoteNumber() == monoSounding)
                        monoSounding = -1;
                    rebuilt.addEvent (m, sp);
                }
                else
                    rebuilt.addEvent (m, sp);
            }
            midiMessages.swapWith (rebuilt);
        }
        else
            monoSounding = -1;   // keep state clean while in Poly
    }

    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());

    if (droneJustTriggered)
        for (int i = 0; i < synth.getNumVoices(); ++i)
            if (auto* v = dynamic_cast<SynthVoice*>(synth.getVoice(i)))
                v->setPluckEnabled(true);

    // Advance the display-only LFO so the editor can draw live modulation rings.
    // Mirrors the patch LFO params; runs regardless of whether a note sounds.
    {
        using namespace Parameters;
        uiLfo.setRate(lfoRateHz);   // Tempo-Sync: mirror the effective (synced or free) rate
        uiLfo.setDepth(*apvts.getRawParameterValue(ID::lfoDepth));
        uiLfo.setWaveform((LFOWaveform)(int) *apvts.getRawParameterValue(ID::lfoWave));
        const bool lfoOn = *apvts.getRawParameterValue(ID::lfoOn) > 0.5f;
        uiLfo.setTarget(lfoOn ? (LFOTarget)((int) *apvts.getRawParameterValue(ID::lfoTarget) + 1)
                              : LFOTarget::Off);
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

    // Master volume — gated by masterOn (Story 2.4): off => silent output.
    const bool  masterOn   = *apvts.getRawParameterValue(Parameters::ID::masterOn) > 0.5f;
    const float masterGain = masterOn ? apvts.getRawParameterValue(Parameters::ID::masterVol)->load() : 0.0f;
    buffer.applyGain(masterGain);
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
