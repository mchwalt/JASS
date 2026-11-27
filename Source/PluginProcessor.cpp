#include "PluginProcessor.h"
#include "UI/PluginEditor.h"
#include "Audio/PresetIO.h"
#include <set>
#include <bit>
#if JucePlugin_Build_Standalone
 // Story 22.1: the standalone's device manager names the enabled MIDI inputs (profile matching).
 // getInstance() is nullptr in the VST3 and in JASS_Render.
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

namespace { const std::array<juce::String, 12>& crossModIdTable(); }   // defined below

SynthyProcessor::SynthyProcessor()
    : AudioProcessor(BusesProperties()
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", Parameters::createLayout())
{
    // Voices, choke-group roster and GRAIN seeds: PatchEngine's constructor (Story 21.4).

    // One-time rebrand of the app-data folder (%AppData%\Synthy -> JASS, *.synthy -> *.jass).
    // MUST run before anything touches jassFolder() (which would create JASS and suppress it).
    PresetIO::migrateLegacyAppData();

    // Ship the demo presets + example wavetables into the user's folders on first run (idempotent).
    PresetIO::seedDemoPresets();
    PresetIO::seedWavetables();
    PresetIO::seedSamples();     // Story 12.1: SAMPLER example recordings
    ControllerProfiles::seedFactoryProfiles();   // Story 22.1: MiniLab 3 + Generic (GM), only if missing
    // ... and pre-load them so the SET combo fills up — on a background thread (12.6), because
    // decoding every installed set (grand pianos: ~1.2 GB) held up the start of the app. The
    // hook lets a preset ask for ITS set: that one is pulled to the front of the queue and
    // selected once loaded, so restoring a piano patch no longer blocks the window either.
    PresetIO::hooks(apvts).requestSamplerSet = [this] (const juce::String& setName, const juce::String& target)
    {
        if (target == Parameters::ID::percKit)
            percKitPending.store (true);   // PERC stays silent until its kit is really here
        samplePreload.request (setName, target);
        samplePreload.notify();   // wake it if it is idling
    };
    PresetIO::hooks(apvts).cancelSamplerSet = [this] (const juce::String& target)
    {
        samplePreload.cancel (target);
        // A withdrawn request will never land, so nothing is pending for PERC any more: the caller
        // resolves the kit right here (it is already loaded). Without this the flag stayed set and
        // PERC silent for good — hit on the first start after 21.8, when the v12 conversion loads
        // every preset in turn and each one asks for its kit (maintainer 2026-11-23: "PERC stumm").
        if (target == Parameters::ID::percKit)
            percKitPending.store (false);
    };
    samplePreload.startThread (juce::Thread::Priority::background);

    // STEP SEQ latch (15.5): saving a patch stores the note a running figure is latched to, loading
    // one puts it back — so a sequencer patch plays itself the moment it is loaded. -1 on load means
    // "no latch" and stops whatever the previous patch left running. Starting the latch also ends
    // the auto-play drone, since the instrument is now playing.
    // A preset is a post-coupling snapshot: while one is applied, the parameter couplings stay
    // silent and their auto-enable memories are dropped (see setPresetLoading in the header).
    PresetIO::hooks(apvts).setPresetLoading  = [this] (bool loading) { setPresetLoading (loading); };
    PresetIO::hooks(apvts).setLoadKeepsLayerB = [this] (bool keep) { setLoadKeepsLayerB (keep); };
    PresetIO::hooks(apvts).patterns          = &patterns;   // AD-14: the PERC grid is read/written through here

    // Story 21.8: one STEP SEQ + ARP lane per layer — lane i plays layer i on its own channel.
    for (int i = 0; i < kNumLanes; ++i)
        lanes[(size_t) i].bind (apvts, i, i == 0 ? juce::String() : juce::String (Modules::kLayerBPrefix), patternChannelOf (i));

    PresetIO::hooks(apvts).seqLatchRoot      = [this] (int lane) { return lanes[(size_t) juce::jlimit (0, kNumLanes - 1, lane)].latchedRoot.load(); };
    PresetIO::hooks(apvts).applySeqLatchRoot = [this] (int lane, int note)
    {
        auto& l = lanes[(size_t) juce::jlimit (0, kNumLanes - 1, lane)];
        if (note >= 0)
        {
            startSeqLatch (l.index, note);
            l.requantize.store (true);   // a loaded latch always re-enters on the drums' downbeat
        }
        else
        {
            l.latchedRoot.store (-1);
            l.anchorPpq.store (-1.0);
        }
    };

    // Double-click-on-a-knob's baseline: the value the loaded preset gave that parameter.
    // cleanSnapshot is indexed like getParameters(), which is what getParameterIndex() names.
    PresetIO::hooks(apvts).presetBaseline01  = [this] (const juce::String& id) -> float
    {
        if (auto* p = apvts.getParameter (id))
            if (const auto i = (size_t) p->getParameterIndex(); i < cleanSnapshot.size())
                return cleanSnapshot[i];
        return -1.0f;
    };

    // Shift+reset's target (21.6): the Init patch, which differs from the declared defaults
    // where initPatchOverrides() says so — layer B's bass above all.
    PresetIO::hooks(apvts).factoryValue01 = [this] (const juce::String& id) -> float
    {
        for (const auto& [oid, plain] : initPatchOverrides())
            if (oid == id)
                if (auto* p = apvts.getParameter (id))
                    return p->convertTo0to1 (plain);
        return -1.0f;
    };

    // Listen for keypresses so the auto-play drone can step aside when played.
    keyboardState.addListener(this);

    crossModIdTable();   // build the CROSS MOD id table here, on the message thread, never on the audio thread (review)

    // Parameter couplings (MIX SRC distinctness, CROSS MOD and MOD MATRIX auto-enable) for BOTH
    // layers, and linked editing (21.6): every layer-A patch parameter — see listenedParamIds().
    // The A+B snapshot starts at the current values; parameterChanged keeps it current.
    {
        const auto& ids = Modules::layeredParamIds();
        linkLastA = std::vector<std::atomic<float>>(ids.size());   // atomics: written from any thread (review)
        for (size_t i = 0; i < ids.size(); ++i)
        {
            linkIndex[ids[i]] = (int) i;
            linkLastA[i].store(apvts.getRawParameterValue(ids[i])->load());
        }
    }
    for (const auto& id : listenedParamIds())
        apvts.addParameterListener(id, this);

    // The shared LiveState bridges the two standalone apps (C# <-> C++). In a
    // plugin host (e.g. REAPER) the host owns project state, so leave it alone.
    if (wrapperType == wrapperType_Standalone)
    {
        // One-time: bring old presets + LiveState up to the current format (flat=>nested, and fold
        // LFO built-in targets into matrix slots). Uses apvts as scratch; the LiveState load below
        // overwrites it. See PresetIO::convertOldPresets.
        PresetIO::convertOldPresets(apvts);

        // No LiveState (first start, or AppData cleared) means nothing loads and every parameter
        // sits at its RAW default — which is not the Init patch: OSC 1..3 off, MOD MATRIX on and
        // therefore revealed (its param default stays 1 for old presets, see resetToDefault). Run
        // the same reset the header button runs, so a first start looks and sounds like RESET
        // (maintainer 2026-10-19).
        auto loadLiveState = [this]() -> bool   // true = no LiveState, Init was run instead
        {
            const auto file = PresetIO::liveStateFile();
            if (! file.existsAsFile()) { resetToDefault(); return true; }
            PresetIO::loadFromFile(apvts, file);
            if (auto n = PresetIO::nameFromFile(file); n.isNotEmpty())
                currentPresetName = n;
            restoreModifiedState(PresetIO::modifiedFromFile(file));
            return false;
        };
        loadLiveState();
        apvts.state.addListener(this);
        startTimer(1500);

        // Story 22.1: controller profiles act here only (a DAW maps controllers itself). Own 30 Hz
        // timer: a knob must follow the hand, the LiveState period (1.5 s) is far too slow.
        controllerHandPick = PresetIO::getSetting ("ControllerProfile");
        rescanControllers (true);
        // The standalone holder opens the MIDI inputs only after the processor exists (its own
        // 500 ms timer), so the rescan above sees none: the first periodic rescan comes after ~1 s
        // (30 of 60 ticks), then every ~2 s.
        controllerTicks = 30;
        controllerPoll.tick = [this] { pollControllers(); };
        controllerPoll.startTimerHz (30);

        // The standalone wrapper restores its OWN saved state right after construction (the
        // "filterState" in JASS.settings, written at shutdown); re-load the shared LiveState
        // afterwards so it wins. The editor has been built against the wrapper's state by then —
        // tell it, so the rack layout follows the LiveState too (onStateReloaded).
        juce::WeakReference<SynthyProcessor> weak (this);
        juce::MessageManager::callAsync([this, weak, loadLiveState]
        {
            if (weak == nullptr) return;   // processor destroyed before this async ran
            const bool fromScratch = loadLiveState();
            if (onStateReloaded) onStateReloaded(fromScratch);
        });
    }
    else
    {
        // A fresh instance in a host is the Init patch too. Without this the instance sat at the
        // RAW defaults - OSC 1..3 off - and a track with a new JASS was silent until a preset was
        // loaded or RESET pressed (found 2026-11-04 by a headless bounce in SoundCraft: -144 dBFS
        // while another instrument on the same session played). A host that restores a project
        // calls setStateInformation right after construction and replaces all of this.
        resetToDefault();
    }
}

void SynthyProcessor::SamplePreloadThread::request (const juce::String& setName,
                                                    const juce::String& targetParamId)
{
    const juce::ScopedLock sl (requestLock);
    int& gen = generations[targetParamId];   // one counter per selector (A/B SAMPLER, PERC KIT)
    ++gen;   // anything still in flight for THIS selector is now stale
    for (auto& r : queue)
        if (r.target == targetParamId)     // one pending set per selector: replace, never queue up
        {
            r.name = setName; r.gen = gen;
            return;
        }
    queue.push_back ({ setName, targetParamId, gen });
}

void SynthyProcessor::SamplePreloadThread::cancel (const juce::String& targetParamId)
{
    const juce::ScopedLock sl (requestLock);
    ++generations[targetParamId];   // a fetch already running lands stale and is dropped
    queue.erase (std::remove_if (queue.begin(), queue.end(),
                                 [&] (const Request& r) { return r.target == targetParamId; }), queue.end());
}

int SynthyProcessor::SamplePreloadThread::currentGeneration (const juce::String& targetParamId) const
{
    const juce::ScopedLock sl (requestLock);
    const auto it = generations.find (targetParamId);
    return it != generations.end() ? it->second : 0;
}

// Load the set a preset asked for and select it — the ONE set the patch actually needs, so it
// goes first. Selecting happens back on the message thread; the generation check drops the
// result if another preset has been loaded since (its own request wins).
void SynthyProcessor::SamplePreloadThread::loadRequested (const Request& req)
{
    const juce::String setName = req.name;
    const int gen = req.gen;
    int idx = SampleBankStore::instance().indexOf (setName);
    if (idx < 0)
    {
        auto folder  = PresetIO::samplesFolder();
        auto matches = folder.findChildFiles (juce::File::findFiles, false, setName + ".*");
        if (! matches.isEmpty())
            idx = SampleBankStore::instance().loadFile (matches.getReference (0));
        else if (auto sub = folder.getChildFile (setName); sub.isDirectory())
            idx = PresetIO::loadSampleSetFolder (sub, nullptr, [this] { return threadShouldExit(); });
    }
    const juce::String target = req.target;
    if (idx < 0)
    {   // not in the Samples folder (renamed/removed): stop shielding the LiveState, or the
        // patch would keep claiming an instrument that does not exist.
        juce::WeakReference<SynthyProcessor> gone (&owner);
        juce::MessageManager::callAsync ([target, gone]
        {
            auto* p = gone.get();
            if (p == nullptr)
                return;
            auto& h = PresetIO::hooks (p->apvts);
            h.pendingFor (target).clear();
            // Give up waiting: PERC may sound again with whatever kit the index points at, rather
            // than staying silent forever because a preset named a set that is not installed.
            if (target == Parameters::ID::percKit)
                p->percKitPending.store (false);
        });
        return;
    }
    if (threadShouldExit())
        return;

    // Weak: the callback may still sit in the message queue when the processor goes away.
    juce::WeakReference<SynthyProcessor> weak (&owner);
    juce::MessageManager::callAsync ([weak, idx, gen, target]
    {
        if (auto* p = weak.get())
            p->selectSamplerSet (idx, gen, target);
    });
}

// Runs off the message thread (12.6). Everything it touches is thread-safe: PresetIO only reads
// the Samples folder, and the store serialises PUBLISHING a set (decoding stays parallel), so a
// preset request arriving mid-preload is fine. `threadShouldExit` is polled per zone, so closing
// the app cannot leave us decoding a gigabyte for nothing.
void SynthyProcessor::SamplePreloadThread::run()
{
    bool bulkDone = false;
    while (! threadShouldExit())
    {
        // 1. A preset is waiting for a set — always first: the patch is silent until it lands.
        if (auto wanted = takeRequest(); wanted.name.isNotEmpty())
        {
            loadRequested (wanted);
            continue;
        }
        // 2. Nothing urgent: fill the SET combo with everything else that is installed.
        if (! bulkDone)
        {
            PresetIO::preloadSamples ([this] { return threadShouldExit(); });
            bulkDone = true;
            continue;
        }
        // 3. Idle — stay alive for presets loaded later (an F-key can name a set that is not in
        //    the folder scan yet, e.g. one imported during this session). stopThread() wakes us.
        wait (500);
    }
}

SynthyProcessor::SamplePreloadThread::Request SynthyProcessor::SamplePreloadThread::takeRequest()
{
    const juce::ScopedLock sl (requestLock);
    if (queue.empty())
        return {};
    auto wanted = queue.front();
    queue.erase (queue.begin());
    return wanted;
}

void SynthyProcessor::selectSamplerSet (int index, int generation, const juce::String& targetParamId)
{
    if (generation != samplePreload.currentGeneration (targetParamId))
        return;   // a newer preset load has since asked for a different set in this selector
    // PERC's KIT counts from 1 (0 = no kit), the SAMPLER's SET from 0.
    const int value = index + (targetParamId == Parameters::ID::percKit ? 1 : 0);
    if (auto* p = apvts.getParameter (targetParamId))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) value));
    PresetIO::hooks(apvts).pendingFor (targetParamId).clear();
    if (targetParamId == Parameters::ID::percKit)
        percKitPending.store (false);   // the kit is real now — PERC may sound
}

