#include "PatchEngine.h"
#include "../DSP/SyncDivision.h"

PatchEngine::PatchEngine (juce::AudioProcessorValueTreeState& apvts, const juce::String& idPrefix)
{
    p.resolve (apvts, idPrefix);

    for (int i = 0; i < 8; ++i)
        synth.addVoice (new SynthVoice());
    synth.addSound (new SynthSound());

    // Choke groups (Story 12.7) need sibling access: a voice knows only its own note, so a closed
    // hi-hat could never reach the open one ringing in another voice. Build the roster ONCE here —
    // the voice list never changes afterwards — and hand every voice a pointer to it. The roster is
    // read-only on the audio thread; the synthesiser stays the sole owner of the voices.
    voiceRoster.reserve ((size_t) synth.getNumVoices());
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<SynthVoice*> (synth.getVoice (i)))
            voiceRoster.push_back (v);
    for (auto* v : voiceRoster)
    {
        v->setVoicePeers (&voiceRoster);
        v->setEngine (this);   // 21.4: a skipped (idle) voice pulls the block push at note-on
    }
    // GRAIN (17.1): one random stream per voice — golden-ratio multiples spread the xorshift seeds
    // apart, so a chord is several different clouds, not one cloud summed coherently (+6 dB).
    for (size_t i = 0; i < voiceRoster.size(); ++i)
        voiceRoster[i]->getGrain().seed (0x9E3779B9u * (uint32_t) (i + 1));
}

void PatchEngine::prepare (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = static_cast<SynthVoice*> (synth.getVoice (i)))
        {
            voice->prepareToPlay (sampleRate, samplesPerBlock);
            voice->setGlideInfo (&glideInfo);   // poly-glide: shared per-block source ratios
        }

    // Pre-size the glide note lists so the audio thread never reallocates.
    glideHeld.reserve (128); glideLastChord.reserve (128);
    glideNewNotes.reserve (128); glideOffNotes.reserve (128);
    glideRebuiltScratch.ensureSize (2048);   // RT: no per-block MidiBuffer growth

    for (auto& l : uiLfos) l.setSampleRate (sampleRate);
}

void PatchEngine::beginBlock (const BlockContext& ctx)
{
    // Per-LFO effective rate (Tempo-Sync resolved once per block; Free => raw RATE knob).
    for (int i = 0; i < kNumLFOs; ++i)
    {
        const int div = (int) *p.lfoSyncDiv[i];
        lfoRateHz[i] = SyncDivision::isSynced(div)
                           ? SyncDivision::lfoRateHz(ctx.syncBpm, div)
                           : (double) *p.lfoRate[i];
        lfoGlobalActive[i] = *p.lfoOn[i] > 0.5f
                          && *p.lfoGlobal[i] > 0.5f;
    }

    const int delayDiv = (int) *p.delaySyncDiv;
    blockDelayTimeSec = SyncDivision::isSynced(delayDiv)
                                    ? SyncDivision::delaySeconds(ctx.syncBpm, delayDiv)
                                    : (double) *p.delayTime;

    // Story 12.1: shared SAMPLER loop clock — ONE master phase advancing at the ROOT rate × SPEED
    // through the START..END region. Loop-mode notes START at this phase (SamplePlayer::trigger),
    // so simultaneous/late notes stay in step instead of each restarting the loop at START.
    {
        const auto* set = SampleBankStore::instance().getSet(
            static_cast<int>(*p.samplerSet));
        const bool on = *p.samplerOn > 0.5f;
        // 12.2 (user request, tightened 2026-08-04): the clock runs only while at least one KEY
        // is held (sustain pedal counts) — release TAILS no longer hold the round open, so the
        // first note after letting go always hits the sample's attack without waiting for the
        // previous tone to fully fade. Notes added while a key is still held keep joining the
        // running round (beat-lock preserved). The auto-play drone holds its key permanently,
        // so with auto-play on the clock free-runs as before.
        bool anyVoiceSounding = false;
        for (int vi = 0; vi < synth.getNumVoices() && ! anyVoiceSounding; ++vi)
            if (auto* v = synth.getVoice(vi))
                anyVoiceSounding = v->isVoiceActive() && (v->isKeyDown() || v->isSustainPedalDown());
        // The clock's reference is the zone C4 would play — for a single sample that is THE zone
        // (12.1 unchanged); for a mapped set it anchors the round on the centre zone (its own
        // root, since the ROOT knob is inert for mapped sets).
        const auto* zone = (set != nullptr) ? set->zoneFor(60) : nullptr;
        if (zone != nullptr && on && anyVoiceSounding && zone->getLength() > 4)
        {
            const double root  = set->isMapped() ? (double) zone->rootKey
                                                 : (double) *p.samplerRoot;
            const double speed = *p.samplerSpeed;
            const double s0 = *p.samplerStart;
            const double s1 = *p.samplerEnd;
            const double lenFile = std::abs(s1 - s0) * (double) (zone->getLength() - 1);
            if (lenFile > 4.0)
            {
                // 12.3 STRETCH mode: the round advances on the TIME axis only (speed, no pitch
                // factor) — voices walk it identically, so the clock must too. Tape mode keeps
                // the root-rate round of 12.1.
                const bool stretchOn = *p.samplerStretch > 0.5f;
                const double pitchPart = stretchOn ? 1.0 : std::pow(2.0, (60.0 - root) / 12.0);
                const double rate = pitchPart * zone->fileSampleRate / ctx.sampleRate * speed;
                samplerMasterFrac += (double) ctx.numSamples * rate / lenFile;
                samplerMasterFrac -= std::floor(samplerMasterFrac);
            }
        }
        else
            samplerMasterFrac = 0.0;   // off/empty/silent: clock parks at START
    }

    blockChaosX = ctx.chaosX;
    blockChaosY = ctx.chaosY;
    blockAccent = ctx.accentDepth;
    for (int li = 0; li < kNumLFOs; ++li)   // 20.1: a GLOBAL LFO's value — the engine's instance,
    {                                       // one block behind, like the rings
        globalLfoPushedBefore[li] = globalLfoPushed[li];
        globalLfoPushed[li]       = lfoDisplayValues[li].load();
    }

    // Update the voice parameters — only where a note plays (21.4). An idle voice renders nothing,
    // so its parameters matter only once a note starts on it: it is marked stale and pulls the
    // push in startNote (refreshVoice), inside this block, with this block's values.
    for (auto* voice : voiceRoster)
    {
        if (voice->isVoiceActive())
        {
            pushToVoice(*voice);
            voice->clearParamsStale();
        }
        else
            voice->markParamsStale();
    }
}

