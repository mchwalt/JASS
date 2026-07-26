#include "PluginProcessor.h"
#include "UI/PluginEditor.h"
#include "Audio/PresetIO.h"
#include <set>

SynthyProcessor::SynthyProcessor()
    : AudioProcessor(BusesProperties()
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", Parameters::createLayout())
{
    for (int i = 0; i < 8; ++i)
        synth.addVoice(new SynthVoice());
    synth.addSound(new SynthSound());

    // One-time rebrand of the app-data folder (%AppData%\Synthy -> JASS, *.synthy -> *.jass).
    // MUST run before anything touches jassFolder() (which would create JASS and suppress it).
    PresetIO::migrateLegacyAppData();

    // Ship the demo presets + example wavetables into the user's folders on first run (idempotent).
    PresetIO::seedDemoPresets();
    PresetIO::seedWavetables();

    // Listen for keypresses so the auto-play drone can step aside when played.
    keyboardState.addListener(this);

    // Epic 5: keep the MIX MODE source selectors distinct (both standalone + plugin).
    apvts.addParameterListener(Parameters::ID::mixSrcA, this);
    apvts.addParameterListener(Parameters::ID::mixSrcB, this);
    // CROSS MOD enable coupling: needs mixModeOn + the three oscOn to auto-enable / drop.
    apvts.addParameterListener(Parameters::ID::mixModeOn, this);
    for (int i = 1; i <= 3; ++i)
        apvts.addParameterListener(Parameters::ID::oscOn(i), this);

    // Convenience: auto-enable a matrix slot's SOURCE and TARGET modules when it starts routing
    // (source picked + module != Off) — otherwise a route silently does nothing because the source
    // (e.g. an LFO) or the target module is still bypassed. The enabled TARGET now depends on the
    // MODULE selector (per-OSC → oscOn(N)), so listen on every slot's SRC + MOD selector.
    for (int n = 1; n <= ModMatrixConfig::kNumSlots; ++n)
    {
        apvts.addParameterListener(Parameters::ID::modSlotSource(n), this);
        apvts.addParameterListener(Parameters::ID::modSlotModule(n), this);
    }

    // The shared LiveState bridges the two standalone apps (C# <-> C++). In a
    // plugin host (e.g. REAPER) the host owns project state, so leave it alone.
    if (wrapperType == wrapperType_Standalone)
    {
        // One-time: bring old presets + LiveState up to the current format (flat=>nested, and fold
        // LFO built-in targets into matrix slots). Uses apvts as scratch; the LiveState load below
        // overwrites it. See PresetIO::convertOldPresets.
        PresetIO::convertOldPresets(apvts);
        PresetIO::loadFromFile(apvts, PresetIO::liveStateFile());
        if (auto n = PresetIO::nameFromFile(PresetIO::liveStateFile()); n.isNotEmpty())
            currentPresetName = n;
        restoreModifiedState(PresetIO::modifiedFromFile(PresetIO::liveStateFile()));
        apvts.state.addListener(this);
        startTimer(1500);

        // The standalone wrapper restores its OWN saved state right after
        // construction; re-load the shared LiveState afterwards so it wins.
        juce::WeakReference<SynthyProcessor> weak (this);
        juce::MessageManager::callAsync([this, weak]
        {
            if (weak == nullptr) return;   // processor destroyed before this async ran
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
    apvts.removeParameterListener(Parameters::ID::mixModeOn, this);
    for (int i = 1; i <= 3; ++i)
        apvts.removeParameterListener(Parameters::ID::oscOn(i), this);
    for (int n = 1; n <= ModMatrixConfig::kNumSlots; ++n)
    {
        apvts.removeParameterListener(Parameters::ID::modSlotSource(n), this);
        apvts.removeParameterListener(Parameters::ID::modSlotModule(n), this);
    }
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

namespace
{
    // Enable-param of the module behind a matrix SOURCE (item order == ModSource in
    // ModMatrixSpecs). Velocity has no module → empty (nothing to enable).
    juce::String matrixSourceEnableParam (int sourceIdx)
    {
        using namespace Parameters;
        switch ((ModSource) sourceIdx)
        {
            case ModSource::LFO1:     return ID::lfoOn (1);
            case ModSource::Envelope: return ID::adsrOn;
            case ModSource::Velocity: return {};
            case ModSource::LFO2:     return ID::lfoOn (2);
            case ModSource::LFO3:     return ID::lfoOn (3);
            case ModSource::LFO4:     return ID::lfoOn (4);
        }
        return {};
    }

    // Enable-param of the MODULE a matrix slot drives — from the single source (ModMatrixCatalog.h).
    // "" means no module to auto-toggle: "Alle OSC" (global voice params) has no single enable.
    // A per-OSC module (OSC 1/2/3) returns its own oscNOn, so a per-OSC routing enables just that OSC.
    juce::String matrixModuleEnableParam (int moduleIdx)
    {
        return juce::String (ModDest::enableIdOf (moduleIdx));
    }
}

void SynthyProcessor::updateMatrixModuleEnables()
{
    using namespace Parameters;

    // Which modules does the matrix currently CLAIM? For every active slot (DEST != Off): its
    // SOURCE module (LFO/Envelope) AND its TARGET module (FILTER/FORMANT/WAVETABLE/WAVEFOLD).
    std::set<juce::String> claimed;
    for (int n = 1; n <= ModMatrixConfig::kNumSlots; ++n)
    {
        const int mod = (int) *apvts.getRawParameterValue(ID::modSlotModule(n));
        if (mod <= 0)   // 0 == "Off"
            continue;
        if (const auto s = matrixSourceEnableParam((int) *apvts.getRawParameterValue(ID::modSlotSource(n))); s.isNotEmpty())
            claimed.insert(s);
        if (const auto t = matrixModuleEnableParam(mod); t.isNotEmpty())   // per-OSC → oscNOn; "Alle OSC" → none
            claimed.insert(t);
    }

    // Every module a slot can auto-drive: sources (LFO 1..4 + ADSR) and target modules — now incl.
    // OSC 1..3 (a per-OSC FREQ/AMP/DETUNE routing enables just that oscillator). "Alle OSC",
    // Velocity and Envelope-less globals have no single enable, so they are never toggled here.
    const juce::String managed[] = { ID::lfoOn(1), ID::lfoOn(2), ID::lfoOn(3), ID::lfoOn(4), ID::adsrOn,
                                     ID::oscOn(1), ID::oscOn(2), ID::oscOn(3),
                                     ID::filterOn, ID::formantOn, ID::wavetableOn, ID::wavefoldOn,
                                     ID::delayOn, ID::reverbOn, ID::chorusOn, ID::distortionOn,
                                     ID::bitcrushOn, ID::phaserOn, ID::subOn,
                                     // appended 2026-07-26 target modules (per-voice + global master-bus).
                                     // MASTER/STEREO default ON, so routing never actually toggles them
                                     // (auto-disable only undoes an enable WE made) — safe to list.
                                     ID::noiseOn, ID::karplusOn, ID::pitchEnvOn,
                                     ID::compOn, ID::stereoOn, ID::masterOn };
    for (const auto& id : managed)
    {
        auto* p = apvts.getParameter(id);
        if (p == nullptr) continue;
        const bool isOn      = *apvts.getRawParameterValue(id) >= 0.5f;
        const bool isClaimed = claimed.count(id) > 0;

        if (isClaimed && ! isOn)
        {
            p->setValueNotifyingHost(1.0f);      // a route needs it → turn on…
            matrixAutoEnabled[id] = true;        // …and remember WE did (it was off before)
        }
        else if (! isClaimed && matrixAutoEnabled[id])
        {
            p->setValueNotifyingHost(0.0f);      // no route uses it any more → undo our auto-enable
            matrixAutoEnabled[id] = false;
        }
        // A source that is on for any OTHER reason (user toggle, ADSR default) keeps its state:
        // matrixAutoEnabled stays false for it, so it is never auto-disabled here.
    }
}

void SynthyProcessor::syncCrossModEnables(const juce::String& changed)
{
    using namespace Parameters;
    auto isOn = [this](const juce::String& id) { return *apvts.getRawParameterValue(id) >= 0.5f; };
    auto setP = [this](const juce::String& id, float v)
                { if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(v); };

    const int a = juce::jlimit(0, 2, (int) *apvts.getRawParameterValue(ID::mixSrcA));
    const int b = juce::jlimit(0, 2, (int) *apvts.getRawParameterValue(ID::mixSrcB));
    const juce::String oscA = ID::oscOn(a + 1);
    const juce::String oscB = ID::oscOn(b + 1);
    const bool mixOn = isOn(ID::mixModeOn);

    const bool oscChanged = (changed == ID::oscOn(1) || changed == ID::oscOn(2) || changed == ID::oscOn(3));

    // A used operand OSC was switched OFF → CROSS MOD can no longer work, so switch it off too
    // (previously it was only shown dimmed while mixModeOn stayed set). Then undo the operands we
    // auto-enabled ourselves (an OSC that was on for its own sake keeps its state — see below).
    if (mixOn && oscChanged && (changed == oscA || changed == oscB) && (! isOn(oscA) || ! isOn(oscB)))
    {
        setP(ID::mixModeOn, 0.0f);
        for (auto& kv : crossModAutoEnabled)
            if (kv.second && isOn(kv.first))
                setP(kv.first, 0.0f);
        crossModAutoEnabled.clear();
        return;
    }

    // CROSS MOD toggled or an operand re-selected: while on, keep BOTH operand OSCs enabled
    // (remember the ones we switch on); when off, undo exactly those again. An OSC that was
    // already on (its own voice) is not remembered, so it is never auto-disabled.
    const bool routeChanged = (changed == ID::mixModeOn || changed == ID::mixSrcA || changed == ID::mixSrcB);
    if (routeChanged)
    {
        if (mixOn)
        {
            for (const auto& osc : { oscA, oscB })
                if (! isOn(osc)) { setP(osc, 1.0f); crossModAutoEnabled[osc] = true; }
            // an operand we auto-enabled earlier but no longer use (operand switched) → undo it
            for (auto& kv : crossModAutoEnabled)
                if (kv.second && kv.first != oscA && kv.first != oscB && isOn(kv.first))
                    { setP(kv.first, 0.0f); kv.second = false; }
        }
        else
        {
            for (auto& kv : crossModAutoEnabled)
                if (kv.second && isOn(kv.first))
                    setP(kv.first, 0.0f);
            crossModAutoEnabled.clear();
        }
    }
}

namespace
{
    // Encode a CROSS-MOD-relevant param id as a small int WITHOUT constructing a juce::String
    // (audio-thread safe). -1 = not a CROSS-MOD param. Reverse map below (message thread only).
    int crossModCode(const juce::String& id)
    {
        using namespace Parameters;
        if (id == ID::mixModeOn) return 0;
        if (id == ID::mixSrcA)   return 1;
        if (id == ID::mixSrcB)   return 2;
        if (id == ID::oscOn(1))  return 3;   // ID::oscOn returns a cached ref (no alloc; warmed)
        if (id == ID::oscOn(2))  return 4;
        if (id == ID::oscOn(3))  return 5;
        return -1;
    }
    juce::String crossModIdFromCode(int c)   // message thread (juce::String construction OK)
    {
        using namespace Parameters;
        switch (c) { case 0: return ID::mixModeOn; case 1: return ID::mixSrcA; case 2: return ID::mixSrcB;
                     case 3: return ID::oscOn(1);  case 4: return ID::oscOn(2); case 5: return ID::oscOn(3); }
        return {};
    }
}

void SynthyProcessor::parameterChanged(const juce::String& paramId, float newValue)
{
    juce::ignoreUnused(newValue);
    // APVTS calls this synchronously on the changing thread — the AUDIO thread under host automation.
    // Off the message thread we do NO allocation / setValueNotifyingHost: just flag the needed
    // reconciliation (atomic) and let reconcileParamCouplingsIfDirty() run it on the message thread.
    const bool onMsgThread = juce::MessageManager::existsAndIsCurrentThread();

    if (paramId.startsWith("modSlot"))   // startsWith(const char*) is alloc-free
    {
        if (onMsgThread) updateMatrixModuleEnables();
        else             matrixEnablesDirty.store(true);
        return;
    }

    const int cc = crossModCode(paramId);
    if (cc < 0)
        return;
    if (! onMsgThread)
    {
        pendingCrossModCode.store(cc);
        crossModDirty.store(true);
        return;
    }
    applyCrossModCoupling(paramId);
}

// Message-thread poll (editor timer): run any reconciliation deferred from the audio thread.
void SynthyProcessor::reconcileParamCouplingsIfDirty()
{
    if (matrixEnablesDirty.exchange(false))
        updateMatrixModuleEnables();
    if (crossModDirty.exchange(false))
        applyCrossModCoupling(crossModIdFromCode(pendingCrossModCode.load()));
}

// The CROSS-MOD coupling body (message thread only): operand distinctness + enable coupling.
void SynthyProcessor::applyCrossModCoupling(const juce::String& paramId)
{
    using namespace Parameters;
    if (paramId.isEmpty())
        return;
    if (fixingMixSrc.exchange(true))   // shared reentrancy guard: ignore our own write-backs
        return;

    // Keep the two operands distinct (Epic 5): a==b would be a no-op, so bump the OTHER to a free OSC.
    if (paramId == ID::mixSrcA || paramId == ID::mixSrcB)
    {
        const int a = (int) *apvts.getRawParameterValue(ID::mixSrcA);
        const int b = (int) *apvts.getRawParameterValue(ID::mixSrcB);
        if (a == b)
        {
            const char* other = (paramId == ID::mixSrcA) ? ID::mixSrcB : ID::mixSrcA;
            if (auto* p = apvts.getParameter(other))
                p->setValueNotifyingHost(p->convertTo0to1((float) (a == 0 ? 1 : 0)));   // first OSC != a
        }
    }

    // Enable coupling: auto on/off of the operand OSCs; drop CROSS MOD when an operand goes off.
    syncCrossModEnables(paramId);

    fixingMixSrc = false;
}

void SynthyProcessor::randomize()
{
    using namespace Parameters;
    auto& rng = juce::Random::getSystemRandom();

    // The whole MASTER BUS zone (MASTER + STEREO + COMPRESSOR) is mastering/output, not sound
    // design → RANDOM must leave it untouched. A random masterOn=off would MUTE the patch and a
    // random syncTempo would reharmonise every tempo-synced LFO/delay. Snapshot the zone in full,
    // restore it verbatim after the dice roll.
    static const char* const masterBusIds[] = {
        ID::masterOn, ID::masterVol, ID::syncTempo,
        ID::stereoOn, ID::stereoWidth, ID::stereoTime,
        ID::compOn, ID::compThreshold, ID::compRatio, ID::compAttack, ID::compRelease, ID::compMakeup };
    const int numMasterBus = (int) (sizeof (masterBusIds) / sizeof (masterBusIds[0]));
    float keepMasterBus[sizeof (masterBusIds) / sizeof (masterBusIds[0])];
    for (int i = 0; i < numMasterBus; ++i)
        keepMasterBus[i] = *apvts.getRawParameterValue (masterBusIds[i]);

    const float keepArpOn       = *apvts.getRawParameterValue(ID::arpOn);   // arp = performance, not sound design
    const float keepGlideOn     = *apvts.getRawParameterValue(ID::glideOn); // glide = performance, not sound design
    const float keepKeyboardOn  = *apvts.getRawParameterValue(ID::keyboardOn); // keyboard = input surface, not sound design

    // Random value for every parameter... (the MOD MATRIX is re-rolled CONTROLLED below —
    // it is the synth's central movement layer, so RANDOM must vary it, not leave it alone.)
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

    // Keep the tone AUDIBLE. Unconstrained oscillator/filter dice made silent patches often: a high
    // oscillator (up to 10 kHz) behind a low low-pass, or a low tone behind a high high-pass, cuts
    // the fundamental completely. Pin the oscillator/wavetable base pitch to a musical range, then
    // put the filter cutoff on the RIGHT side of it for the rolled filter type so the tone passes.
    for (int i = 1; i <= 3; ++i)
        set(ID::oscFreq(i), 40.0f + rng.nextFloat() * 620.0f);           // ~40..660 Hz
    set(ID::wavetableFreq,  40.0f + rng.nextFloat() * 620.0f);
    const bool highpass = *apvts.getRawParameterValue(ID::filterType) > 0.5f;   // 0=Lowpass, 1=Highpass
    set(ID::filterCutoff, highpass ? 20.0f   + rng.nextFloat() * 130.0f          // 20..150 Hz => lows pass
                                   : 1500.0f + rng.nextFloat() * 13000.0f);      // 1.5..14.5 kHz => tone passes

    // Restore the whole MASTER BUS zone + the performance/input surfaces the dice roll overwrote.
    for (int i = 0; i < numMasterBus; ++i)
        set(masterBusIds[i], keepMasterBus[i]);
    set(ID::arpOn,       keepArpOn);
    set(ID::glideOn,     keepGlideOn);
    set(ID::keyboardOn,  keepKeyboardOn);

    // Modulation matrix: RANDOM VARIES it too (it is the central movement layer). The blanket
    // dice above already wrote random slots; redo them CONTROLLED so patches move without blowing
    // up. Matrix ON; a few slots get UNIQUE targets (no stacking on one target — the old blow-up
    // risk that made v1 exclude the matrix) and reined-in bipolar amounts. Any LFO/ENV chosen as a
    // source is enabled so the routing is actually audible.
    set(ID::modMatrixOn, 1.0f);
    // A diverse pool across modules (Epic 8.3 full coverage). fromLegacyTarget maps each to its
    // (module, param); OSC-scoped ones may then be re-pointed to a single oscillator below.
    int rndTargets[] = { (int) LFOTarget::Frequency,        (int) LFOTarget::Amplitude,
                         (int) LFOTarget::OscDetune,        (int) LFOTarget::FilterCutoff,
                         (int) LFOTarget::FilterResonance,  (int) LFOTarget::WavetablePosition,
                         (int) LFOTarget::FormantVowel,     (int) LFOTarget::WavefolderDrive,
                         (int) LFOTarget::ChorusDepth,      (int) LFOTarget::DelayMix,
                         (int) LFOTarget::ReverbMix,        (int) LFOTarget::BitcrushMix,
                         (int) LFOTarget::DistortionDrive,  (int) LFOTarget::SubLevel };
    const int numRndTargets = (int) (sizeof(rndTargets) / sizeof(rndTargets[0]));
    for (int i = numRndTargets - 1; i > 0; --i)             // Fisher–Yates: pick distinct targets
    {
        const int j = rng.nextInt(i + 1);
        const int t = rndTargets[i]; rndTargets[i] = rndTargets[j]; rndTargets[j] = t;
    }

    const int activeSlots = rng.nextInt(juce::Range<int>(2, 5));   // 2..4 routings (rest = Off)
    for (int n = 0; n < ModMatrixConfig::kNumSlots; ++n)
    {
        if (n >= activeSlots)
        {
            set(ID::modSlotModule(n + 1), 0.0f);   // 0 == "Off" (inactive slot)
            continue;
        }
        const int   src = rng.nextInt(ModMatrixConfig::kNumSources);   // LFO1/Env/Vel/LFO2..4
        const float mag = 0.3f + rng.nextFloat() * 0.6f;              // 0.3..0.9 (audible, tame)
        auto        mp  = ModDest::fromLegacyTarget(rndTargets[n]);   // LFOTarget → (module, param)
        // Showcase per-OSC: FREQ/AMP/DETUNE sometimes target a SINGLE oscillator, not just "Alle OSC".
        if (ModDest::oscParamSlot(ModDest::targetOf(mp.module, mp.param)) >= 0)
        {
            static const char* const oscMods[4] = { "OSC 1", "OSC 2", "OSC 3", "Alle OSC" };
            if (const int m = ModDest::moduleIndexForLabel(oscMods[rng.nextInt(4)]); m >= 0)
                mp.module = m;
        }
        set(ID::modSlotSource(n + 1), (float) src);
        set(ID::modSlotModule(n + 1), (float) mp.module);
        set(ID::modSlotParam (n + 1), (float) mp.param);
        set(ID::modSlotAmount(n + 1), rng.nextBool() ? mag : -mag);   // bipolar

        switch ((ModSource) src)   // enable the picked source so the slot is heard
        {
            case ModSource::LFO1: set(ID::lfoOn(1), 1.0f); break;
            case ModSource::LFO2: set(ID::lfoOn(2), 1.0f); break;
            case ModSource::LFO3: set(ID::lfoOn(3), 1.0f); break;
            case ModSource::LFO4: set(ID::lfoOn(4), 1.0f); break;
            case ModSource::Envelope: set(ID::adsrOn, 1.0f); break;
            case ModSource::Velocity: break;
        }
    }

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
    Parameters::ID::warmIndexedIds();   // RT-safety: build the indexed-ID caches on the message thread
    synth.setCurrentPlaybackSampleRate(sampleRate);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = static_cast<SynthVoice*>(synth.getVoice(i)))
        {
            voice->prepareToPlay(sampleRate, samplesPerBlock);
            voice->setGlideInfo(&glideInfo);   // poly-glide: shared per-block source ratios
        }

    // Pre-size the glide note lists so the audio thread never reallocates.
    glideHeld.reserve(128); glideLastChord.reserve(128);
    glideNewNotes.reserve(128); glideOffNotes.reserve(128);
    arpKeptScratch.ensureSize(2048); glideRebuiltScratch.ensureSize(2048);   // RT: no per-block MidiBuffer growth

    stereoWidth.prepare(sampleRate);
    compressor.prepare(sampleRate);
    prevMasterGain = 0.0f;   // ramp start for the (possibly modulated) master gain
    for (auto& l : uiLfos) l.setSampleRate(sampleRate);
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

    // ── Global (master-bus) modulation offsets ──────────────────────────────────────────────
    // STEREO / MASTER / COMPRESSOR run on the SUMMED mix (further below), not per voice, so their
    // MOD MATRIX routings are applied HERE at block rate. Source values are the GLOBAL uiLfo values
    // from the PREVIOUS block (lfoDisplayValues, advanced after the render) — a one-block lag that is
    // inaudible. Only LFO sources drive global targets (Velocity/Envelope have no single global
    // value), which matches the editor's ring feed exactly, so ring == audio.
    std::array<double, ModMatrixConfig::kNumTargets> gMod {};
    {
        using namespace Parameters;
        static constexpr int kLfoSrc[kNumLFOs] = { (int) ModSource::LFO1, (int) ModSource::LFO2,
                                                   (int) ModSource::LFO3, (int) ModSource::LFO4 };
        if (*apvts.getRawParameterValue(ID::modMatrixOn) > 0.5f)
            for (int n = 1; n <= ModMatrixConfig::kNumSlots; ++n)
            {
                const int mod = (int) *apvts.getRawParameterValue(ID::modSlotModule(n));
                if (mod <= 0) continue;   // Off
                const int   src = (int) *apvts.getRawParameterValue(ID::modSlotSource(n));
                const int   par = (int) *apvts.getRawParameterValue(ID::modSlotParam(n));
                const float amt = *apvts.getRawParameterValue(ID::modSlotAmount(n));
                float sv = 0.0f;
                for (int i = 0; i < kNumLFOs; ++i) if (src == kLfoSrc[i]) sv = lfoDisplayValues[i].load();
                const int tgt = (int) ModDest::targetOf(mod, par);
                if (tgt > 0 && tgt < (int) gMod.size()) gMod[(size_t) tgt] += (double) amt * sv;
            }
    }
    // MASTER · TEMPO — wobble the Tempo-Sync BPM so synced LFOs/delay drift around the beat (applied
    // to the resolved tempo, host or internal, ±40 BPM at full modulation).
    syncBpm = juce::jlimit(40.0, 250.0, syncBpm + gMod[(size_t) LFOTarget::MasterTempo] * 90.0);

    // Per-LFO effective rate (Tempo-Sync resolved once per block; Free => raw RATE knob).
    double lfoRateHz[kNumLFOs];
    for (int i = 0; i < kNumLFOs; ++i)
    {
        const int div = (int) *apvts.getRawParameterValue(Parameters::ID::lfoSyncDiv(i + 1));
        lfoRateHz[i] = SyncDivision::isSynced(div)
                           ? SyncDivision::lfoRateHz(syncBpm, div)
                           : (double) *apvts.getRawParameterValue(Parameters::ID::lfoRate(i + 1));
    }

    const int delayDiv = (int) *apvts.getRawParameterValue(Parameters::ID::delaySyncDiv);
    const double delayTimeSec = SyncDivision::isSynced(delayDiv)
                                    ? SyncDivision::delaySeconds(syncBpm, delayDiv)
                                    : (double) *apvts.getRawParameterValue(Parameters::ID::delayTime);

    // Update all voice parameters
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = static_cast<SynthVoice*>(synth.getVoice(i)))
            Parameters::applyToVoice(apvts, voice->getOscillators(),
                                     voice->getEnvelope(), voice->getStrips(),   // Epic 10: per-channel FX strips
                                     voice->getLFOs(), voice->getNoise(),
                                     voice->getKarplus(), voice->getWavetable(),
                                     voice->getMixMode(),
                                     voice->getSubOsc(), voice->getSubOctaveRef(),
                                     voice->getAdsrOnRef(), voice->getMixModeOnRef(),
                                     voice->getMixSrcARef(), voice->getMixSrcBRef(),
                                     voice->getPitchEnv(), voice->getPitchEnvAmountRef(),
                                     voice->getPitchEnvOnRef(),
                                     voice->getModSlots(), voice->getModMatrixOnRef(),
                                     voice->getOutputModeRef(), voice->getGeneratorPan(),   // Epic 10
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
            auto& kept = arpKeptScratch; kept.clear();   // reused member (no per-block MidiBuffer alloc)
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
            if (auto* v = static_cast<SynthVoice*>(synth.getVoice(i)))
                v->setPluckEnabled(false);

    // Manual PLUCK (button / spacebar): re-excite the Karplus string on every voice.
    // RT-safe — the atomic flag is set on the message thread and consumed here.
    if (pluckRequested.exchange(false))
        for (int i = 0; i < synth.getNumVoices(); ++i)
            if (auto* v = static_cast<SynthVoice*>(synth.getVoice(i)))
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
            auto& rebuilt = glideRebuiltScratch; rebuilt.clear();   // reused member (no per-block alloc)
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
            if (auto* v = static_cast<SynthVoice*>(synth.getVoice(i)))
                v->setPluckEnabled(true);

    // Advance the display-only LFO so the editor can draw live modulation rings.
    // Mirrors the patch LFO params; runs regardless of whether a note sounds.
    {
        using namespace Parameters;
        for (int li = 0; li < kNumLFOs; ++li)
        {
            uiLfos[li].setRate(lfoRateHz[li]);   // mirror the effective (synced or free) rate
            uiLfos[li].setDepth(*apvts.getRawParameterValue(ID::lfoDepth(li + 1)));
            uiLfos[li].setWaveform((LFOWaveform)(int) *apvts.getRawParameterValue(ID::lfoWave(li + 1)));
            const bool on = *apvts.getRawParameterValue(ID::lfoOn(li + 1)) > 0.5f;
            uiLfos[li].setTarget(on ? (LFOTarget)((int) *apvts.getRawParameterValue(ID::lfoTarget(li + 1)) + 1)
                                    : LFOTarget::Off);
            float v = 0.0f;
            for (int i = 0, n = buffer.getNumSamples(); i < n; ++i)
                v = uiLfos[li].process();
            lfoDisplayValues[li].store(v);
        }
    }

    // Capture waveform before master volume (still mono content -> the scope
    // shows the dry mono mix, unaffected by the stereo stage below).
    auto* channelData = buffer.getReadPointer(0);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        waveformCapture.writeSample(channelData[i]);

    // Master-bus compressor: glue the summed mono mix before it is widened.
    {
        using namespace Parameters;
        compressor.enabled   = *apvts.getRawParameterValue(ID::compOn) > 0.5f;
        // Base + MOD MATRIX offset (gMod), each clamped to the param's own range.
        compressor.threshold = juce::jlimit(-60.0f, 0.0f,   (float) (*apvts.getRawParameterValue(ID::compThreshold) + gMod[(size_t) LFOTarget::CompThreshold] * 12.0));
        compressor.ratio     = juce::jlimit(1.0f, 20.0f,    (float) (*apvts.getRawParameterValue(ID::compRatio)     + gMod[(size_t) LFOTarget::CompRatio]     * 4.0));
        compressor.attackMs  = juce::jlimit(0.1f, 100.0f,   (float) (*apvts.getRawParameterValue(ID::compAttack)    + gMod[(size_t) LFOTarget::CompAttack]    * 20.0));
        compressor.releaseMs = juce::jlimit(10.0f, 1000.0f, (float) (*apvts.getRawParameterValue(ID::compRelease)   + gMod[(size_t) LFOTarget::CompRelease]   * 100.0));
        compressor.makeupDb  = juce::jlimit(0.0f, 24.0f,    (float) (*apvts.getRawParameterValue(ID::compMakeup)    + gMod[(size_t) LFOTarget::CompMakeup]    * 6.0));
        compressor.process(buffer);
    }

    // Final pseudo-stereo stage: turns the mono mix into a wide stereo image. Only in Pseudo-Stereo
    // mode (Epic 10) — Mono needs no widening, and Stereo-Pan already produces a true stereo image (the
    // voices pan into L/R), so widening it would double-image. The width/time knobs still feed it.
    const int outMode = (int) *apvts.getRawParameterValue(Parameters::ID::outputMode);
    stereoWidth.enabled = (outMode == (int) OutputMode::PseudoStereo)
                          && *apvts.getRawParameterValue(Parameters::ID::stereoOn) > 0.5f;
    stereoWidth.width   = juce::jlimit(0.0f, 1.0f,  (float) (*apvts.getRawParameterValue(Parameters::ID::stereoWidth) + gMod[(size_t) LFOTarget::StereoWidth] * 1.0));
    stereoWidth.timeMs  = juce::jlimit(1.0f, 15.0f, (float) (*apvts.getRawParameterValue(Parameters::ID::stereoTime)  + gMod[(size_t) LFOTarget::StereoTime]  * 7.0));
    stereoWidth.process(buffer);

    // Master volume — gated by masterOn (Story 2.4): off => silent output. Base + MOD MATRIX offset,
    // applied as a per-block RAMP (prev→cur) so LFO-modulated VOL doesn't zipper.
    const bool  masterOn   = *apvts.getRawParameterValue(Parameters::ID::masterOn) > 0.5f;
    const float masterVol  = juce::jlimit(0.0f, 1.0f, (float) (apvts.getRawParameterValue(Parameters::ID::masterVol)->load() + gMod[(size_t) LFOTarget::MasterVol] * 1.0));
    const float masterGain = masterOn ? masterVol : 0.0f;
    buffer.applyGainRamp(0, buffer.getNumSamples(), prevMasterGain, masterGain);
    prevMasterGain = masterGain;
}

void SynthyProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

namespace
{
    // v4→v5 DAW-state migration: a host project saved before the MOD MATRIX DEST split still stores
    // <PARAM id="modSlotNTarget" value="<LFOTarget index>">. Translate each present slot into the new
    // modSlotNModule + modSlotNParam params so an old session keeps its routing (global
    // Pitch/Amp/Detune → "Alle OSC"). APVTS stores DENORMALISED values in the tree, so the legacy
    // "value" attribute is the LFOTarget index directly. Idempotent: skips an already-migrated slot.
    void migrateLegacyMatrixXml (juce::XmlElement& state)
    {
        using namespace Parameters;
        for (int n = 1; n <= ModMatrixConfig::kNumSlots; ++n)
        {
            auto* tgt = state.getChildByAttribute ("id", ID::modSlotTargetLegacy (n));
            if (tgt == nullptr) continue;                                          // not an old project
            if (state.getChildByAttribute ("id", ID::modSlotModule (n)) != nullptr) continue;   // done
            const int  legacy = (int) tgt->getDoubleAttribute ("value");          // LFOTarget index
            const auto mp = ModDest::fromLegacyTarget (legacy);
            auto* mEl = state.createNewChildElement ("PARAM");
            mEl->setAttribute ("id", ID::modSlotModule (n)); mEl->setAttribute ("value", (double) mp.module);
            auto* pEl = state.createNewChildElement ("PARAM");
            pEl->setAttribute ("id", ID::modSlotParam  (n)); pEl->setAttribute ("value", (double) mp.param);
        }
    }
}

void SynthyProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
    {
        migrateLegacyMatrixXml(*xml);   // v4→v5: SlotNTarget → SlotNModule + SlotNParam
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        // The just-restored state IS the clean baseline: snapshot it so a freshly loaded host
        // project does not spuriously report "modified" (isPresetModified compares to this).
        markPresetClean();
    }
}

juce::AudioProcessorEditor* SynthyProcessor::createEditor()
{
    return new SynthyEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SynthyProcessor();
}