SynthyProcessor::~SynthyProcessor()
{
    // Stop the loader before anything else goes away, and drop the hook first so a late preset
    // load cannot queue work into a dying thread. The abort is polled per zone, so this returns
    // in milliseconds even mid-piano.
    PresetIO::unregisterHooks (apvts);   // they capture `this` — gone before it dies; the other
                                         // instances in this process keep theirs (2026-10-20)
    samplePreload.stopThread (4000);

    keyboardState.removeListener(this);
    for (const auto& id : listenedParamIds())
        apvts.removeParameterListener(id, this);
    if (wrapperType == wrapperType_Standalone)
    {
        controllerPoll.stopTimer();
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
    if (midiChannel == kAuditionChannel || midiChannel == kAuditionChannelB)
    {
        // A previewed STEP SEQ step (15.3) is not a played key: it must not enter the held-note
        // set (the sequencer would take it as its root) and it must not move the FREQ display.
        // It does step on the auto-play drone, though — auditioning against a droning C4 is
        // exactly what one does not want to hear.
        autoPlayEnabled.store(false);
        return;
    }

    // 21.6 LAYERS · LEARN: the next played key becomes the split point. Runs on the audio thread
    // for MIDI, so only the note is handed over; the editor's timer writes the parameter.
    if (splitLearnArmed.exchange(false))
        learnedSplitNote.store(midiNote);

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
    if (midiChannel == kDroneChannel || midiChannel == kAuditionChannel || midiChannel == kAuditionChannelB)
        return;                          // neither one ever entered the held-note set
    if (midiNote >= 0 && midiNote < 128)
        (midiNote < 64 ? heldNotesLo : heldNotesHi).fetch_and(~(1ULL << (midiNote & 63)));
    if (heldNotesLo.load() == 0 && heldNotesHi.load() == 0)
        currentNoteRatio.store(1.0f);   // nothing held → FREQ knobs back to base
}

void SynthyProcessor::timerCallback()
{
#if JASS_HOSTSYNC_DIAG
    if (isHostedByDaw()) { flushDiag(); return; }
#endif
    if (liveDirty.exchange(false))
        saveLiveState();
}

#if JASS_HOSTSYNC_DIAG
void SynthyProcessor::diagNote (const char* what)
{
    PresetIO::jassFolder().getChildFile ("hostsync.log").appendText (
        juce::String::toHexString ((juce::pointer_sized_int) this) + " t=" + juce::String (juce::Time::getMillisecondCounterHiRes() / 1000.0, 2)
        + "  NOTE " + what + "\n");
}

void SynthyProcessor::flushDiag()
{
    const int w = diagWrite.load();
    if (w == diagRead) return;
    juce::String out;
    for (; diagRead < w; ++diagRead)
    {
        const auto& r = diag[(size_t) (diagRead % 256)];
        out << juce::String::toHexString ((juce::pointer_sized_int) this) << " t=" << juce::String (r.t / 1000.0, 2) << "  ppq=" << juce::String (r.ppq, 5)
            << " stepsPos=" << juce::String (r.stepsPos, 4) << " anchor=" << juce::String (r.anchor, 4)
            << " percAnchor=" << juce::String (r.percAnchor, 4) << " playing=" << r.playing
            << " seqOn=" << r.seqOn << " latch=" << r.latch << " percOn=" << r.percOn << " kitPending=" << r.kitPending
            << " kitIdx=" << r.kitIdx << " percStep=" << r.percStep
            << " pre(idx=" << r.preIdx << ",cnt=" << r.preCnt << ")"
            << " post(idx=" << r.postIdx << ",cnt=" << r.postCnt << ",delay=" << r.postDelay << ")"
            << " fired=" << r.fired << " n=" << r.n << "\n";
    }
    PresetIO::jassFolder().getChildFile ("hostsync.log").appendText (out);
}
#endif

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
            case ModSource::ChaosX:
            case ModSource::ChaosY:   return ID::chaosOn;
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
    // AD-16 (21.6): both layers' matrices. A layer's patch modules are its own ("b"-prefixed for B)
    // and B's claims on them count ALWAYS - switching Single to A must not edit B's patch (review
    // 2026-11-18). An instrument module (COMPRESSOR, STEREO, MASTER, CHAOS) is shared: B's claim on
    // it counts while B sounds (live keys or a pattern aimed at it); a claim from either keeps it on.
    const auto playingNow = layersPlaying();
    const bool bOn = playingNow.b || lanes[1].sounding();   // 21.8: B's own running figure makes it sound
    std::set<juce::String> claimed;
    for (const juce::String pre : { juce::String(), juce::String(Modules::kLayerBPrefix) })
    {
        auto claim = [&] (const juce::String& id)
        {
            if (Modules::isLayeredParamId(id)) claimed.insert(pre + id);
            else if (pre.isEmpty() || bOn)     claimed.insert(id);
        };
        auto L = [&pre] (const juce::String& id) { return Modules::isLayeredParamId(id) ? pre + id : id; };
        for (int n = 1; n <= ModMatrixConfig::kNumSlots; ++n)
        {
            const int mod = (int) *apvts.getRawParameterValue(L(ID::modSlotModule(n)));
            if (mod <= 0)   // 0 == "Off"
                continue;
            if (const auto s = matrixSourceEnableParam((int) *apvts.getRawParameterValue(L(ID::modSlotSource(n)))); s.isNotEmpty())
                claim(s);
            if (const auto t = matrixModuleEnableParam(mod); t.isNotEmpty())   // per-OSC -> oscNOn; "Alle OSC" -> none
                claim(t);
        }
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
                                     ID::compOn, ID::stereoOn, ID::masterOn,
                                     ID::chaosOn,     // LFO expansion: Chaos X/Y source
                                     ID::grainOn };   // Story 17.1: GRAIN targets
    // ...for layer A as written, and layer B's copies of the patch ones (21.6).
    juce::StringArray managedAll;
    for (const auto& id : managed)
    {
        managedAll.addIfNotAlreadyThere(id);
        if (Modules::isLayeredParamId(id))
            managedAll.add(juce::String(Modules::kLayerBPrefix) + id);
    }
    for (const auto& id : managedAll)
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

void SynthyProcessor::syncCrossModEnables(const juce::String& changed, const juce::String& pre)
{
    using namespace Parameters;
    auto isOn = [this](const juce::String& id) { return *apvts.getRawParameterValue(id) >= 0.5f; };
    auto setP = [this](const juce::String& id, float v)
                { if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(v); };
    auto L = [&pre] (const juce::String& id) { return pre + id; };   // 21.6: the layer's own CROSS MOD

    const int a = juce::jlimit(0, 2, (int) *apvts.getRawParameterValue(L(ID::mixSrcA)));
    const int b = juce::jlimit(0, 2, (int) *apvts.getRawParameterValue(L(ID::mixSrcB)));
    const juce::String oscA = L(ID::oscOn(a + 1));
    const juce::String oscB = L(ID::oscOn(b + 1));
    const bool mixOn = isOn(L(ID::mixModeOn));

    const bool oscChanged = (changed == L(ID::oscOn(1)) || changed == L(ID::oscOn(2)) || changed == L(ID::oscOn(3)));

    // A used operand OSC was switched OFF → CROSS MOD can no longer work, so switch it off too
    // (previously it was only shown dimmed while mixModeOn stayed set). Then undo the operands we
    // auto-enabled ourselves (an OSC that was on for its own sake keeps its state — see below).
    // The auto-enable memory is keyed by full id, so A's and B's entries never mix; a layer only
    // ever undoes its own (the prefix test below).
    auto ownLayer = [&pre] (const juce::String& id)
    {
        const bool isB = id.startsWith(Modules::kLayerBPrefix)
                      && Modules::isLayeredParamId(id.substring((int) std::strlen(Modules::kLayerBPrefix)));
        return isB == pre.isNotEmpty();
    };
    if (mixOn && oscChanged && (changed == oscA || changed == oscB) && (! isOn(oscA) || ! isOn(oscB)))
    {
        setP(L(ID::mixModeOn), 0.0f);
        for (auto& kv : crossModAutoEnabled)
            if (ownLayer(kv.first) && kv.second && isOn(kv.first))
                { setP(kv.first, 0.0f); kv.second = false; }
        return;
    }

    // CROSS MOD toggled or an operand re-selected: while on, keep BOTH operand OSCs enabled
    // (remember the ones we switch on); when off, undo exactly those again. An OSC that was
    // already on (its own voice) is not remembered, so it is never auto-disabled.
    const bool routeChanged = (changed == L(ID::mixModeOn) || changed == L(ID::mixSrcA) || changed == L(ID::mixSrcB));
    if (routeChanged)
    {
        if (mixOn)
        {
            for (const auto& osc : { oscA, oscB })
                if (! isOn(osc)) { setP(osc, 1.0f); crossModAutoEnabled[osc] = true; }
            // an operand we auto-enabled earlier but no longer use (operand switched) → undo it
            for (auto& kv : crossModAutoEnabled)
                if (ownLayer(kv.first) && kv.second && kv.first != oscA && kv.first != oscB && isOn(kv.first))
                    { setP(kv.first, 0.0f); kv.second = false; }
        }
        else
        {
            for (auto& kv : crossModAutoEnabled)
                if (ownLayer(kv.first) && kv.second)
                {
                    if (isOn(kv.first)) setP(kv.first, 0.0f);
                    kv.second = false;
                }
        }
    }
}

namespace
{
    // Encode a CROSS-MOD-relevant param id as a small int WITHOUT constructing a juce::String
    // (audio-thread safe). -1 = not a CROSS-MOD param. Reverse map below (message thread only).
    // Codes 0..5 = layer A's six CROSS-MOD params, 6..11 = layer B's (21.6). The table is built
    // once (warmed in the constructor, on the message thread), so the audio thread only compares.
    const std::array<juce::String, 12>& crossModIdTable()
    {
        static const std::array<juce::String, 12> t = []
        {
            using namespace Parameters;
            std::array<juce::String, 12> a;
            const juce::String base[6] = { ID::mixModeOn, ID::mixSrcA, ID::mixSrcB, ID::oscOn(1), ID::oscOn(2), ID::oscOn(3) };
            for (int i = 0; i < 6; ++i)
            {
                a[(size_t) i]     = base[i];
                a[(size_t) i + 6] = juce::String(Modules::kLayerBPrefix) + base[i];
            }
            return a;
        }();
        return t;
    }
    int crossModCode(const juce::String& id)
    {
        const auto& t = crossModIdTable();
        for (int i = 0; i < 12; ++i)
            if (id == t[(size_t) i]) return i;
        return -1;
    }
    juce::String crossModIdFromCode(int c)   // message thread (juce::String construction OK)
    {
        return juce::isPositiveAndBelow(c, 12) ? crossModIdTable()[(size_t) c] : juce::String();
    }
}

void SynthyProcessor::parameterChanged(const juce::String& paramId, float newValue)
{
    // APVTS calls this synchronously on the changing thread — the AUDIO thread under host automation.
    // Off the message thread we do NO allocation / setValueNotifyingHost: just flag the needed
    // reconciliation (atomic) and let reconcileParamCouplingsIfDirty() run it on the message thread.
    const bool onMsgThread = juce::MessageManager::existsAndIsCurrentThread();

    // Linked editing (21.6, focus A+B): an edit of a layer-A patch parameter is mirrored to B —
    // relative for knobs (B keeps its offset), absolute for switches and choices. The snapshot of
    // A's last values is kept current on EVERY change (any thread, alloc-free), so the delta is
    // always against what A was a moment ago. Mirroring itself is a message-thread gesture only:
    // not under host automation, not while a preset lands, not during RANDOM.
    if (auto it = linkIndex.find(paramId); it != linkIndex.end())
    {
        const float before = linkLastA[(size_t) it->second].exchange(newValue);
        // A module's on/off switch is not mirrored (maintainer 2026-11-23): in the A+B overall
        // view it acts on the layer whose frame is on screen, like the rest of that frame.
        if (onMsgThread && ! presetLoading.load() && ! linkSuspended && getLayerFocus() == 2
            && ! Modules::isLayeredEnableParamId(paramId))
            mirrorToLayerB(paramId, newValue - before);
    }

    // A preset being applied is a post-coupling snapshot — apply it verbatim, run no couplings
    // (see setPresetLoading). Without this guard the load itself triggered them against
    // half-applied states, and the auto-enable memories made the outcome depend on the patch
    // loaded BEFORE — presets sometimes needed two or three loads to arrive intact.
    if (presetLoading.load())
        return;

    // Either layer's slots (21.6: "bmodSlot…"), and B switching on or off (its claims count only
    // while it plays).
    if (paramId.startsWith("modSlot") || paramId.startsWith("bmodSlot") || paramId == Parameters::ID::layerMode || paramId == Parameters::ID::layerFocus)
    {
        if (onMsgThread) updateMatrixModuleEnables();
        else             matrixEnablesDirty.store(true);
        return;
    }

    // ARP and STEP SEQ both REPLACE the keys of their layer, so only one of a layer's pair can run
    // (21.8: per layer — A's ARP and B's STEP SEQ run together). Switching one ON switches the other
    // OFF, which keeps the rack honest: no lit module sitting there doing nothing. Writing a
    // parameter is a message-thread job (setValueNotifyingHost allocates and notifies), so under
    // host automation this only raises the flag. A pattern in B also makes B sound, which the
    // matrix's shared-module claims depend on — so the switch reconciles those as well.
    for (const auto& l : lanes)
        if (paramId == l.seqOnId || paramId == l.arpOnId)   // cached ids: no String built here (review)
        {
            if (l.index != 0)
            {
                if (onMsgThread) updateMatrixModuleEnables();
                else             matrixEnablesDirty.store(true);
            }
            if (newValue <= 0.5f)          // switching OFF frees the other one; nothing to reconcile
                return;
            pendingExclusive.fetch_or(1u << (l.index * 2 + (paramId == l.seqOnId ? 1 : 0)));
            if (onMsgThread) applySeqArpExclusion();
            else             seqArpDirty.store(true);
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
    if (seqArpDirty.exchange(false))
        applySeqArpExclusion();
}

// Message thread only: the module that was just switched on wins, the other one OF THE SAME
// LAYER is switched off (21.8).
void SynthyProcessor::applySeqArpExclusion()
{
    using namespace Parameters;
    const std::uint32_t mask = pendingExclusive.exchange(0);
    for (const auto& l : lanes)
    {
        const bool seqWon = (mask >> (l.index * 2 + 1)) & 1u;
        const bool arpWon = (mask >> (l.index * 2)) & 1u;
        if (! seqWon && ! arpWon)
            continue;
        // Both switched on before this ran: STEP SEQ wins, as it does inside the block (arpActive).
        const juce::String& loser = seqWon ? l.arpOnId : l.seqOnId;
        if (auto* p = apvts.getParameter(loser))
            if (*apvts.getRawParameterValue(loser) > 0.5f)
                p->setValueNotifyingHost(0.0f);
    }
}

// The CROSS-MOD coupling body (message thread only): operand distinctness + enable coupling.
void SynthyProcessor::applyCrossModCoupling(const juce::String& paramId)
{
    using namespace Parameters;
    if (paramId.isEmpty())
        return;
    if (fixingMixSrc.exchange(true))   // shared reentrancy guard: ignore our own write-backs
        return;

    // 21.6: which layer's CROSS MOD — B's ids carry the prefix.
    const juce::String pre = (crossModCode(paramId) >= 6) ? juce::String(Modules::kLayerBPrefix) : juce::String();
    const juce::String srcA = pre + ID::mixSrcA, srcB = pre + ID::mixSrcB;

    // Keep the two operands distinct (Epic 5): a==b would be a no-op, so bump the OTHER to a free OSC.
    if (paramId == srcA || paramId == srcB)
    {
        const int a = (int) *apvts.getRawParameterValue(srcA);
        const int b = (int) *apvts.getRawParameterValue(srcB);
        if (a == b)
        {
            const juce::String& other = (paramId == srcA) ? srcB : srcA;
            if (auto* p = apvts.getParameter(other))
                p->setValueNotifyingHost(p->convertTo0to1((float) (a == 0 ? 1 : 0)));   // first OSC != a
        }
    }

    // Enable coupling: auto on/off of the operand OSCs; drop CROSS MOD when an operand goes off.
    syncCrossModEnables(paramId, pre);

    fixingMixSrc = false;
}

// RANDOM rolls the layer being edited (21.6): A, B, or with A+B both, independently. Layer A's
// roll is the long-standing one below (with all its guards); B's is that same roll, carried over
// to B's parameters, with A and the instrument put back — one set of dice rules, not two.
void SynthyProcessor::randomize()
{
    const juce::ScopedValueSetter<bool> noLink (linkSuspended, true);   // the rolls are not edits to mirror
    const int focus = getLayerFocus();
    if (focus != 0)
    {
        std::vector<float> keep;
        for (auto* p : getParameters()) keep.push_back(p->getValue());
        const auto memMatrix = matrixAutoEnabled;    // layer A's auto-enable memories before the roll
        const auto memCross  = crossModAutoEnabled;
        randomizeLayerA();   // couplings run here, as for an A roll - the roll is post-coupling after
        const juce::String bp (Modules::kLayerBPrefix);
        {
            // The copy to B and the restore of A are snapshots, not gestures: no coupling may fire
            // against the half-restored state (review 2026-11-18) - like a preset load, but the
            // auto-enable memories are handed over by hand below instead of cleared.
            presetLoading.store(true);
            for (const auto& id : Modules::layeredParamIds())
            {
                // 21.8: B's STEP SEQ / ARP switches are performance, like A's (the A roll keeps both).
                if (id == Parameters::ID::seqOn || id == Parameters::ID::arpOn) continue;
                if (auto* a = apvts.getParameter(id))
                    if (auto* b = apvts.getParameter(bp + id))
                        b->setValueNotifyingHost(a->getValue());
            }
            auto& params = getParameters();
            for (int i = 0; i < params.size(); ++i)
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(params[i]))
                    if (! (rp->paramID.startsWith(bp) && Modules::isLayeredParamId(rp->paramID.substring(bp.length()))))
                        rp->setValueNotifyingHost(keep[(size_t) i]);
            presetLoading.store(false);
        }
        // B inherits the roll's memories, A gets its own back.
        for (auto* mem : { &matrixAutoEnabled, &crossModAutoEnabled })
        {
            const auto& before = (mem == &matrixAutoEnabled) ? memMatrix : memCross;
            std::map<juce::String, bool> merged;
            for (const auto& [id, on] : before)
                if (! (id.startsWith(bp) && Modules::isLayeredParamId(id.substring(bp.length()))))
                    merged[id] = on;                                     // A's (and shared) as before
            for (const auto& [id, on] : *mem)
                if (Modules::isLayeredParamId(id)) merged[bp + id] = on; // the roll's A memory -> B
            *mem = std::move(merged);
        }
    }
    if (focus != 1)
        randomizeLayerA();
    currentPresetName = "Random";
    markPresetClean();   // a fresh random patch is its own "clean" state
}

void SynthyProcessor::randomizeLayerA()
{
    using namespace Parameters;
    auto& rng = juce::Random::getSystemRandom();

    // The whole MASTER BUS zone (MASTER + STEREO + COMPRESSOR) is mastering/output, not sound
    // design → RANDOM must leave it untouched. A random masterOn=off would MUTE the patch and a
    // random syncTempo would reharmonise every tempo-synced LFO/delay. Snapshot the zone in full,
    // restore it verbatim after the dice roll.
    static const char* const masterBusIds[] = {
        ID::masterOn, ID::masterVol, ID::syncTempo,
        ID::stereoOn, ID::stereoWidth, ID::stereoTime, ID::outputMode, ID::hrtfRoom,
        ID::compOn, ID::compThreshold, ID::compRatio, ID::compAttack, ID::compRelease, ID::compMakeup };
    const int numMasterBus = (int) (sizeof (masterBusIds) / sizeof (masterBusIds[0]));
    float keepMasterBus[sizeof (masterBusIds) / sizeof (masterBusIds[0])];
    for (int i = 0; i < numMasterBus; ++i)
        keepMasterBus[i] = *apvts.getRawParameterValue (masterBusIds[i]);

    const float keepArpOn       = *apvts.getRawParameterValue(ID::arpOn);   // arp = performance, not sound design
    const float keepSeqOn       = *apvts.getRawParameterValue(ID::seqOn);   // so is the STEP SEQ (review E2, 2026-11-24)
    const float keepGlideOn     = *apvts.getRawParameterValue(ID::glideOn); // glide = performance, not sound design
    const float keepKeyboardOn  = *apvts.getRawParameterValue(ID::keyboardOn); // keyboard = input surface, not sound design

    // Random value for every parameter... (the MOD MATRIX is re-rolled CONTROLLED below —
    // it is the synth's central movement layer, so RANDOM must vary it, not leave it alone.)

    // The roll of layer A's parameters: layer B and the LAYERS switches stay as they are here —
    // randomize() carries this roll over to B when B is being edited (21.6).
    auto isLayerBOrLayers = [] (const juce::String& id)
    {
        if (id.startsWith ("layer") || id == ID::splitNote) return true;
        return id.startsWith (Modules::kLayerBPrefix)
            && Modules::isLayeredParamId (id.substring ((int) std::strlen (Modules::kLayerBPrefix)));
    };
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p); rp == nullptr || ! isLayerBOrLayers (rp->paramID))
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
    set(ID::seqOn,       keepSeqOn);
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
            case ModSource::ChaosX:
            case ModSource::ChaosY: set(ID::chaosOn, 1.0f); break;
        }
    }

}