void PatchEngine::refreshVoice (SynthVoice& voice)
{
    // The previous block's GLOBAL LFO values first: a voice pushed every block would hold them as
    // its ramp start (setGlobalLfo shifts current → previous).
    for (int li = 0; li < kNumLFOs; ++li)
        voice.setGlobalLfo(li, lfoGlobalActive[li], globalLfoPushedBefore[li]);
    pushToVoice(voice);
    voice.clearParamsStale();
}

void PatchEngine::pushToVoice (SynthVoice& v)
{
    auto* voice = &v;
    voice->setChaosValues(blockChaosX, blockChaosY);   // one orbit, all voices
    for (int li = 0; li < kNumLFOs; ++li)
        voice->setGlobalLfo(li, lfoGlobalActive[li], globalLfoPushed[li]);
    voice->setAccentDepth(blockAccent);   // 15.2
    Parameters::applyToVoice(p, voice->getOscillators(),
                             voice->getEnvelope(), voice->getStrips(),   // Epic 10: per-channel FX strips
                             voice->getLFOs(), voice->getNoise(),
                             voice->getKarplus(), voice->getWavetable(),
                             voice->getSampler(), samplerMasterFrac,   // Story 12.1 (+ loop clock)
                             voice->getGrain(),                         // Story 17.1
                             voice->getMixMode(),
                             voice->getSubOsc(), voice->getSubOctaveRef(),
                             voice->getAdsrOnRef(), voice->getMixModeOnRef(),
                             voice->getMixSrcARef(), voice->getMixSrcBRef(),
                             voice->getPitchEnv(), voice->getPitchEnvAmountRef(),
                             voice->getPitchEnvOnRef(),
                             voice->getModSlots(), voice->getModMatrixOnRef(),
                             voice->getOutputModeRef(), voice->getGeneratorPan(),   // Epic 10
                             lfoRateHz, blockDelayTimeSec,
                             voice->getFilterKeytrackRef());
}

void PatchEngine::slideVoice (int note, double semitones, double seconds)
{
    for (int v = 0; v < synth.getNumVoices(); ++v)
        if (auto* voice = static_cast<SynthVoice*>(synth.getVoice(v)))
            if (voice->isVoiceActive() && voice->getCurrentlyPlayingNote() == note)
            {
                voice->slideTo(semitones, seconds);
                break;
            }
}

void PatchEngine::setPluckEnabled (bool enabled)
{
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = static_cast<SynthVoice*>(synth.getVoice(i)))
            v->setPluckEnabled(enabled);
}

void PatchEngine::pluck()
{
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = static_cast<SynthVoice*>(synth.getVoice(i)))
            v->pluckKarplus();
}