const std::vector<std::pair<juce::String, float>>& SynthyProcessor::initPatchOverrides()
{
    static const std::vector<std::pair<juce::String, float>> table = []
    {
        namespace ID = Parameters::ID;
        const juce::String b (Modules::kLayerBPrefix);
        std::vector<std::pair<juce::String, float>> t;
        // MOD MATRIX off in layer A's Init (maintainer 2026-10-03): with every slot empty it does
        // nothing to the sound, and a module that is on must be visible — so an on-but-unused
        // matrix would sit in every Init rack. Its PARAM default stays 1 on purpose: presets from
        // before the enable existed carry no "ModMatrixOn" and must keep routing (missing ⇒
        // default, AD-11 pattern). Layer B's Init USES its matrix (below).
        t.push_back({ ID::modMatrixOn, 0.0f });
        // Layer A: the octave stack OSC 1..3 (saw C4, sine C5, sine C3), all on.
        for (int i = 1; i <= 3; ++i)
            t.push_back({ ID::oscOn(i), 1.0f });
        // Layer B (maintainer 2026-11-18: both layers sound at once in a Split, clearly different,
        // and the bass "satt" like the maintainer's reference bass.wav). B sits below the split,
        // under the left hand. Fitted on the render rig against that reference (E2, harmonics in
        // four time windows, level): one sawtooth (its attack spectrum is a plain 1/n series, no
        // sub), a lowpass that a SEPARATE envelope sweeps from ~3 octaves open down to the
        // fundamental within ~0.3 s — the punch — while the amp envelope holds the note. The
        // shared ADSR cannot do both (closing the filter fast enough dropped the level 10 dB by
        // 200 ms), so the filter sweep is LFO 1 in One-Shot mode through MOD MATRIX slot 1.
        // TRACK 100 % keeps the sweep the same across the keys (a fixed 80 Hz choked the upper
        // notes, -9 dB against A); 250 Hz at C4 = ~80 Hz at E2.
        t.push_back({ b + ID::oscOn(1),   1.0f });
        t.push_back({ b + ID::oscWave(1), 1.0f });      // Sawtooth
        t.push_back({ b + ID::oscFreq(1), 261.63f });   // at the key (C4)
        t.push_back({ b + ID::oscAmp(1),  1.0f });      // one oscillator against A's three
        t.push_back({ b + ID::oscOn(2),   0.0f });
        t.push_back({ b + ID::oscOn(3),   0.0f });
        t.push_back({ b + ID::filterOn,     1.0f });
        t.push_back({ b + ID::filterType,   0.0f });    // Lowpass
        t.push_back({ b + ID::filterCutoff, 250.0f });
        t.push_back({ b + ID::filterReso,   0.9f });
        t.push_back({ b + ID::filterTrack,  100.0f });
        t.push_back({ b + ID::attack,  0.003f });
        t.push_back({ b + ID::decay,   0.4f });
        t.push_back({ b + ID::sustain, 0.6f });
        t.push_back({ b + ID::release, 0.12f });
        t.push_back({ b + ID::lfoOn(1),    1.0f });
        t.push_back({ b + ID::lfoWave(1),  5.0f });     // One-Shot: 1 -> 0 once per note
        t.push_back({ b + ID::lfoRate(1),  3.5f });     // ~0.29 s sweep
        t.push_back({ b + ID::lfoDepth(1), 1.0f });
        t.push_back({ b + ID::modMatrixOn, 1.0f });
        t.push_back({ b + ID::modSlotSource(1), 0.0f });   // LFO 1
        t.push_back({ b + ID::modSlotModule(1), (float) ModDest::moduleIndexForLabel ("FILTER") });
        t.push_back({ b + ID::modSlotParam(1),  0.0f });   // CUTOFF
        t.push_back({ b + ID::modSlotAmount(1), 1.0f });   // +3 octaves at the attack
        return t;
    }();
    return table;
}

void SynthyProcessor::resetToDefault()
{
    // Reset every parameter to its default, then enable all three oscillators
    // so the octave-stack default tuning (C3/C4/C5) sounds full immediately —
    // the auto-play drone now drives the whole stack.
    for (auto* p : getParameters())
        p->setValueNotifyingHost(p->getDefaultValue());
    patterns.perc.clear();   // AD-14: the patterns are no parameters, so the loop above cannot reach them
    for (auto& fig : patterns.steps) fig.clear();

    for (const auto& [id, plain] : initPatchOverrides())
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(plain));

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
    cleanPercRevision = patterns.perc.revision.load();   // AD-14: a changed cell is a change too
    patterns.perc.markClean();                            // 7.6: the grid as loaded, for PERC's ↺
    for (size_t i = 0; i < patterns.steps.size(); ++i)    // ...and each figure's double-click / ↺ baseline
    {
        patterns.steps[i].markClean();
        cleanStepRevision[i] = patterns.steps[i].revision.load();
    }
}

void SynthyProcessor::markLayerBClean()
{
    // A load into B (21.6): B's module resets and double-clicks go back to the sound just loaded,
    // A's stay on A's preset. Without a baseline (a modified LiveState) there is nothing to anchor.
    auto& params = getParameters();
    if (cleanSnapshot.size() != (size_t) params.size())
        return;
    const juce::String bp (Modules::kLayerBPrefix);
    for (int i = 0; i < params.size(); ++i)
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(params[i]))
            if (rp->paramID.startsWith(bp) && Modules::isLayeredParamId(rp->paramID.substring(bp.length())))
                cleanSnapshot[(size_t) i] = rp->getValue();
    patterns.steps[1].markClean();   // 21.8: B's figure is part of the sound just loaded
    cleanStepRevision[1] = patterns.steps[1].revision.load();
}

bool SynthyProcessor::isPresetModified() const
{
    auto& params = getParameters();
    if (cleanSnapshot.size() != (size_t) params.size())
        return true;   // no baseline (e.g. restored as a modified working state)
    if (patterns.perc.revision.load() != cleanPercRevision)
        return true;   // a pattern cell changed since the baseline (AD-14)
    for (size_t i = 0; i < patterns.steps.size(); ++i)
        if (patterns.steps[i].revision.load() != cleanStepRevision[i])
            return true;
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
    {
        cleanSnapshot.clear();
        patterns.perc.invalidateBaseline();   // 7.6: no "as loaded" state ⇒ the ↺ falls back to factory
        for (auto& fig : patterns.steps) fig.invalidateBaseline();
    }
    else
        markPresetClean();
}

void SynthyProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
#if JASS_HOSTSYNC_DIAG
    if (isHostedByDaw()) diagNote ("prepareToPlay");
#endif
    Parameters::ID::warmIndexedIds();   // RT-safety: build the indexed-ID caches on the message thread
    engineA.prepare(sampleRate, samplesPerBlock);   // 21.4: voices, glide lists, display LFOs
    engineB.prepare(sampleRate, samplesPerBlock);   // 21.5: layer B, ready whether or not it is on
    layerMidiA.ensureSize(2048); layerMidiB.ensureSize(2048);   // RT: the router never grows them
    layerBRender.setSize(2, juce::jmax(1, samplesPerBlock));   // B's render, summed into the mix
    layerRouter.reset();
    arpKeptScratch.ensureSize(2048);   // RT: no per-block MidiBuffer growth

    stereoWidth.prepare(sampleRate);
    binauralRoom.prepare(sampleRate);
    wasKunstkopf = false;
    compressor.prepare(sampleRate);
    prevMasterGain = 0.0f;   // ramp start for the (possibly modulated) master gain
    chaos.setSampleRate(sampleRate);
    for (auto& l : lanes) l.prepare(sampleRate);   // 21.8: each layer's STEP SEQ + ARP
    perc.prepare(sampleRate);   // 16.1: four SamplePlayers, configured once (their stretch engine allocates)

    // Feed the real sample rate to the scope/spectrum displays (ms-window + bin→Hz).
    waveformCapture.setSampleRate(sampleRate);
}

void SynthyProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // A preset load kills every sounding voice (flag set by setPresetLoading). Nothing else does:
    // a note held across the switch kept its voice ringing with the NEW patch's parameters — and
    // once the new patch runs STEP SEQ, the chord filter below drops the key's channel-1 note-off,
    // so the voice hung FOREVER (GrandPiano → DAF Beat left a saw drone under the beat,
    // maintainer 2026-08-24). Hard stop, no tail: the old patch's tail through the new patch's
    // processing is the very sound being removed. allNotesOff walks the voices directly —
    // RT-safe, and immune to the chord filter since no buffer event is involved.
    if (const int kill = killVoicesRequested.exchange (0); kill != 0)
    {
        engineA.killAllVoices();
        if (kill == 1)   // 2: a single-layer load keeps layer B playing (its figure runs on)
            engineB.killAllVoices();
        // The drone's voice just died with the rest — reset its bookkeeping too, or a patch that
        // WANTS the drone (no latch, nothing held) would stay silent: autoNoteOn would still say
        // "sounding" and nothing would ever re-trigger it. The noteOff clears the keyboard state;
        // the drone logic below then re-triggers it within this very block if it is wanted.
        if (autoNoteOn)
        {
            keyboardState.noteOff (kDroneChannel, kDroneNote, 0.0f);
            autoNoteOn = false;
        }
    }

    // Which sound generators are currently enabled (one bit each).
    unsigned mask = 0;
    for (int i = 1; i <= 3; ++i)
        if (*focusedEngine().params().oscOn[(size_t) i - 1] > 0.5f) mask |= (1u << i);
    if (*focusedEngine().params().karplusOn   > 0.5f) mask |= (1u << 4);   // AD-16: the drone plays
    if (*focusedEngine().params().noiseOn     > 0.5f) mask |= (1u << 5);   // the layer being edited
    if (*focusedEngine().params().wavetableOn > 0.5f) mask |= (1u << 6);
    if (*focusedEngine().params().subOn       > 0.5f) mask |= (1u << 7);

    // A newly enabled generator re-arms the auto-play drone (rising edge) — but only while the
    // instrument is otherwise silent: never under a LATCHED figure (a held C4 under a running
    // bass line is the very confusion the latch removed, 15.5), and never while keys are held
    // (the keys already sound the patch). This matters because EVERY preset load fires this edge
    // — the factory reset drops the generator mask to zero and the file's enables raise it again,
    // racing this code over several blocks — so the edge re-armed the drone AFTER startSeqLatch
    // (or a held key) had just said no: a saw C4 hung under the beat until the next keypress
    // chased it away (maintainer 2026-08-24, holding a key while switching DAF Beat ⇄ Los Ninos).
    if ((mask & ~prevSourcesMask) != 0
        && ! isSeqLatched()
        && heldNotesLo.load() == 0 && heldNotesHi.load() == 0)
        autoPlayEnabled.store(true);
    prevSourcesMask = mask;

    // Drone note 60 (C4) while auto-play is armed and a source is on. The user
    // playing a key clears autoPlayEnabled (see handleNoteOn) → drone steps aside.
    // NEVER in a DAW (AD-13, 2026-10-11): an instrument on a track sounds when the track sends
    // notes, not by itself — and the drone kept one voice with its whole per-voice effect chain
    // rendering in every idle instance (maintainer in Waveform: "ein laufender JASS beansprucht
    // 100% CPU"). The standalone keeps it: there the drone is how a patch is auditioned.
    bool wantDrone = ! isHostedByDaw() && autoPlayEnabled.load() && (mask != 0);
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

    // Story 22.1: hardware knobs / faders — the standalone only records them here; the message
    // thread maps and applies them (pollControllers).
    if (! isHostedByDaw())
        captureControllers (midiMessages);

    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);

    // Tempo-Sync (Feature): resolve the effective LFO rate + delay time ONCE per block.
    // BPM = the host's tempo when hosted (VST3/DAW), else the internal Sync Tempo knob
    // (Standalone). A division of "Free" (0) keeps the module's own free-running knob.
    double syncBpm = *apvts.getRawParameterValue(Parameters::ID::syncTempo);
    // 18.6: in a DAW the host owns the CLOCK, not just the tempo (AD-13). Transport state and the
    // position in beats are read once per block: the sequencers lock their step to the position
    // (StepSequencer::syncToHost / PercSequencer::syncToHost - two instances on two tracks then
    // agree by construction) and stand while the host stands. A host without position info, and
    // the standalone, keep the free clocks exactly as before.
    const bool hosted = isHostedByDaw();
    bool hostPlaying = true;
    juce::Optional<double> hostPpq;   // JUCE's own Optional (what getPpqPosition returns)
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto hostBpm = pos->getBpm())
                syncBpm = *hostBpm;
            if (hosted)
            {
                hostPlaying = pos->getIsPlaying();
                hostPpq     = pos->getPpqPosition();
            }
        }
    const bool hostRunning = ! hosted || hostPlaying;
    // 18.6: STOP freezes the sequencers, PLAY continues them - wherever the host's playhead is
    // by then. The first cut counted both from the song's start, and a figure of three bars then
    // sat at step 0 only every third bar: PLAY at bar 51 began it at its last note. The second
    // cut anchored step 0 at PLAY's position, but Bitwig's STOP returns the playhead to where
    // PLAY started, so every PLAY was "the same spot" and began the figure afresh (maintainer
    // 2026-10-25: "nach Stopp beginnt es wieder von vorne"). So the anchor is moved on the PLAY
    // edge by the playhead's jump, which keeps the phase STOP left behind: the drum machine's
    // pause/continue, in host beats. Two instances on two tracks see the same edge and keep
    // their relative phase. A clip note still moves the figure's anchor to the note (below);
    // the first PLAY of a session starts at step 0. Loops and locates WHILE playing are not
    // edges: the clock then follows the host's position against the anchor, as before.
    if (hosted && hostPlaying && hostPpq.hasValue())
    {
        if (! hostWasPlaying)
        {
            auto continueAnchor = [&] (std::atomic<double>& a)
            {
                const double cur = a.load();
                a.store (cur >= 0.0 && hostLastPpq >= 0.0 ? *hostPpq - (hostLastPpq - cur)   // continue
                                                            : *hostPpq);                       // first PLAY: step 0
            };
            continueAnchor (percAnchorPpq);
            for (auto& l : lanes) continueAnchor (l.anchorPpq);
        }
        hostLastPpq = *hostPpq + buffer.getNumSamples() * syncBpm / (60.0 * juce::jmax (1.0, getSampleRate()));
    }
    hostWasPlaying = hosted && hostPlaying;
#if JASS_HOSTSYNC_DIAG
    if (hosted && ! isTimerRunning()) startTimer (500);
    diagSeqPre[0] = diagSeqPre[1] = -1;   // lane A's clock around its host sync, filled in laneOutput
    diagSeqPost[0] = diagSeqPost[1] = diagSeqPost[2] = -1;
    const bool diagPlayEdge = hosted && hostPlaying && ! diagWasPlaying;
    diagWasPlaying = hostPlaying;