void PatchEngine::processGlide (juce::MidiBuffer& midiMessages, int droneChannel)
{
    glideInfo.enabled = *p.glideOn > 0.5f;
    glideInfo.timeSec = *p.glideTime;
    glideInfo.startRatio.fill(-1.0f);

    glideNewNotes.clear();
    glideOffNotes.clear();
    for (const auto meta : midiMessages)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn()) noteOnThisBlock = true;   // the rings' per-voice One-Shot restarts too (endBlock)
        if (m.getChannel() == droneChannel) continue;   // never glide the auto-play drone
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
    const bool glideMono = (int) *p.glideMode == 0;
    if (glideInfo.enabled && glideMono)
    {
        auto& rebuilt = glideRebuiltScratch; rebuilt.clear();   // reused member (no per-block alloc)
        for (const auto meta : midiMessages)
        {
            const auto m = meta.getMessage();
            const int  sp = meta.samplePosition;
            if (m.isNoteOn() && m.getChannel() != droneChannel)
            {
                // On the channel the sounding note STARTED on — the synth matches note-offs by
                // channel, and ARP / STEP SEQ notes play on the pattern channel (21.7 review).
                if (monoSounding >= 0 && (monoSounding != m.getNoteNumber() || monoSoundingChannel != m.getChannel()))
                    rebuilt.addEvent (juce::MidiMessage::noteOff (monoSoundingChannel, monoSounding), sp);
                monoSounding = m.getNoteNumber();
                monoSoundingChannel = m.getChannel();
                rebuilt.addEvent (m, sp);
            }
            else if (m.isNoteOff() && m.getChannel() != droneChannel)
            {
                if (m.getNoteNumber() == monoSounding && m.getChannel() == monoSoundingChannel)
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

void PatchEngine::endBlock (const BlockContext& ctx)
{
    // Advance the display LFOs so the editor can draw live modulation rings — and, since 20.1,
    // because a GLOBAL LFO IS this instance: what the ring shows is what every voice hears.
    // Mirrors the patch LFO params; runs regardless of whether a note sounds.
    const bool rearm = rearmGlobalLfos.exchange(false);   // preset / DAW-state load (20.1)
    for (int li = 0; li < kNumLFOs; ++li)
    {
        uiLfos[li].setRate(lfoRateHz[li]);   // mirror the effective (synced or free) rate
        uiLfos[li].setDepth(*p.lfoDepth[li]);
        uiLfos[li].setWaveform((LFOWaveform)(int) *p.lfoWave[li]);
        const bool on = *p.lfoOn[li] > 0.5f;
        uiLfos[li].setTarget(on ? (LFOTarget)((int) *p.lfoTarget[li] + 1)
                                : LFOTarget::Off);
        // 20.1 GLOBAL: rearm on the rising edge of (on && GLOBAL) and on a load, so a global
        // One-Shot sweeps once from "now" (a per-voice One-Shot is rearmed by the key instead).
        // In a DAW with SYNC on the phase is pinned to the song position every block — bar-locked,
        // so a locate or STOP/PLAY lands on the bar by construction — and it stands while the
        // host stands. A global LFO with SYNC = Free keeps running: it is a movement, not a clock.
        const bool global = lfoGlobalActive[li];
        if (global && (rearm || ! lfoGlobalWasOn[li]))
            uiLfos[li].reset();
        // A per-voice One-Shot sweeps once from every key, so its ring must too (2026-11-23: the
        // MOD MATRIX ring on B's CUTOFF stood still — B's Init bass sweeps it with such an LFO; the
        // display instance had swept once at the load and then sat at 0).
        if (! global && noteOnThisBlock && uiLfos[li].getWaveform() == LFOWaveform::OneShot)
            uiLfos[li].reset();
        lfoGlobalWasOn[li] = global;
        const int  div    = (int) *p.lfoSyncDiv[li];
        // A One-Shot is never pinned: pinning would restart it every cycle, and its point is to
        // sweep ONCE from the rearm (review 2026-11-08, maintainer's call). It still takes its
        // sweep time from the synced rate.
        const bool pinned = global && ctx.hosted && ctx.hostPpq.hasValue() && SyncDivision::isSynced(div)
                         && uiLfos[li].getWaveform() != LFOWaveform::OneShot;
        if (pinned)
        {
            double ph = std::fmod(*ctx.hostPpq / SyncDivision::beatsPerCycle(div), 1.0);
            if (ph < 0.0) ph += 1.0;
            uiLfos[li].setPhase(ph);
        }
        float v = lfoDisplayValues[li].load();   // held while a pinned LFO's host stands
        if (! (pinned && ! ctx.hostPlaying))
            for (int i = 0, n = ctx.numSamples; i < n; ++i)
                v = uiLfos[li].process();
        lfoDisplayValues[li].store(v);
    }
    noteOnThisBlock = false;
}