#endif
    // The host's STOP stands the figure still but KEEPS its latched root (it is part of the
    // project state too, see getStateInformation): PLAY from anywhere runs it again at the
    // host's position, with or without a note from the clip. A first version cleared the latch
    // on STOP and relied on the clip re-sending its note; Bitwig does not chase a note the
    // playhead starts inside of, and a reopened project had no root at all until a note-on
    // arrived - the bass track fell silent (maintainer 2026-10-19/20).

    // CHAOS: advance the one global attractor and publish this block's snapshot. Published as 0
    // when the module is off (a routed-but-silent source, same rule as the LFOs). Advanced BEFORE
    // the global offsets below so bus targets read the current block, not the previous one.
    {
        using namespace Parameters;
        if (*apvts.getRawParameterValue(ID::chaosOn) > 0.5f)
        {
            chaos.setRate(*apvts.getRawParameterValue(ID::chaosRate));
            chaos.advance(buffer.getNumSamples());
            chaosDisplay[0].store(chaos.outX());
            chaosDisplay[1].store(chaos.outY());
        }
        else
        {
            chaosDisplay[0].store(0.0f);
            chaosDisplay[1].store(0.0f);
        }
    }

    // ── Global (master-bus) modulation offsets ──────────────────────────────────────────────
    // STEREO / MASTER / COMPRESSOR run on the SUMMED mix (further below), not per voice, so their
    // MOD MATRIX routings are applied HERE at block rate. Source values are the GLOBAL uiLfo values
    // from the PREVIOUS block (the engines display LFOs, advanced after the render) — a one-block lag that is
    // inaudible. LFO and Chaos sources drive global targets (Velocity/Envelope have no single global
    // value), which matches the editor's ring feed exactly, so ring == audio.
    // Which layers SOUND this block (21.6/21.7): live keys, a pattern aimed at them, or a release
    // tail still fading. One predicate for the render and the master-bus modulation (review).
    const auto playing = layersPlaying();
    const auto keys = keyScope(playing);   // 21.8: which keys reach which layer (and its lane)
    std::uint8_t activeLanes = 0;          // bit i = lane i's STEP SEQ or ARP replaces its keys
    for (auto& l : lanes)
    {
        l.snapshot();                      // one read of the switches for the whole block (review)
        if (l.blockActive()) activeLanes |= (std::uint8_t) (1u << l.index);
    }
    const bool layerAOn = playing.a || engineA.isSounding() || (activeLanes & 1u) != 0;
    bool previewB = false;   // 21.8: a step of B's STEP SEQ being previewed makes B sound
    for (const auto meta : midiMessages)
        if (const auto m = meta.getMessage(); m.isNoteOn() && m.getChannel() == kAuditionChannelB) { previewB = true; break; }
    // A figure of B's that can play (switched on AND latched) — a switch alone renders nothing (review).
    const bool bFigure  = lanes[1].blockSeq && lanes[1].latchedRoot.load() >= 0;
    const bool layerBOn = playing.b || engineB.isSounding() || bFigure || previewB;
    if (bFigure != laneBSoundingLast)      // B's shared-module claims follow its figure (message thread)
    {
        laneBSoundingLast = bFigure;
        matrixEnablesDirty.store(true);
    }

    std::array<double, ModMatrixConfig::kNumTargets> gMod {};
    {
        using namespace Parameters;
        static constexpr int kLfoSrc[kNumLFOs] = { (int) ModSource::LFO1, (int) ModSource::LFO2,
                                                   (int) ModSource::LFO3, (int) ModSource::LFO4 };
        // AD-16 D3: both layers' matrices drive the master bus (each while it sounds); the
        // offsets add. Layer A alone is the pre-AD-16 sum, in the same order.
        for (const PatchEngine* layer : { &engineA, &engineB })
        {
            if (layer == &engineA && ! layerAOn) continue;
            if (layer == &engineB && ! layerBOn) break;
            const auto& pp = layer->params();
            if (*pp.modMatrixOn > 0.5f)
                for (int n = 1; n <= ModMatrixConfig::kNumSlots; ++n)
                {
                    const int mod = (int) *pp.modSlotModule[(size_t) n - 1];
                    if (mod <= 0) continue;   // Off
                    const int   src = (int) *pp.modSlotSource[(size_t) n - 1];
                    const int   par = (int) *pp.modSlotParam [(size_t) n - 1];
                    const float amt = *pp.modSlotAmount[(size_t) n - 1];
                    float sv = 0.0f;
                    for (int i = 0; i < kNumLFOs; ++i) if (src == kLfoSrc[i]) sv = layer->getLfoDisplayValue(i);
                    if (src == (int) ModSource::ChaosX) sv = chaosDisplay[0].load();   // global attractor —
                    if (src == (int) ModSource::ChaosY) sv = chaosDisplay[1].load();   // same values the voices get
                    const int tgt = (int) ModDest::targetOf(mod, par);
                    if (tgt > 0 && tgt < (int) gMod.size()) gMod[(size_t) tgt] += (double) amt * sv;
                }
        }   // layers
    }
    // MASTER · TEMPO — wobble the Tempo-Sync BPM so synced LFOs/delay drift around the beat (applied
    // to the resolved tempo, ±40 BPM at full modulation). Standalone only: in a DAW the MASTER
    // module is off (AD-13, Story 18.1) — the host's tempo is the tempo, its mod target is dead.
    if (! isHostedByDaw())
        syncBpm = juce::jlimit(40.0, 250.0, syncBpm + gMod[(size_t) LFOTarget::MasterTempo] * 90.0);

    // The patch's block start (Story 21.4): LFO rates, delay time, SAMPLER loop clock, and the
    // parameter push into the voices — see PatchEngine::beginBlock.
    PatchEngine::BlockContext patchCtx;
    patchCtx.numSamples  = buffer.getNumSamples();
    patchCtx.sampleRate  = getSampleRate();
    patchCtx.syncBpm     = syncBpm;
    patchCtx.chaosX      = chaosDisplay[0].load();
    patchCtx.chaosY      = chaosDisplay[1].load();
    patchCtx.accentDepth = lanes[0].seqAccent->load();   // 15.2 — layer A's STEP SEQ ACCENT (B's: ctxB below)
    patchCtx.hosted      = hosted;
    patchCtx.hostPlaying = hostPlaying;
    patchCtx.hostPpq     = hostPpq;
    SamplePlayer::resetSeekBudget();   // 12.3: per-block cap on note-on stretch pre-rolls — ONE budget per block for both layers (review)
    engineA.beginBlock(patchCtx);
    // Layer B (AD-16): not called at all while it is silent — no push, no render, no LFO advance —
    // so the instrument with B silent is the pre-AD-16 one, bit for bit. (Resolved above, before
    // the master-bus modulation.) B renders while it takes notes, and on after that until its last
    // release tail has faded — switching SINGLE from B to A lets B's notes ring out, not cut.
    if (layerBOn)
    {
        PatchEngine::BlockContext ctxB = patchCtx;
        ctxB.accentDepth = lanes[1].seqAccent->load();
        engineB.beginBlock(ctxB);
    }

    // Arpeggiator: replace the raw held chord with an automatic note sequence.
    {
        using namespace Parameters;
        // STEP SEQ (15.1) and the ARP both REPLACE the held chord, so they cannot both run. The
        // sequencer wins: it is the more specific instrument. The editor also switches the other
        // module off when one is enabled, so the rack never shows a lit module doing nothing.
        // PERC transport, resolved BEFORE the STEP SEQ block: the quantised entry below reads
        // perc.enabled and perc.samplesToPatternStart(), and both used to be refreshed only in
        // the render section further down — so the first block after a preset load saw the
        // PREVIOUS patch's drum state and skipped the quantisation (bass permanently off the
        // beat). Kit, levels and rendering stay below; only the clock moves up here.
        {
            // STOP (header latch) counts as "not running" here on purpose: the clock stands, the
            // playhead goes out, and the STEP SEQ does not wait for a drum downbeat that will not
            // come. Rendering below still runs, so a grid click can sound its lane while held.
            const bool percOn = *apvts.getRawParameterValue(ID::percOn) > 0.5f && ! percHeld.load()
                                && hostRunning;   // 18.6: in a DAW the host's STOP stops the drums too
            if (! percOn && perc.enabled)
                perc.reset();      // switched off or held ⇒ the next start is a downbeat, not where it stopped
            // The drums (re)starting IS a downbeat (maintainer 2026-10-11: after STOP, or after
            // switching PERC on, the bass ran on out of step). With a figure RUNNING, both wait
            // for the figure's next step boundary: the running bass step finishes, then figure
            // (legato, step 0 takes the voice over like a TIE) and drums start on the same sample.
            // An immediate jump cut the running step short — the "rhythmischer Ruck". With no
            // figure running the drums start now, and a figure entered later quantises to them.
            if (percOn && ! perc.enabled)
            {
                // Reset on the RISING edge as well: PercSequencer advances its sample counter even
                // while disabled (the lanes keep rendering), so the falling-edge reset above had
                // drifted by the length of the hold — step 0 then fired up to one step late, and
                // the drums came in a fraction of a step behind the bass (maintainer 2026-10-11:
                // "um einen halben Takt (einen Step?) verschoben"; found with a sample simulation).
                perc.reset();
                // Standalone only: in a DAW both clocks follow the host's position (below), so
                // there is nothing to negotiate between them.
                // 21.8: with figures in both layers, the first running one sets the moment and
                // every running figure restarts on it, so they stay together with the drums.
                if (! hosted)
                    for (auto& ref : lanes)
                        if (ref.seq.enabled && ref.seq.playingStep() >= 0)
                        {
                            const int wait = ref.seq.samplesToNextStep();
                            perc.setStartDelay(wait);
                            for (auto& l : lanes)
                                if (l.seq.enabled && l.seq.playingStep() >= 0)
                                    l.seq.restartLegatoIn(wait);
                            break;
                        }
            }
            perc.enabled = percOn;
            perc.length  = (int) *apvts.getRawParameterValue(ID::percLength);
            const int pdiv = (int) *apvts.getRawParameterValue(ID::percSync);
            perc.stepSeconds = SyncDivision::isSynced(pdiv)
                                   ? SyncDivision::delaySeconds(syncBpm, pdiv)
                                   : 1.0 / juce::jmax(0.5, (double) *apvts.getRawParameterValue(ID::percRate));
            // 18.6: lock the drum clock to the host's bar position (a free RATE has no beat to lock to).
            if (hosted && hostPlaying && percOn && hostPpq.hasValue() && SyncDivision::isSynced(pdiv))
            {
                const double anchor = percAnchorPpq.load();   // PLAY's position = step 0 (see above)
                perc.syncToHost((*hostPpq - (anchor >= 0.0 ? anchor : 0.0)) / SyncDivision::beatsPerCycle(pdiv));
            }
        }

        // Story 21.8: each layer's STEP SEQ / ARP — one lane per layer, the same code for each.
        // Stage 1 (raw keys): every lane reads its root / held chord and anchors from the keys that
        // reach its layer. Stage 2: the keys a running lane takes leave the buffer (a key that also
        // reaches a layer whose lane is idle stays — the router keeps it from the busy one). Stage
        // 3: each lane writes its pattern on its own channel.
        for (auto& l : lanes)
            laneInput(l, midiMessages, keys, syncBpm, hosted, hostPlaying, hostPpq);
        if (activeLanes != 0)
        {
            auto& kept = arpKeptScratch; kept.clear();
            std::bitset<128> liveThisBlock;   // live note-ons let through in THIS block (review)
            for (const auto meta : midiMessages)
            {
                auto m = meta.getMessage();
                if (m.getChannel() == 1)
                {
                    // A key reaching only busy layers is the pattern's; one reaching an idle layer
                    // plays live there. A note-off follows its note-on — also when both land in this
                    // block (a tap, a short DAW clip note): the router learns of this block's
                    // note-ons only later, so they are remembered here.
                    const int nn = m.getNoteNumber() & 127;
                    if (m.isNoteOn())
                    {
                        std::uint8_t reach = 0;
                        for (const auto& l : lanes)
                            if (keys.reaches(l.index, nn)) reach |= (std::uint8_t) (1u << l.index);
                        if (reach != 0 && (reach & ~activeLanes) == 0) continue;
                        liveThisBlock.set((size_t) nn);
                    }
                    if (m.isNoteOff() && ! layerRouter.isRouted(1, nn) && ! liveThisBlock[(size_t) nn])
                        continue;
                }
                kept.addEvent(m, meta.samplePosition);
            }
            midiMessages.swapWith(kept);
        }
        for (auto& l : lanes)
            laneOutput(l, midiMessages, keys, buffer.getNumSamples(), syncBpm, hosted, hostPlaying, hostPpq, hostRunning);

#if JASS_HOSTSYNC_DIAG
        // Per-block record after a PLAY edge, an anchor move or a state change (Bitwig diagnosis,
        // 2026-10-19): layer A's figure and the drums, as before 21.8 (ported, review).
        if (hosted)
        {
            const auto& la = lanes[0];
            const double anchorNow = la.anchorPpq.load();
            const int stateNow = (perc.enabled ? 1 : 0) | (percKitPending.load() ? 2 : 0) | (hostPlaying ? 4 : 0) | (la.seq.enabled ? 8 : 0);
            if (diagPlayEdge || anchorNow != diagLastAnchor || stateNow != diagLastState) diagBlocksLeft = 24;
            diagLastAnchor = anchorNow; diagLastState = stateNow;
            if (diagBlocksLeft > 0)
            {
                --diagBlocksLeft;
                const int sdivD = (int) la.seqSync->load();
                auto& r = diag[(size_t) (diagWrite.load() % 256)];
                r.t = juce::Time::getMillisecondCounterHiRes();
                r.ppq = hostPpq.hasValue() ? *hostPpq : -999.0;
                r.stepsPos = hostPpq.hasValue() ? *hostPpq / juce::jmax (1e-9, SyncDivision::beatsPerCycle(sdivD)) : -999.0;
                r.anchor = anchorNow; r.percAnchor = percAnchorPpq.load();
                r.playing = hostPlaying ? 1 : 0; r.seqOn = la.seq.enabled ? 1 : 0; r.latch = la.latchedRoot.load();
                r.percOn = perc.enabled ? 1 : 0; r.kitPending = percKitPending.load() ? 1 : 0;
                r.kitIdx = (int) *apvts.getRawParameterValue(Parameters::ID::percKit); r.percStep = perc.playingStep();
                r.preIdx = diagSeqPre[0]; r.preCnt = diagSeqPre[1];
                r.postIdx = diagSeqPost[0]; r.postCnt = diagSeqPost[1]; r.postDelay = diagSeqPost[2];
                r.fired = la.seq.enabled ? la.seq.playingStep() : -1; r.n = buffer.getNumSamples();
                diagWrite.fetch_add (1);
            }
        }
#endif
    }

    // Don't let the auto-play drone pluck the Karplus string (it's played via
    // the keyboard). Suppress the pluck only for the drone's own note-on.
    if (droneJustTriggered)
    {
        engineA.setPluckEnabled(false);
        engineB.setPluckEnabled(false);
    }

    // Manual PLUCK (button / spacebar): re-excite the Karplus string on every voice.
    // RT-safe — the atomic flag is set on the message thread and consumed here.
    if (pluckRequested.exchange(false))
        focusedEngine().pluck();   // AD-16: the layer being edited

    // A host's All-Notes-Off / All-Sound-Off (any channel but the pattern's own) also releases the
    // ARP / STEP SEQ voices: before 21.7 they played channel 1 and a channel-1 panic reached them;
    // on the pattern channel it would not (review 2026-11-18). Block-granular, alloc-free.
    for (const auto meta : midiMessages)
        if (const auto m = meta.getMessage(); (m.isAllNotesOff() || m.isAllSoundOff())
                                              && m.getChannel() != kPatternChannel && m.getChannel() != kPatternChannelB)
        {
            for (const auto& l : lanes)
            {
                engineA.releaseChannel(l.channel);
                engineB.releaseChannel(l.channel);
            }
            break;
        }

    // Poly-glide (portamento) and mono glide: on the FINAL midi buffer (after the arpeggiator) so
    // it glides arp steps too — see PatchEngine::processGlide.
    // AD-16: B off — layer A gets the buffer untouched (the router only keeps its note memory).
    // B on — the router splits the buffer into one per layer (LAYER: both, SPLIT: below → B).
    if (! layerBOn)
    {
        layerRouter.track(midiMessages);
        engineA.processGlide(midiMessages, kDroneChannel);
        engineA.render(buffer, midiMessages);
        applyLayerGain(0, buffer, buffer.getNumSamples());
    }
    else
    {
        LayerRouter::Settings rs;
        rs.playA           = playing.a;
        rs.playB           = playing.b;
        rs.split           = juce::roundToInt (apvts.getRawParameterValue(Parameters::ID::layerMode)->load()) == 2;
        rs.splitNote       = (int) *apvts.getRawParameterValue(Parameters::ID::splitNote);
        rs.focusB          = isFocusB();   // A+B edits through A: drone and preview stay on A
        rs.patternChannelA = kPatternChannel;    // 21.8: each lane's channel goes to its layer
        rs.patternChannelB = kPatternChannelB;
        rs.liveBlock       = activeLanes;        // ...and a busy lane's layer takes no live keys
        rs.droneChannel    = kDroneChannel;
        rs.auditionChannel = kAuditionChannel;
        rs.auditionChannelB = kAuditionChannelB;   // 21.8: B's STEP SEQ preview
        layerRouter.route(midiMessages, layerMidiA, layerMidiB, rs);

        engineA.processGlide(layerMidiA, kDroneChannel);
        engineA.render(buffer, layerMidiA);
        applyLayerGain(0, buffer, buffer.getNumSamples());

        const int n = buffer.getNumSamples();
        layerBRender.setSize(buffer.getNumChannels(), n, false, false, true);   // no realloc within capacity
        layerBRender.clear();
        engineB.processGlide(layerMidiB, kDroneChannel);
        engineB.render(layerBRender, layerMidiB);
        applyLayerGain(1, layerBRender, n);
        for (int c = 0; c < buffer.getNumChannels(); ++c)
            buffer.addFrom(c, 0, layerBRender, c, 0, n);
    }

    // ---- PERC (Story 16.1): layer B, straight into the summed mix -----------------------------
    // Here and not inside a voice, which is the whole point: a voice would drag the kit through
    // the patch's filter and effect chain. After the synth so it mixes with it, before the
    // compressor so the master glue and the MASTER level still apply — and nothing else does.
    {
        using namespace Parameters;
        // Transport (enabled + falling-edge reset + length + stepSeconds) is resolved ABOVE the
        // STEP SEQ block, where the quantised entry needs it fresh — see the comment there.
        const bool percOn = perc.enabled;

        // Its OWN kit, independent of the SAMPLER's set — a sampled instrument and a drum kit have
        // to coexist, which is the case this module exists for. While a preset's kit is still being
        // fetched, hand in NOTHING rather than the set the stale index points at.
        // KIT is the store index PLUS ONE; 0 means no kit at all, and getSet(-1) yields nullptr.
        const int kitIdx = (int) *apvts.getRawParameterValue(ID::percKit) - 1;
        perc.setSource(percKitPending.load() ? nullptr
                                             : SampleBankStore::instance().getSet(kitIdx));
        perc.amp    = *apvts.getRawParameterValue(ID::percAmp);
        for (int l = 0; l < PercSequencer::kLanes; ++l)
        {
            perc.note [(size_t) l] = (int) *apvts.getRawParameterValue(ID::percNote (l + 1));
            perc.level[(size_t) l] =       *apvts.getRawParameterValue(ID::percLevel(l + 1));
            perc.pan  [(size_t) l] =       *apvts.getRawParameterValue(ID::percPan  (l + 1));
            // Same bound playback uses (see PercSequencer::processBlock) — cells past LENGTH are
            // never read, so they are not copied either (16 pages would be ~3000 loads per block).
            if (percOn)
                for (int s = 0, n = juce::jlimit(1, (int) PercSequencer::kMaxSteps, perc.length); s < n; ++s)
                    perc.on[(size_t) l][(size_t) s] = patterns.perc.get(l, s);   // AD-14: one relaxed load per cell
        }
        // A step just placed in the grid sounds once, whether or not the pattern is running.
        if (const int lane = percAuditionLane.exchange(-1); lane >= 0)
            perc.triggerLane(lane);
        // Rendered even when the module is off: the preview above, and the tail of a hit still
        // ringing when it was switched off, both have to come out.
        perc.processBlock(buffer, buffer.getNumSamples());
        percStepDisplay.store(percOn ? perc.playingStep() : -1);
    }

    // Mirror the note the STEP SEQ is holding, so the on-screen keyboard can show what the pattern
    // is playing (maintainer 2026-08-11). Display only: the sequencer's notes go straight into the
    // MIDI buffer and must stay out of the keyboard STATE, or they would come back as held keys and
    // the pattern would keep re-rooting itself.
    for (auto& l : lanes)
    {
        l.noteDisplay.store(l.seq.enabled ? l.seq.currentNote() : -1);
        // …and the step it is on, so STEP SEQ can mark the running step the way PERC's grid does.
        l.stepDisplay.store(l.seq.enabled ? l.seq.playingStep() : -1);
    }

    if (droneJustTriggered)
    {
        engineA.setPluckEnabled(true);
        engineB.setPluckEnabled(true);
    }

    // Advance the display / GLOBAL LFOs (20.1) — after the render, as before; see
    // PatchEngine::endBlock.
    engineA.endBlock(patchCtx);
    if (layerBOn)
        engineB.endBlock(patchCtx);

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

    // Kunstkopf externalization (Story 10.4): shared binaural early-reflection stage, bus-level and
    // ONLY in Kunstkopf mode — the other four modes are byte-identical to before (regression oracle).
    // Runs once on the stereo mix (not per voice): reflections need no per-source accuracy to move
    // the image out of the head, so the shared pattern gets the effect at a fixed, tiny cost.
    {
        const bool kunstkopf = (outMode == (int) OutputMode::Kunstkopf) && buffer.getNumChannels() >= 2;
        if (kunstkopf && !wasKunstkopf)
            binauralRoom.reset();   // entering the mode: clear stale ring content
        wasKunstkopf = kunstkopf;
        if (kunstkopf)
            binauralRoom.process(buffer.getWritePointer(0), buffer.getWritePointer(1),
                                 buffer.getNumSamples(),
                                 *apvts.getRawParameterValue(Parameters::ID::hrtfRoom));
    }

    // Master volume — gated by masterOn (Story 2.4): off => silent output. Base + MOD MATRIX offset,
    // applied as a per-block RAMP (prev→cur) so LFO-modulated VOL doesn't zipper.
    // In a DAW the MASTER module is off (AD-13, Story 18.1): mute ignored, VOL mod dead — the
    // track fader is the one level and the host's mute is the mute. The stored params stay
    // untouched so the same patch keeps its VOL/mute in the standalone. The host gain is a FIXED
    // pad equal to VOL's default (0.5, -6 dB), not unity: every preset so far sits at that VOL,
    // so unity put each one 6 dB hotter in a DAW than in the standalone, and the Init stack
    // (three oscillators at 0.5) peaked at +7 dBFS (headless bounce, maintainer 2026-11-04).
    // With the pad a patch lands in the DAW at the level it has in the standalone.
    const bool  masterOn   = *apvts.getRawParameterValue(Parameters::ID::masterOn) > 0.5f;
    const float masterVol  = juce::jlimit(0.0f, 1.0f, (float) (apvts.getRawParameterValue(Parameters::ID::masterVol)->load() + gMod[(size_t) LFOTarget::MasterVol] * 1.0));
    const float masterGain = isHostedByDaw() ? kHostPad : (masterOn ? masterVol : 0.0f);
    buffer.applyGainRamp(0, buffer.getNumSamples(), prevMasterGain, masterGain);
    prevMasterGain = masterGain;

    // Capture the STEREO output for OSCILLOSCOPE + SPECTRUM (Story 10.6) — the VERY LAST stage:
    // the displays show exactly what leaves the plugin ("was hinten rauskommt"), including the
    // whole master section (compressor, widener, Binaural/Kunstkopf, ROOM) and master volume.
    // (Supersedes the Story-2.3 pre-bus mono tap, deliberately: the engine output is genuinely
    // stereo since Epic 10 and the user wants to SEE the spatial stages.)
    {
        const float* capL = buffer.getReadPointer(0);
        const float* capR = buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : capL;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            waveformCapture.writeSample(capL[i], capR[i]);
    }
}

// ── Story 21.8: one layer's STEP SEQ / ARP (a PatternLane) ─────────────────────────────────────
// The code below is the pre-21.8 sequencer block, per lane: every id is the lane's own (prefixed
// for B), its keys are the keys that reach its layer, its notes go out on its own channel, and a
// TIE/SLIDE retunes a voice of its own layer. Nothing here names a particular lane.
void SynthyProcessor::laneInput (PatternLane& lane, const juce::MidiBuffer& rawMidi, const PatternLane::KeyScope& keys,
                                 double syncBpm, bool hosted, bool hostPlaying, const juce::Optional<double>& hostPpq)
{
    if (! lane.blockSeq)
        return;
    auto& seq = lane.seq;
    auto feeds = [&] (int n) { return keys.reaches (lane.index, n); };

    seq.enabled = true;
    seq.length  = (int) lane.seqLength->load();
    // Step length: the musical division against the resolved tempo (host BPM when hosted, else the
    // MASTER knob), or the free-running RATE in steps/s. Same clock the LFOs and DELAY ride on, so a
    // synced pattern cannot drift against a synced delay.
    const int sdiv = (int) lane.seqSync->load();
    seq.stepSeconds = SyncDivision::isSynced(sdiv)
                          ? SyncDivision::delaySeconds(syncBpm, sdiv)
                          : 1.0 / juce::jmax(0.5, (double) lane.seqRate->load());
    seq.gate = lane.seqGate->load();
    lane.stepSecondsDisplay.store(seq.stepSeconds);   // the GATE preview's time base (editor)
    // Only the playable range is copied: playback clamps to `length` with the same bound, so steps
    // beyond it are never read — and at kMaxSteps 768 (16 pages) copying the full arrays would be
    // ~3000 atomic loads per block for a 16-step figure.
    const auto& fig = patterns.steps[(size_t) lane.index];
    const int seqUsed = juce::jlimit(1, (int) StepSequencer::kMaxSteps, seq.length);
    for (int s = 0; s < seqUsed; ++s)   // AD-14: from the PatternStore, one relaxed load per cell
    {
        seq.pitch [(size_t) s] = fig.pitch (s);
        seq.on    [(size_t) s] = fig.on    (s);
        seq.accent[(size_t) s] = fig.accent(s);   // 15.2
        seq.sgate [(size_t) s] = fig.gate  (s);   // 15.7
    }

    // The LOWEST held channel-1 note that reaches this layer is the root the step offsets are added
    // to; the ch.16 auto-play drone is excluded, exactly as for the arp. While the editor is
    // RECORDING this lane's figure (15.4) the search is skipped outright: those keys are being
    // written into the steps, and taking one as the root would restart and transpose the pattern on
    // every note entered.
    int root = -1;
    if (! lane.recordArmed.load())
    {
        for (int n = 0; n < 128 && root < 0; ++n)
            if (keyboardState.isNoteOn(1, n) && feeds(n)) root = n;
        // A note whose note-off lands in the SAME block is already up by now and the search above
        // never sees it — a short clip note in a DAW, or a tapped key. It still latches: take the
        // lowest channel-1 note-on of this block (maintainer 2026-10-19, Bitwig: the figure started
        // "nur manchmal" — a block-alignment lottery).
        if (root < 0)
            for (const auto meta : rawMidi)
                if (const auto m = meta.getMessage(); m.isNoteOn() && m.getChannel() == 1 && feeds(m.getNoteNumber()))
                    if (root < 0 || m.getNoteNumber() < root)
                        root = m.getNoteNumber();
    }
    lane.blockRoot = root;

    // 18.6 ANCHOR: in a host the figure's step 0 is where its note ENTERED, not the song's start.
    // The host position is taken modulo LEN (syncToHost), and a 48-step figure is three bars:
    // counted from bar 1 it sits at step 0 only every third bar, so a clip launched or a PLAY
    // pressed anywhere else began the figure in its middle (maintainer 2026-10-25, Bitwig: "der
    // erste gespielte Ton ist der letzte der Sequenz"). The edge is taken event by event, not from
    // the keys held at the block's end: a clip boundary in a DAW is a note-off and a note-on at the
    // SAME sample, and that is a fresh entry - only a key added while another is really still down
    // changes the root without restarting, exactly the standalone's rule. The note's exact position,
    // not a grid: the user placed it. A key pressed while the host stands anchors nothing.
    {
        const bool anchorArmed = hosted && hostPlaying && hostPpq.hasValue()
                              && SyncDivision::isSynced(sdiv) && ! lane.recordArmed.load();
        for (const auto meta : rawMidi)
        {
            const auto m = meta.getMessage();
            if (m.getChannel() != 1) continue;
            // A release always clears its key, whichever layer it reaches NOW: the split or the
            // selection may have moved since its note-on, and a bit left set would keep the
            // "from silence" edge from ever arming the anchor again (review).
            if (m.isNoteOff() || (m.isNoteOn() && m.getVelocity() == 0))
            {
                lane.hostKeysDown.reset ((size_t) m.getNoteNumber());
                continue;
            }
            if (m.isNoteOn() && ! feeds(m.getNoteNumber()))
                continue;   // a key of another layer does not anchor this figure
            if (m.isNoteOn())
            {
                // A note-off at this very sample that the buffer orders AFTER the note-on still
                // belongs before it (a clip boundary): apply those first.
                for (const auto later : rawMidi)
                    if (later.samplePosition == meta.samplePosition)
                        if (const auto l = later.getMessage(); l.isNoteOff() && l.getChannel() == 1)
                            lane.hostKeysDown.reset ((size_t) l.getNoteNumber());
                if (lane.hostKeysDown.none() && anchorArmed)
                    lane.anchorPpq.store (*hostPpq + meta.samplePosition * syncBpm / (60.0 * juce::jmax (1.0, getSampleRate())));
                lane.hostKeysDown.set ((size_t) m.getNoteNumber());
            }
            else if (m.isAllNotesOff() || m.isAllSoundOff())
                lane.hostKeysDown.reset();
        }
    }

    // LATCH (maintainer 2026-08-11): the first key takes the pattern over and it keeps running after
    // the key is released — holding one down for a whole piece is not playing, it is standing
    // still. What changes the root afterwards is a NEW key, or an octave shift, which the editor
    // applies to this latched value (there is no held key left for it to move). Switching the
    // module off clears the latch, so the transport is still the module's own switch.
    if (root >= 0)
        lane.latchedRoot.store(root);
    const int playRoot = lane.latchedRoot.load();
    if (playRoot >= 0)
        seq.setRoot(playRoot);

    // Preset-load latch (15.5): a stored latch must ALWAYS enter on the drums' downbeat. The
    // rising-edge logic below cannot see this case — a latch loaded over a still-running one keeps
    // playRoot high, so there is no edge. applySeqLatchRoot sets the flag; the restart itself
    // happens in laneOutput, AFTER the chord filter, because its note-off must land in the buffer
    // that is kept: everything channel-1 in the RAW buffer may be dropped there, and a swallowed
    // note-off is a bass note that hangs forever (maintainer found it in minutes: "irgendwas wird
    // nicht richtig abgeschaltet").
    lane.blockRequantize = lane.requantize.exchange(false) && playRoot >= 0;
    if (lane.blockRequantize)
        lane.seqKeyWasHeld = true;   // edge consumed — the check below must not fire again

    // Quantised entry (16.1): a figure started from silence while PERC runs waits for the drum
    // pattern's next step 0. Only on the rising edge — a key ADDED to an already running figure
    // must not restart or delay anything, exactly as in 15.1. 21.8: without PERC, a figure entering
    // while another layer's figure runs waits for that one's next step, so the two share the grid.
    if (! hosted && playRoot >= 0 && ! lane.seqKeyWasHeld)   // 18.6: a DAW's grid rules
    {
        if (perc.enabled)
            seq.setStartDelay(perc.samplesToPatternStart());
        else
            for (const auto& other : lanes)
                if (&other != &lane && other.seq.enabled && other.seq.playingStep() >= 0)
                {
                    seq.setStartDelay(other.seq.samplesToNextStep());
                    break;
                }
    }
    lane.seqKeyWasHeld = (playRoot >= 0);
}

void SynthyProcessor::laneOutput (PatternLane& lane, juce::MidiBuffer& midi, const PatternLane::KeyScope& keys, int numSamples,
                                  double syncBpm, bool hosted, bool hostPlaying, const juce::Optional<double>& hostPpq, bool hostRunning)
{
    PatchEngine& engine = lane.index == 0 ? engineA : engineB;
    auto& seq = lane.seq;
    if (lane.blockSeq)
    {
        const int playRoot = lane.latchedRoot.load();
        const int sdiv = (int) lane.seqSync->load();
        if (lane.blockRequantize)
        {
            // Restart at step 0, quantised to the drum clock — LEGATO: the note that is sounding is
            // kept and step 0 takes it over like a TIE boundary, so the bass does not dip while it
            // falls back in step with the drums (maintainer 2026-10-11). A preset load (the other
            // caller) has no voice sounding, so the takeover finds nothing and the entry is a plain
            // note-on, as before.
            seq.restartLegato();
            if (! hosted && perc.enabled)
                seq.setStartDelay(perc.samplesToPatternStart());
            else if (! hosted)   // no drums: on the other running figure's grid, as laneInput's entry (review)
                for (const auto& other : lanes)
                    if (&other != &lane && other.seq.enabled && other.seq.playingStep() >= 0)
                    {
                        seq.setStartDelay(other.seq.samplesToNextStep());
                        break;
                    }
        }
        // 18.6: in a DAW the figure's step is the host's bar position, whatever key started it and
        // whenever - so it lines up with a PERC on another track, and with the DAW's grid. Only
        // while the host PLAYS: a stopped host's position would pull a free-running clock back to
        // the same step every block. While it stands, the figure runs only under a key that is
        // actually held (sound design with the transport off), on the free clock.
#if JASS_HOSTSYNC_DIAG
        const int diagPreIdx = seq.currentStep(), diagPreCnt = seq.clockSampleCounter();
#endif
        if (hosted && hostPlaying && hostPpq.hasValue() && playRoot >= 0 && SyncDivision::isSynced(sdiv))
        {
            const double anchor = lane.anchorPpq.load();   // step 0 = the note's entry
            seq.syncToHost((*hostPpq - (anchor >= 0.0 ? anchor : 0.0)) / SyncDivision::beatsPerCycle(sdiv));
        }
#if JASS_HOSTSYNC_DIAG
        if (lane.index == 0)   // the diagnostic records follow layer A's figure, as before 21.8
        {
            diagSeqPre[0] = diagPreIdx; diagSeqPre[1] = diagPreCnt;
            diagSeqPost[0] = seq.currentStep(); diagSeqPost[1] = seq.clockSampleCounter(); diagSeqPost[2] = seq.clockStartDelay();
        }
#endif
        const bool seqRuns = hostRunning ? (playRoot >= 0) : (lane.blockRoot >= 0 && playRoot >= 0);
        seq.processBlock(numSamples, midi, lane.channel, seqRuns);   // the lane's own channel

        // 15.7 TIE/SLIDE: a takeover boundary retunes the sounding voice instead of retriggering
        // it — MIDI cannot say that, so the sequencer recorded it and the voice of this lane's
        // layer is moved here (SynthVoice::slideTo: envelope untouched). Block-granular start
        // (≤ one buffer early against a ≥ tens-of-ms step — inaudible). The chain's note-on is at
        // least one full step old, so its voice always exists by now.
        for (int e = 0; e < seq.numLegatoEvents(); ++e)
        {
            const auto& ev = seq.legatoEvent(e);
            engine.slideVoice(ev.note, (double) ev.semitones, ev.slide ? StepSequencer::kSlideSeconds : 0.0);
        }
    }
    else if (seq.enabled)
    {
        seq.enabled = false;       // just switched off => release its note
        seq.releaseAll(midi, lane.channel);
        seq.reset();
        lane.latchedRoot.store(-1);      // the switch is the transport: off forgets the root
        lane.seqKeyWasHeld = false;
        lane.requantize.store(false);    // a stray flag must not fire on a later re-enable
    }

    auto& arp = lane.arp;
    if (lane.blockArp)
    {
        arp.enabled = true;
        // 21.9: SYNC = the step as a note value against the resolved tempo (host BPM when hosted,
        // else MASTER), the same clock as STEP SEQ and PERC; Free = RATE in steps/s.
        const int adiv = (int) lane.arpSync->load();
        arp.rateHz  = SyncDivision::isSynced(adiv) ? 1.0 / juce::jmax(1.0e-3, SyncDivision::delaySeconds(syncBpm, adiv))
                                                   : (double) lane.arpRate->load();
        arp.mode    = (Arpeggiator::Mode)(int) lane.arpMode->load();
        arp.octaves = (int) lane.arpOctaves->load();
        arp.gate    = lane.arpGate->load();
        // Held chord = the channel-1 notes currently down that reach this layer (keyboardState was
        // just updated). The drone lives on channel 16, so it's excluded.
        lane.arpHeld.clear();
        for (int n = 0; n < 128; ++n)
            if (keyboardState.isNoteOn(1, n) && keys.reaches(lane.index, n)) lane.arpHeld.push_back(n);
        arp.setHeldNotes(lane.arpHeld);
        arp.processBlock(numSamples, midi, lane.channel);
    }
    else if (arp.enabled)
    {
        arp.enabled = false;           // just switched off → release its note
        arp.releaseAll(midi, lane.channel);
        arp.reset();
    }
}

void SynthyProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    patterns.perc.writeXml(*xml);   // AD-14: the patterns ride along as ONE element each, not thousands of PARAMs
    for (size_t i = 0; i < patterns.steps.size(); ++i)   // 21.8: one figure per layer; A keeps the old tag
        patterns.steps[i].writeXml(*xml, StepPattern::xmlTagFor((int) i));
    // The sample-set NAMES beside the indices, for the same reason a preset carries them
    // (PresetIO::toVar): a DAW project must find its drum kit again whatever the store's order
    // is when the project opens (maintainer 2026-10-19, Bitwig: PERC came in late / not at all).
    xml->setAttribute("SamplerSetFile", PresetIO::currentSetName(apvts, Parameters::ID::samplerSet));
    xml->setAttribute("PercKitFile",    PresetIO::currentSetName(apvts, Parameters::ID::percKit));
    // AD-16: layer B's SAMPLER set, by name for the same reason. Written only when it names one.
    {
        const juce::String bSet (juce::String (Modules::kLayerBPrefix) + Parameters::ID::samplerSet);
        if (const auto n = PresetIO::currentSetName(apvts, bSet.toRawUTF8()); n.isNotEmpty())
            xml->setAttribute("BSamplerSetFile", n);
    }
    // The figure's latched root (15.5 for presets): a sequencer track comes back running on the
    // note it was running on, exactly as a preset does - without it a reopened project waited for
    // a note-on that Bitwig does not send for a note the playhead starts inside of.
    // One per layer since 21.8: "SeqLatchRoot" (A, the old name) and "SeqLatchRootB".
    for (const auto& l : lanes)
        xml->setAttribute(l.index == 0 ? juce::String("SeqLatchRoot") : "SeqLatchRoot" + juce::String::charToString((juce::juce_wchar) ('A' + l.index)),
                          l.latchedRoot.load());
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

    // 18.4→18.5 DAW-state migration (AD-14): a host project saved while the PERC grid cells were
    // parameters carries them as <PARAM id="percStep<lane>_<step>" value="0|1">. Fold every one
    // into the PatternStore and drop the element, so the tree handed to replaceState holds
    // parameters that still exist. Gated on their presence — a state saved by this build has
    // none (it carries a <PercPattern> element instead, read after this) — and idempotent.
    // Returns true when anything was folded, so the caller knows the grid is already set.
    // (The STEP SEQ twin below does the same for seqPitch<n> / seqStep<n> / seqAcc<n> / seqSGate<n>.)
    bool migrateLegacyPercXml (juce::XmlElement& state, PercPattern& grid)
    {
        bool found = false;
        for (auto* el = state.getFirstChildElement(); el != nullptr;)
        {
            auto* next = el->getNextElement();
            if (el->hasTagName ("PARAM"))
            {
                const juce::String id = el->getStringAttribute ("id");
                if (id.startsWith (Parameters::ID::percStepLegacyPrefix))
                {
                    const int us = id.indexOfChar ('_');
                    if (us > 0)
                    {
                        if (! found) { grid.clear(); found = true; }   // the old state is complete
                        const int lane = id.substring ((int) std::strlen (Parameters::ID::percStepLegacyPrefix), us).getIntValue();
                        const int step = id.substring (us + 1).getIntValue();
                        grid.set (lane - 1, step - 1, el->getDoubleAttribute ("value") > 0.5);
                    }
                    state.removeChildElement (el, true);
                }
            }
            el = next;
        }
        return found;
    }

    bool migrateLegacyStepXml (juce::XmlElement& state, StepPattern& fig)
    {
        using namespace Parameters;
        bool found = false;
        auto numberAfter = [] (const juce::String& id, const char* prefix) -> int
        {
            if (! id.startsWith (prefix)) return -1;
            const juce::String tail = id.substring ((int) std::strlen (prefix));
            return tail.isNotEmpty() && tail.containsOnly ("0123456789") ? tail.getIntValue() : -1;
        };
        for (auto* el = state.getFirstChildElement(); el != nullptr;)
        {
            auto* next = el->getNextElement();
            if (el->hasTagName ("PARAM"))
            {
                const juce::String id = el->getStringAttribute ("id");
                const double value = el->getDoubleAttribute ("value");
                int n = -1;
                enum { None, Pitch, On, Acc, Gate } kind = None;
                if      ((n = numberAfter (id, ID::seqSGateLegacyPrefix)) > 0) kind = Gate;    // before seqStep: "seqS…" vs "seqStep"
                else if ((n = numberAfter (id, ID::seqPitchLegacyPrefix)) > 0) kind = Pitch;
                else if ((n = numberAfter (id, ID::seqStepLegacyPrefix))  > 0) kind = On;
                else if ((n = numberAfter (id, ID::seqAccLegacyPrefix))   > 0) kind = Acc;
                if (kind != None)
                {
                    if (! found) { fig.clear(); found = true; }   // the old state is complete
                    switch (kind)
                    {
                        case Pitch: fig.setPitch  (n - 1, (int) value);  break;
                        case On:    fig.setOn     (n - 1, value > 0.5);  break;
                        case Acc:   fig.setAccent (n - 1, value > 0.5);  break;
                        case Gate:  fig.setGate   (n - 1, (int) value);  break;
                        default: break;
                    }
                    state.removeChildElement (el, true);
                }
            }
            el = next;
        }
        return found;
    }
}

void SynthyProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
    {
        migrateLegacyMatrixXml(*xml);   // v4→v5: SlotNTarget → SlotNModule + SlotNParam
        // AD-14: the patterns. A pre-18.5 project carries them as parameters (folded in here), a
        // current one as <PercPattern> / <StepPattern> elements; a state with neither is an empty
        // pattern — a DAW state is a complete snapshot, exactly like a preset.
        const bool foldedPerc = migrateLegacyPercXml(*xml, patterns.perc);
        if (! patterns.perc.readXml(*xml) && ! foldedPerc)
            patterns.perc.clear();
        const bool foldedStep = migrateLegacyStepXml(*xml, patterns.steps[0]);
        if (! patterns.steps[0].readXml(*xml) && ! foldedStep)
            patterns.steps[0].clear();
        for (size_t i = 1; i < patterns.steps.size(); ++i)   // 21.8: B's figure (absent before = empty)
            if (! patterns.steps[i].readXml(*xml, StepPattern::xmlTagFor((int) i)))
                patterns.steps[i].clear();
        // The set names (written by getStateInformation since 2026-10-19; a project saved before
        // carries none and keeps the index behaviour). Taken off the tree before it becomes the
        // state, then re-resolved AFTER replaceState so the selection lands on top of the indices:
        // the store fills in the background at start-up, so the restored index alone pointed at
        // nothing — or at another kit — until the list was complete.
        const juce::String samplerName = xml->getStringAttribute("SamplerSetFile");
        const juce::String kitName     = xml->getStringAttribute("PercKitFile");
        const juce::String bSamplerName = xml->getStringAttribute("BSamplerSetFile");   // AD-16
        int laneLatch[kNumLanes];   // absent (older project) = -1 = none; one per layer since 21.8
        for (int i = 0; i < kNumLanes; ++i)
        {
            const juce::String attr = i == 0 ? juce::String("SeqLatchRoot") : "SeqLatchRoot" + juce::String::charToString((juce::juce_wchar) ('A' + i));
            laneLatch[i] = xml->getIntAttribute(attr, -1);
            xml->removeAttribute(attr);
        }
        xml->removeAttribute("SamplerSetFile");
        xml->removeAttribute("PercKitFile");
        xml->removeAttribute("BSamplerSetFile");
        for (int i = 0; i < kNumLanes; ++i)
            lanes[(size_t) i].latchedRoot.store(laneLatch[i] >= 0 ? juce::jlimit(0, 127, laneLatch[i]) : -1);
#if JASS_HOSTSYNC_DIAG
        diagNote ("setStateInformation");
#endif
        {
            // JUCE restores parameters in id order (alphabetical), so B's "b..." ids land before most
            // of A's - mirroring A's restored values onto B would shift B off its saved state (review).
            const juce::ScopedValueSetter<bool> noLink (linkSuspended, true);
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
        }
        requestGlobalLfoRearm();        // 20.1: a restored project starts its global One-Shots afresh —
                                        // AFTER the replace, or a block in between spends it on the old state
        PresetIO::resolveSetByName(apvts, samplerName, Parameters::ID::samplerSet, PresetIO::hooks(apvts).pendingSamplerSetName, 0);
        PresetIO::resolveSetByName(apvts, kitName,     Parameters::ID::percKit,    PresetIO::hooks(apvts).pendingPercKitName,    1);
        {
            const juce::String bSet (juce::String (Modules::kLayerBPrefix) + Parameters::ID::samplerSet);
            PresetIO::resolveSetByName(apvts, bSamplerName, bSet.toRawUTF8(), PresetIO::hooks(apvts).pendingBSamplerSetName, 0);
        }
        // An editor that already exists was built against the state this one replaces — tell it
        // to apply the restored rack layout (see onStateReloaded, case 2). Posted, because a host
        // may hand the state over off the message thread.
        juce::WeakReference<SynthyProcessor> weak(this);
        juce::MessageManager::callAsync([weak]
        {
            if (auto* p = weak.get(); p != nullptr && p->onStateReloaded)
                p->onStateReloaded(false);
        });
        // The just-restored state IS the clean baseline: snapshot it so a freshly loaded host
        // project does not spuriously report "modified" (isPresetModified compares to this).
        markPresetClean();
    }
}

juce::AudioProcessorEditor* SynthyProcessor::createEditor()
{
#if JASS_HOSTSYNC_DIAG
    diagNote ("createEditor");
#endif
    return new SynthyEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SynthyProcessor();
}

// AD-16 (Story 21.5): the layer the rack edits — the drone, PLUCK and the rings follow it.
PatchEngine& SynthyProcessor::focusedEngine() noexcept
{
    return isFocusB() ? engineB : engineA;   // A+B (linked editing) shows and plays A
}

// Level × balance of one layer (AD-16 D2). Balance, not pan: a layer is already stereo, so the
// centre leaves both channels at unity and a side only turns the OTHER channel down. Ramped from
// the previous block. Layer A at its defaults (level 1, centre) is skipped entirely — the
// pre-AD-16 signal, bit for bit.
void SynthyProcessor::applyLayerGain (int layer, juce::AudioBuffer<float>& b, int numSamples)
{
    using namespace Parameters;
    const float level = *apvts.getRawParameterValue(layer == 0 ? ID::layerALevel : ID::layerBLevel);
    const float pan   = *apvts.getRawParameterValue(layer == 0 ? ID::layerAPan   : ID::layerBPan);
    const float g[2]  = { level * (pan > 0.0f ? 1.0f - pan : 1.0f),     // left: down when panned right
                          level * (pan < 0.0f ? 1.0f + pan : 1.0f) };   // right: down when panned left
    auto& prev = layerGainPrev[layer];
    if (g[0] == 1.0f && g[1] == 1.0f && prev[0] == 1.0f && prev[1] == 1.0f)
        return;
    for (int c = 0; c < b.getNumChannels(); ++c)
    {
        const int k = juce::jmin(c, 1);
        b.applyGainRamp(c, 0, numSamples, prev[k], g[k]);
    }
    prev[0] = g[0];
    prev[1] = g[1];
}

// Every parameter the processor listens to (constructor adds, destructor removes the same list):
// layer A's patch parameters (linked editing, and A's couplings among them), layer B's coupling
// params (MIX SRC / CROSS MOD / OSC on / matrix SRC + MOD), and what decides which layers sound
// (MODE, selection, the ARP / STEP SEQ TARGET).
juce::StringArray SynthyProcessor::listenedParamIds() const
{
    using namespace Parameters;
    juce::StringArray ids (Modules::layeredParamIds());
    const juce::String bp (Modules::kLayerBPrefix);
    for (const juce::String id : { juce::String(ID::mixSrcA), juce::String(ID::mixSrcB), juce::String(ID::mixModeOn),
                                   ID::oscOn(1), ID::oscOn(2), ID::oscOn(3) })
        ids.add(bp + id);
    for (int n = 1; n <= ModMatrixConfig::kNumSlots; ++n)
    {
        ids.add(bp + ID::modSlotSource(n));
        ids.add(bp + ID::modSlotModule(n));
    }
    ids.add(ID::layerMode);    // which layers play (Split: both) …
    ids.add(ID::layerFocus);   // … and outside a Split the selection
    ids.add(bp + ID::seqOn);   // 21.8: B's own STEP SEQ / ARP — exclusion, and B sounds (matrix claims)
    ids.add(bp + ID::arpOn);
    return ids;
}

// Linked editing (21.6): A's parameter `idA` moved by `deltaRaw` (in its own units); B follows.
// Knobs move by the same distance on the knob's travel, clamped — B keeps its offset; switches,
// choices and content selectors take A's value. Message thread only (parameterChanged checks).
void SynthyProcessor::mirrorToLayerB (const juce::String& idA, float deltaRaw)
{
    auto* a = apvts.getParameter(idA);
    auto* b = apvts.getParameter(juce::String(Modules::kLayerBPrefix) + idA);
    if (a == nullptr || b == nullptr)
        return;
    // A content selector (which sample set, which wavetable bank) is a choice in a number's clothes:
    // B takes the same pick, not "two sets further" (review 2026-11-18).
    const bool selector = idA == Parameters::ID::samplerSet || idA == Parameters::ID::wavetableBank;
    const bool continuous = ! selector
                         && (dynamic_cast<juce::AudioParameterFloat*>(a) != nullptr
                             || dynamic_cast<juce::AudioParameterInt*>(a) != nullptr);
    if (continuous)
    {
        // The same distance on the KNOB's travel (maintainer 2026-11-18): an octave on A's cutoff
        // is about an octave on B's too, not the same number of Hz (skewed ranges).
        const float nowA01    = a->getValue();
        const float beforeA01 = a->convertTo0to1(a->convertFrom0to1(nowA01) - deltaRaw);
        b->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, b->getValue() + (nowA01 - beforeA01)));
    }
    else
        b->setValueNotifyingHost(a->getValue());
}

//==================================================================================================
// Story 22.1 / 22.5 — controller profiles (see ControllerProfiles.h and ARCHITECTURE.md, "Controller input").

// Audio thread: the latest value per (channel, CC) and a dirty bit. Raw bytes, no MidiMessage, no
// allocation. Channels 12..16 are JASS's own (drone, previews, pattern channels) and are skipped.
// 22.5: Shift is tracked here, in buffer order, so every value lands in the slot of the Shift state
// it was sent under; the program-change SysEx releases Shift; a relative delta is summed per CC.
void SynthyProcessor::captureControllers (const juce::MidiBuffer& midi) noexcept
{
    for (const auto meta : midi)
    {
        // Before the CC filter: the device changed its program, a held Shift's release is lost.
        if (ControllerProfiles::isProgramChangeSysEx (meta.data, meta.numBytes))
        {
            ccShiftHeld.store (false);
            continue;
        }
        if (meta.numBytes < 3 || (meta.data[0] & 0xF0) != 0xB0)
            continue;
        const int ch = meta.data[0] & 0x0F;   // 0-based: 0..10 = channels 1..11
        if (ch >= kCcChannels)
            continue;
        const int cc = meta.data[1] & 0x7F;
        const int value = meta.data[2] & 0x7F;
        // A device may send its Shift on a different CC per program (the MiniLab 3: CC 27 in the
        // DAW program, CC 9 in the ARTURIA one) — any of them is the one Shift.
        for (const auto& published : ccShiftCc)
            if (const int shift = published.load(); shift >= 0 && (shift & 0x7F) == cc
                                                    && ((shift >> 8) == 0 || (shift >> 8) == ch + 1))
                ccShiftHeld.store (ControllerProfiles::toggleOn (value));
        const size_t slot = ccShiftHeld.load() ? 1 : 0;
        const size_t at = (size_t) (ch * 128 + cc);
        ccValue[at].store ((std::uint8_t) value);
        ccSlotValue[slot * kCcSlots + at].store ((std::uint8_t) value);
        ccRelative[at].fetch_add (ControllerProfiles::relativeStep (value));
        ccDirty[slot * kCcWords + (size_t) (ch * 2 + (cc >> 6))].fetch_or (std::uint64_t (1) << (cc & 63));
    }
}

namespace
{
    // The MIDI inputs switched on in the standalone's audio/MIDI settings. Empty elsewhere.
    juce::StringArray enabledMidiInputNames()
    {
        juce::StringArray names;
       #if JucePlugin_Build_Standalone
        if (auto* holder = juce::StandalonePluginHolder::getInstance())
            for (const auto& d : juce::MidiInput::getAvailableDevices())
                if (holder->deviceManager.isMidiInputDeviceEnabled (d.identifier))
                    names.add (d.name);
       #endif
        return names;
    }
}

void SynthyProcessor::rescanControllers (bool force)
{
    const auto inputs = enabledMidiInputNames();
    const auto sig = ControllerProfiles::folderSignature();
    if (! force && inputs == controllerInputs && sig == controllerFolderSig)
        return;
    const bool reload = force || sig != controllerFolderSig;
    if (inputs != controllerInputs)
        controllerInputsChanged();
    controllerInputs = inputs;
    controllerFolderSig = sig;
    if (reload)
    {
        controllerProfiles = ControllerProfiles::loadFolder ([this] (const juce::String& id) { return apvts.getParameter (id) != nullptr; });
        ++controllerRevision;
    }
    controllerActive = ControllerProfiles::pick (controllerProfiles, controllerInputs, controllerHandPick);
    controllerProfilePicked();
}

const ControllerProfiles::Profile* SynthyProcessor::activeControllerProfile() const
{
    return juce::isPositiveAndBelow (controllerActive, (int) controllerProfiles.size())
               ? &controllerProfiles[(size_t) controllerActive] : nullptr;
}

// After every re-pick (rescan, hand pick): tell the audio thread which CC is Shift, and drop the
// selection when the profile now active has no "@module" / "@select" entry.
void SynthyProcessor::controllerProfilePicked()
{
    const auto* profile = activeControllerProfile();
    // Every "@shift" entry is the one Shift (one CC per device program); more than kShiftCcs are ignored.
    std::array<int, kShiftCcs> shifts;
    shifts.fill (-1);
    size_t n = 0;
    if (profile != nullptr)
        for (const auto& c : profile->controls)
            if (c.kind == ControllerProfiles::Kind::Shift && n < shifts.size())
                shifts[n++] = (c.channel << 8) | c.cc;
    bool changed = false;
    for (size_t i = 0; i < shifts.size(); ++i)
        changed = (ccShiftCc[i].exchange (shifts[i]) != shifts[i]) || changed;
    if (changed)
        ccShiftHeld.store (false);
    if (profile == nullptr || ! profile->hasModuleTargets())
        clearControllerSelection();
}

// The enabled MIDI inputs changed (cable pulled, device switched off): a Shift that was held never
// sends its release — do not stay in bank mode.
void SynthyProcessor::controllerInputsChanged()
{
    ccShiftHeld.store (false);
}

bool SynthyProcessor::controllerProfileSelectsModules() const
{
    const auto* profile = activeControllerProfile();
    return profile != nullptr && profile->hasModuleTargets();
}

const SynthyProcessor::ControllerFrame* SynthyProcessor::selectedControllerFrame() const
{
    if (controllerSel.valid)
        for (const auto& f : controllerFrames)
            if (f.entryId == controllerSel.entryId && f.layerB == controllerSel.layerB)
                return &f;
    return nullptr;
}

void SynthyProcessor::setControllerSelection (const juce::String& entryId, bool layerB, int bank)
{
    if (controllerSel.valid && controllerSel.entryId == entryId && controllerSel.layerB == layerB && controllerSel.bank == bank)
        return;
    controllerSel.valid = true;
    controllerSel.entryId = entryId;
    controllerSel.layerB = layerB;
    controllerSel.bank = bank;
    ++controllerSelRevision;
}

void SynthyProcessor::clearControllerSelection()
{
    controllerLastSelectIndex = -1;   // the selector's next message selects again
    if (! controllerSel.valid)
        return;
    controllerSel = {};
    ++controllerSelRevision;
}

SynthyProcessor::ControllerSelection SynthyProcessor::getControllerSelection() const
{
    auto s = controllerSel;
    if (const auto* f = selectedControllerFrame())
        s.bankCount = ControllerProfiles::bankCount ((int) f->controls.size());
    return s;
}

bool SynthyProcessor::selectControllerModule (const juce::String& entryId, bool layerB)
{
    if (! controllerProfileSelectsModules())
        return false;
    for (const auto& f : controllerFrames)
        if (f.entryId == entryId && f.layerB == layerB)
        {
            if (! (controllerSel.valid && controllerSel.entryId == entryId && controllerSel.layerB == layerB))
                setControllerSelection (entryId, layerB, 0);   // a new selection starts at bank 1
            return true;
        }
    return false;
}

void SynthyProcessor::setControllerFrames (std::vector<ControllerFrame> frames)
{
    // A module without a countable control is not selectable.
    frames.erase (std::remove_if (frames.begin(), frames.end(), [] (const ControllerFrame& f) { return f.controls.empty(); }),
                  frames.end());
    const int revisionBefore = controllerSelRevision;
    const auto* before = selectedControllerFrame();
    const int banksBefore = before != nullptr ? ControllerProfiles::bankCount ((int) before->controls.size()) : 0;
    controllerFrames = std::move (frames);
    if (! controllerSel.valid)
        return;
    // One rule for every change of what the rack shows: the selection stays while the same module
    // is still on screen — on the other layer's frame too (a swap: the flag flips, the bank is
    // kept) — and is dropped only when the module is gone.
    const ControllerFrame* same = nullptr;
    const ControllerFrame* other = nullptr;
    for (const auto& f : controllerFrames)
        if (f.entryId == controllerSel.entryId)
            (f.layerB == controllerSel.layerB ? same : other) = &f;
    if (const auto* f = same != nullptr ? same : other)
    {
        const int banks = ControllerProfiles::bankCount ((int) f->controls.size());
        setControllerSelection (f->entryId, f->layerB, juce::jlimit (0, banks - 1, controllerSel.bank));
        if (controllerSelRevision == revisionBefore && banks != banksBefore)
            ++controllerSelRevision;   // same module, layer and bank, but the marker's "n/m" changed
    }
    else
        clearControllerSelection();
}

void SynthyProcessor::applyControllerValue (juce::RangedAudioParameter& p, float v01, bool pinned)
{
    // A pinned entry wants its one layer — in A+B, linked editing must not carry it to B.
    // Unpinned in A+B: A is set and mirrorToLayerB moves B.
    const juce::ScopedValueSetter<bool> pin (linkSuspended, linkSuspended || pinned);
    p.beginChangeGesture();
    p.setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v01));
    p.endChangeGesture();
}

// One control of the selected module. Its id is used as it is: a B frame carries the "b" id, an A
// frame in A+B keeps linked editing. A combo steps through its items over the knob's travel, a
// toggle is on from half way.
void SynthyProcessor::applyModuleControl (const ControllerProfiles::ModuleControl& mc, int ccValue7)
{
    auto* p = apvts.getParameter (mc.paramId);
    if (p == nullptr)
        return;
    const float v01 = (float) ccValue7 / 127.0f;
    switch (mc.kind)
    {
        case ControllerProfiles::ControlKind::Knob:
            applyControllerValue (*p, v01, false);
            break;
        case ControllerProfiles::ControlKind::Combo:
            if (const int item = ControllerProfiles::stepIndex (v01, mc.items); item >= 0)
            {
                // Item -> the parameter's own value: its range need not start at 0 or step by 1.
                const auto& range = p->getNormalisableRange();
                applyControllerValue (*p, p->convertTo0to1 (range.start + (float) item * juce::jmax (1.0f, range.interval)), false);
            }
            break;
        case ControllerProfiles::ControlKind::Toggle:
            applyControllerValue (*p, ControllerProfiles::toggleOn (ccValue7) ? 1.0f : 0.0f, false);
            break;
    }
}

void SynthyProcessor::pollControllers()
{
    if (++controllerTicks >= 60)   // ~2 s: a newly enabled input, an edited / added / deleted file
    {
        controllerTicks = 0;
        rescanControllers (false);
    }
    using CP = ControllerProfiles;
    std::array<std::uint64_t, kCcWords * 2> dirty;
    bool any = false;
    for (size_t i = 0; i < dirty.size(); ++i)
    {
        dirty[i] = ccDirty[i].exchange (0);
        any = any || dirty[i] != 0;
    }
    if (! any)
        return;
    const int focus = getLayerFocus();

    // fn (channel 1..11, cc, at, dirty in the not-held slot, dirty in the Shift-held slot), in
    // channel / CC order.
    auto forEachDirty = [&] (auto&& fn)
    {
        for (int ch = 0; ch < kCcChannels; ++ch)
            for (int w = 0; w < 2; ++w)
            {
                const auto plain = dirty[(size_t) (ch * 2 + w)], held = dirty[kCcWords + (size_t) (ch * 2 + w)];
                auto bits = plain | held;
                while (bits != 0)
                {
                    const int b = std::countr_zero (bits);
                    bits &= bits - 1;
                    fn (ch + 1, w * 64 + b, (size_t) (ch * 128 + w * 64 + b), ((plain >> b) & 1) != 0, ((held >> b) & 1) != 0);
                }
            }
    };

    // Pass 1 — the selector, before any other control: in the DAW program knob 8 (CC 117) drains
    // after knobs 1..7, and they must already act on the module it picked. (Shift itself is
    // tracked at capture.) Shift held: the bank of the selected module; not held: the module.
    if (const auto* profile = activeControllerProfile(); profile != nullptr && profile->hasModuleTargets())
        forEachDirty ([&] (int channel, int cc, size_t at, bool plain, bool held)
        {
            for (size_t i = 0; i < profile->controls.size(); ++i)
            {
                const auto& c = profile->controls[i];
                if (c.kind != CP::Kind::Select || ! CP::matches (c, channel, cc))
                    continue;
                if (held)
                {
                    const float v01 = (float) ccSlotValue[kCcSlots + at].load() / 127.0f;
                    if (const auto* f = selectedControllerFrame())   // ignored with nothing selected
                        setControllerSelection (f->entryId, f->layerB,
                                                juce::jmax (0, CP::stepIndex (v01, CP::bankCount ((int) f->controls.size()))));
                    // The knob stands somewhere else now: remember that position, so the first
                    // message after Shift is released does not jump to another module by itself.
                    controllerLastSelectIndex = CP::stepIndex (v01, (int) controllerFrames.size());
                }
                if (plain)
                {
                    // Absolute: the position picks the module, so the first touch jumps. A module is
                    // selected only when the index differs from the last one this knob produced —
                    // a list change or a mouse selection never reselects by itself.
                    const float v01 = (float) ccSlotValue[at].load() / 127.0f;
                    const int index = CP::stepIndex (v01, (int) controllerFrames.size());
                    if (index >= 0 && index != controllerLastSelectIndex)
                    {
                        const auto& f = controllerFrames[(size_t) index];
                        if (! (controllerSel.valid && controllerSel.entryId == f.entryId && controllerSel.layerB == f.layerB))
                            setControllerSelection (f.entryId, f.layerB, 0);
                    }
                    controllerLastSelectIndex = index;
                }
            }
        });

    // Pass 2 — everything else. The relative sum is drained for every dirty CC, mapped or not, so
    // a later profile never inherits old detents.
    forEachDirty ([&] (int channel, int cc, size_t at, bool, bool)
    {
        const int detents = ccRelative[at].exchange (0);
        const auto* profile = activeControllerProfile();
        if (profile == nullptr)
            return;
        // Absolute 0..127 onto the knob's travel; the parameter's own skew applies. Jump on first
        // touch.
        const int value = ccValue[at].load();
        for (size_t i = 0; i < profile->controls.size(); ++i)
        {
            const auto& c = profile->controls[i];
            if (! CP::matches (c, channel, cc))
                continue;
            if (c.kind == CP::Kind::TempoFine)
            {
                if (auto* p = apvts.getParameter (Parameters::ID::syncTempo); p != nullptr && detents != 0)
                {
                    const auto& range = p->getNormalisableRange();
                    const double bpm = CP::tempoAfterDetents (p->convertFrom0to1 (p->getValue()), detents,
                                                              juce::jmax (40.0, (double) range.start), juce::jmin (250.0, (double) range.end));
                    applyControllerValue (*p, p->convertTo0to1 ((float) bpm), false);
                }
                continue;
            }
            if (c.kind == CP::Kind::Module)
                if (const auto* f = selectedControllerFrame())
                {
                    // The n-th control of the current bank; a knob beyond the list moves nothing.
                    const size_t n = (size_t) (controllerSel.bank * CP::kBankSize + c.moduleSlot - 1);
                    if (n < f->controls.size())
                    {
                        const auto mc = f->controls[n];   // a copy: a parameter write may relayout the rack
                        applyModuleControl (mc, value);
                    }
                    continue;
                }
            // The fixed path: a plain entry, or an "@module:n" entry's fallback while nothing is selected.
            CP::Target t;
            if (CP::fixedTarget (c, focus, t))
                if (auto* p = apvts.getParameter (t.paramId))
                    applyControllerValue (*p, (float) value / 127.0f, t.pinned);
        }
    });
}

void SynthyProcessor::runControllersForTest (std::vector<ControllerProfiles::Profile> profiles, const juce::MidiBuffer& midi)
{
    controllerProfiles = std::move (profiles);
    controllerActive = controllerProfiles.empty() ? -1 : 0;
    controllerProfilePicked();
    captureControllers (midi);
    controllerTicks = 0;   // no folder rescan inside the test
    pollControllers();
}

juce::StringArray SynthyProcessor::getControllerProfileNames() const
{
    juce::StringArray names;
    for (const auto& p : controllerProfiles)
        names.add (p.name);
    return names;
}

juce::String SynthyProcessor::getActiveControllerProfile() const
{
    const auto* profile = activeControllerProfile();
    return profile != nullptr ? profile->name : juce::String();
}

void SynthyProcessor::setControllerProfileChoice (const juce::String& name)
{
    controllerHandPick = name == "Auto" ? juce::String() : name;
    PresetIO::setSetting ("ControllerProfile", controllerHandPick);
    controllerActive = ControllerProfiles::pick (controllerProfiles, controllerInputs, controllerHandPick);
    controllerProfilePicked();
}
