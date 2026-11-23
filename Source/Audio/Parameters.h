#pragma once
#include <JuceHeader.h>
#include "../DSP/Oscillator.h"
#include "../DSP/BiquadFilter.h"
#include "../DSP/Effects.h"
#include "../DSP/AdsrEnvelope.h"
#include "../DSP/PitchEnvelope.h"
#include "../DSP/LFO.h"
#include "../DSP/NoiseGenerator.h"
#include "../DSP/KarplusStrong.h"
#include "../DSP/WavetableOscillator.h"
#include "../DSP/SamplePlayer.h"          // Story 12.1: SAMPLER generator (+ SampleBank store)
#include "../DSP/GrainEngine.h"           // Story 17.1: GRAIN generator (cloud on the sampler's set)
#include "../DSP/StepSequencer.h"         // 16.3: kMaxSteps — the indexed-ID counts derive from it
#include "../DSP/PercSequencer.h"         //       (one source of truth, no drifting literals)
#include "../DSP/SyncDivision.h"
#include "../DSP/ModMatrix.h"
#include "../DSP/ModMatrixCatalog.h"      // ModDest: MOD MATRIX destination = MODULE → PARAM
#include "../DSP/ChannelStrip.h"          // Epic 10: ChannelStrip (per-channel FX), OutputMode, pans
#include "../Modules/ModuleRegistry.h"   // spec-driven modules — generates APVTS params (audio-safe)

namespace Parameters
{
    // Parameter IDs
    namespace ID
    {
        // Indexed parameter IDs (RT-safety, Story 11.1): these used to build a fresh juce::String on
        // every call — ~550 heap allocations per audio block once applyToVoice reads them 8×/block.
        // Each now returns a reference into a static cache built ONCE (lazily, then warmed on the
        // message thread via warmIndexedIds()), so the audio thread never constructs a String here.
        // `count` is the max 1-based index the helper is ever called with; the index is clamped.
        #define JASS_INDEXED_ID(fn, count, prefix, suffix)                                              \
            inline const juce::String& fn (int i)                                                       \
            {                                                                                           \
                static const auto cache = []                                                            \
                {                                                                                       \
                    std::array<juce::String, (size_t) (count)> a;                                       \
                    for (int k = 0; k < (count); ++k) a[(size_t) k] = juce::String (prefix) + juce::String (k + 1) + suffix; \
                    return a;                                                                           \
                }();                                                                                    \
                return cache[(size_t) (juce::jlimit (1, (int) (count), i) - 1)];                        \
            }

        // Oscillators (max index 3)
        JASS_INDEXED_ID (oscOn,        3, "osc", "On")
        JASS_INDEXED_ID (oscWave,      3, "osc", "Wave")
        JASS_INDEXED_ID (oscFreq,      3, "osc", "Freq")
        JASS_INDEXED_ID (oscAmp,       3, "osc", "Amp")
        JASS_INDEXED_ID (oscUniVoices, 3, "osc", "UniVoices")
        JASS_INDEXED_ID (oscUniDetune, 3, "osc", "UniDetune")
        JASS_INDEXED_ID (oscFeedback,  3, "osc", "Feedback")   // Self-FM depth (append-only)
        JASS_INDEXED_ID (oscPan,       3, "osc", "Pan")        // Epic 10: per-oscillator stereo pan

        // Mix mode
        constexpr const char* mixMode = "mixMode";
        constexpr const char* mixSrcA = "mixSrcA";   // Epic 5: RingMod/FM operand A (0..2 => OSC 1/2/3)
        constexpr const char* mixSrcB = "mixSrcB";   // Epic 5: RingMod/FM operand B

        // ADSR
        constexpr const char* attack    = "attack";
        constexpr const char* decay     = "decay";
        constexpr const char* sustain   = "sustain";
        constexpr const char* release   = "release";

        // Filter
        constexpr const char* filterOn     = "filterOn";
        constexpr const char* filterType   = "filterType";
        constexpr const char* filterCutoff = "filterCutoff";
        constexpr const char* filterReso   = "filterReso";
        constexpr const char* filterTrack  = "filterTrack";

        // Distortion
        constexpr const char* distortionOn    = "distortionOn";
        constexpr const char* distortionType  = "distortionType";
        constexpr const char* distortionDrive = "distortionDrive";
        constexpr const char* distortionMix   = "distortionMix";

        // Formant / vowel filter (append-only)
        constexpr const char* formantOn    = "formantOn";
        constexpr const char* formantVowel = "formantVowel";
        constexpr const char* formantReso  = "formantReso";
        constexpr const char* formantMix   = "formantMix";

        // Wavefolder
        constexpr const char* wavefoldOn       = "wavefoldOn";
        constexpr const char* wavefoldDrive    = "wavefoldDrive";
        constexpr const char* wavefoldSymmetry = "wavefoldSymmetry";
        constexpr const char* wavefoldMix      = "wavefoldMix";

        // Bitcrusher (lo-fi)
        constexpr const char* bitcrushOn   = "bitcrushOn";
        constexpr const char* bitcrushBits = "bitcrushBits";
        constexpr const char* bitcrushRate = "bitcrushRate";
        constexpr const char* bitcrushMix  = "bitcrushMix";

        // Phaser / Flanger (append-only)
        constexpr const char* phaserOn       = "phaserOn";
        constexpr const char* phaserType     = "phaserType";
        constexpr const char* phaserRate     = "phaserRate";
        constexpr const char* phaserDepth    = "phaserDepth";
        constexpr const char* phaserFeedback = "phaserFeedback";
        constexpr const char* phaserMix      = "phaserMix";

        // Arpeggiator (turns a held chord into an automatic note sequence)
        constexpr const char* arpOn      = "arpOn";
        constexpr const char* arpRate    = "arpRate";
        constexpr const char* arpMode    = "arpMode";
        constexpr const char* arpOctaves = "arpOctaves";
        constexpr const char* arpGate    = "arpGate";
        constexpr const char* arpSync    = "arpSync";   // 21.9: Free | note values (SyncDivision)
        // (arpTarget, 21.7, is retired since 21.8 — never reuse the id.)

        // STEP SEQ (Story 15.1) — an authored 16-step figure. Per-step params are indexed like the
        // oscillators/LFOs so the audio thread never builds a String (RT-safety, Story 11.1).
        constexpr const char* seqOn     = "seqOn";
        constexpr const char* seqSync   = "seqSync";
        constexpr const char* seqRate   = "seqRate";
        constexpr const char* seqLength = "seqLength";
        constexpr const char* seqGate   = "seqGate";     // ONE note length for the whole pattern
        constexpr const char* seqAccent = "seqAccent";   // ACCENT depth: what an accented step does (15.2)
        // The step cells (seqPitch<n> / seqStep<n> / seqAcc<n> / seqSGate<n>) are NO LONGER
        // parameters (AD-14, Story 18.5 stage 2): they live in PatternStore::step. A DAW state
        // saved before that still carries them as <PARAM> elements, folded in once by
        // PluginProcessor::setStateInformation under these prefixes.
        constexpr const char* seqPitchLegacyPrefix = "seqPitch";
        constexpr const char* seqStepLegacyPrefix  = "seqStep";
        constexpr const char* seqAccLegacyPrefix   = "seqAcc";
        constexpr const char* seqSGateLegacyPrefix = "seqSGate";

        // PERC (Story 16.1) — four percussion tracks rendered straight to the master bus. The step
        // grid is one indexed id PER LANE (a 2-D index would have to build a String on the audio
        // thread; these are the same cached-array ids as everywhere else, RT-safe by Story 11.1).
        constexpr const char* percOn     = "percOn";
        constexpr const char* percKit    = "percKit";     // dynamic set list, stored as an index
        constexpr const char* percAmp    = "percAmp";     // the kit's master level (per-lane AMPs balance)
        constexpr const char* percSync   = "percSync";
        constexpr const char* percRate   = "percRate";
        constexpr const char* percLength = "percLength";
        JASS_INDEXED_ID (percNote,  4, "percNote",  "")   // which instrument of the kit a lane fires
        JASS_INDEXED_ID (percLevel, 4, "percLevel", "")
        JASS_INDEXED_ID (percPan,   4, "percPan",   "")   // per-lane placement (16.1: hats off-centre)
        // (The BASS box ids bass* and seqToBass, Story 20.2, are retired since 2026-11-23 — never
        // reuse them: old DAW projects still carry them.)
        // (seqTarget, 21.7, is retired since 21.8 — never reuse the id.)
        // The grid cells (percStep<lane>_<step>) are NO LONGER parameters (AD-14, Story 18.5):
        // they live in PatternStore::perc. A DAW state saved before 18.5 still carries them as
        // <PARAM> elements; PluginProcessor::setStateInformation folds those into the store once.
        constexpr const char* percStepLegacyPrefix = "percStep";   // "percStep2_17" = lane 2, step 17

        // Portamento / glide (append-only)
        constexpr const char* glideOn   = "glideOn";
        constexpr const char* glideTime = "glideTime";
        constexpr const char* glideMode = "glideMode";   // 0=Mono (one voice, distinct glide), 1=Poly (per-voice)

        // Pitch envelope (one-shot pitch sweep; append-only)
        constexpr const char* pitchEnvOn     = "pitchEnvOn";
        constexpr const char* pitchEnvAmount = "pitchEnvAmount";   // semitones (bipolar)
        constexpr const char* pitchEnvTime   = "pitchEnvTime";     // decay seconds

        // Stereo width (pseudo-stereo master stage; mono engine -> stereo)
        constexpr const char* stereoOn    = "stereoOn";
        constexpr const char* stereoWidth = "stereoWidth";
        constexpr const char* stereoTime  = "stereoTime";
        constexpr const char* outputMode  = "outputMode";   // Epic 10: Mono / Pseudo-Stereo / Stereo-Pan
        constexpr const char* hrtfRoom    = "hrtfRoom";     // Story 10.4: Kunstkopf early-reflection amount

        // Master-bus compressor (append-only)
        constexpr const char* compOn        = "compOn";
        constexpr const char* compThreshold = "compThreshold";   // dB
        constexpr const char* compRatio     = "compRatio";       // 1..20
        constexpr const char* compAttack    = "compAttack";      // ms
        constexpr const char* compRelease   = "compRelease";     // ms
        constexpr const char* compMakeup    = "compMakeup";      // dB

        // Sub oscillator (tracks OSC 1 pitch, octave(s) down)
        constexpr const char* subOn     = "subOn";
        constexpr const char* subWave   = "subWave";
        constexpr const char* subOctave = "subOctave";
        constexpr const char* subLevel  = "subLevel";
        constexpr const char* subPan    = "subPan";         // Epic 10: stereo placement
        constexpr const char* subFeedback = "subFeedback";  // Self-FM depth (append-only)

        // Delay
        constexpr const char* delayOn       = "delayOn";
        constexpr const char* delayTime     = "delayTime";
        constexpr const char* delayFeedback = "delayFeedback";
        constexpr const char* delayMix      = "delayMix";
        constexpr const char* delaySyncDiv  = "delaySyncDiv";   // Tempo-Sync: 0=Free, else note division (append-only)

        // Chorus
        constexpr const char* chorusOn    = "chorusOn";
        constexpr const char* chorusRate  = "chorusRate";
        constexpr const char* chorusDepth = "chorusDepth";
        constexpr const char* chorusMix   = "chorusMix";

        // LFO — indexed 1..kNumLFOs, exactly like the oscillators (oscOn(i) …). lfoOn(1)="lfo1On".
        // A new LFO = bump kNumLFOs (LFO.h) + append a ModSource; every layer loops over these.
        JASS_INDEXED_ID (lfoOn,      kNumLFOs, "lfo", "On")       // max index kNumLFOs
        JASS_INDEXED_ID (lfoWave,    kNumLFOs, "lfo", "Wave")
        JASS_INDEXED_ID (lfoRate,    kNumLFOs, "lfo", "Rate")
        JASS_INDEXED_ID (lfoDepth,   kNumLFOs, "lfo", "Depth")
        JASS_INDEXED_ID (lfoTarget,  kNumLFOs, "lfo", "Target")
        JASS_INDEXED_ID (lfoSyncDiv, kNumLFOs, "lfo", "SyncDiv")
        JASS_INDEXED_ID (lfoGlobal,  kNumLFOs, "lfo", "Global")    // 20.1: one phase for all voices

        // CHAOS — Lorenz attractor mod source (LFO expansion)
        constexpr const char* chaosOn   = "chaosOn";
        constexpr const char* chaosRate = "chaosRate";

        // Reverb
        constexpr const char* reverbOn   = "reverbOn";
        constexpr const char* reverbRoom = "reverbRoom";
        constexpr const char* reverbDamp = "reverbDamp";
        constexpr const char* reverbMix  = "reverbMix";

        // Noise
        constexpr const char* noiseOn   = "noiseOn";
        constexpr const char* noiseType = "noiseType";
        constexpr const char* noiseAmp  = "noiseAmp";
        constexpr const char* noisePan  = "noisePan";       // Epic 10: stereo placement

        // Karplus-Strong
        constexpr const char* karplusOn      = "karplusOn";
        constexpr const char* karplusFreq    = "karplusFreq";
        constexpr const char* karplusAmp     = "karplusAmp";
        constexpr const char* karplusDamping = "karplusDamping";
        constexpr const char* karplusStretch = "karplusStretch";
        constexpr const char* karplusPan     = "karplusPan";   // Epic 10: stereo placement

        // Wavetable
        constexpr const char* wavetableOn        = "wavetableOn";
        constexpr const char* wavetableBank      = "wavetableBank";
        constexpr const char* wavetablePosition  = "wavetablePosition";
        constexpr const char* wavetableFreq      = "wavetableFreq";
        constexpr const char* wavetableAmp       = "wavetableAmp";
        constexpr const char* wavetableUniVoices = "wavetableUniVoices";
        constexpr const char* wavetableUniDetune = "wavetableUniDetune";
        constexpr const char* wavetablePan       = "wavetablePan";   // Epic 10: stereo placement
        constexpr const char* wavetableFeedback  = "wavetableFeedback";   // Self-FM depth (append-only)

        // Master
        constexpr const char* masterVol = "masterVol";

        // Global sync tempo (BPM). Used for Tempo-Sync when no host tempo is available
        // (Standalone) or as the base; the host's BPM overrides it in a DAW. Append-only.
        constexpr const char* syncTempo = "syncTempo";

        // Module enables for the formerly always-on modules (Story 2.4). Append-only,
        // default TRUE so existing users/presets are unchanged until toggled.
        constexpr const char* masterOn  = "masterOn";
        constexpr const char* adsrOn    = "adsrOn";
        constexpr const char* mixModeOn = "mixModeOn";

        // Display enables (Oscilloscope / Spectrum). UI-only (dim the display when off);
        // still real APVTS params so every module has a working enabler. Append-only, default true.
        constexpr const char* scopeOn    = "scopeOn";
        constexpr const char* spectrumOn = "spectrumOn";

        // On-screen keyboard enable (INPUT zone). UI-only placeholder for now (the toggle
        // drives only the module's dim state; the keyboard stays playable) — reserved so the
        // module carries a working enabler for a future use. Append-only, default true.
        constexpr const char* keyboardOn = "keyboardOn";

        // Sampler (Story 12.1)
        constexpr const char* samplerOn    = "samplerOn";
        constexpr const char* samplerSet   = "samplerSet";     // index into SampleBankStore (session)
        constexpr const char* samplerRoot  = "samplerRoot";    // MIDI key at original speed
        constexpr const char* samplerStart = "samplerStart";
        constexpr const char* samplerEnd   = "samplerEnd";
        constexpr const char* samplerMode  = "samplerMode";    // One-Shot / Loop / Reverse / Rev-Loop
        constexpr const char* samplerLevel = "samplerLevel";
        constexpr const char* samplerPan   = "samplerPan";
        constexpr const char* samplerSpeed = "samplerSpeed";   // playback-rate multiplier (tape-style)
        constexpr const char* samplerStretch = "samplerStretch";   // 12.3: pitch/time decoupling on/off
        constexpr const char* samplerRelease = "samplerRelease";   // 12.4: note-off fade fallback (s); 0 = off

        // GRAIN (Story 17.1): granular cloud on the SAMPLER's set. No set param of its own.
        constexpr const char* grainOn      = "grainOn";
        constexpr const char* grainPos     = "grainPos";       // centre of grain starts, fraction of the zone
        constexpr const char* grainSpray   = "grainSpray";     // random spread around POS, fraction of the zone
        constexpr const char* grainSize    = "grainSize";      // grain length, ms
        constexpr const char* grainDensity = "grainDensity";   // grains per second
        constexpr const char* grainPitch   = "grainPitch";     // per-grain pitch spread, semitones
        constexpr const char* grainQuant   = "grainQuant";     // 0 Off, 1 Chrom, 2 Major, 3 Minor, 4 Penta
        constexpr const char* grainAmp     = "grainAmp";
        constexpr const char* grainPan     = "grainPan";
        constexpr const char* grainKey     = "grainKey";       // 17.2: pitch-synchronous mode (rate = note)

        // Preset quick-access bank enable (MASTER BUS). UI-only (dim placeholder) — the F1..F12
        // slot assignments themselves are a GLOBAL app setting (PresetBanks.json), not per-preset,
        // so only this enabler is an APVTS param. Append-only, default true.
        constexpr const char* presetBankOn = "presetBankOn";

        // Modulation matrix (Story 8.1 / Epic 8). N routing slots, each {Source, Module, Param,
        // Amount}, plus a master enable. Append-only, indexed helpers (mirror oscFreq(i)).
        // v5 (Epic 8.3): the flat DEST target became MODULE (ModDest module index) + PARAM (param
        // index within that module). The legacy modSlotTarget id is migrated away (PresetIO / XML).
        JASS_INDEXED_ID (modSlotSource,       ModMatrixConfig::kNumSlots, "modSlot", "Source")   // max index kNumSlots
        JASS_INDEXED_ID (modSlotModule,       ModMatrixConfig::kNumSlots, "modSlot", "Module")
        JASS_INDEXED_ID (modSlotParam,        ModMatrixConfig::kNumSlots, "modSlot", "Param")
        JASS_INDEXED_ID (modSlotAmount,       ModMatrixConfig::kNumSlots, "modSlot", "Amount")
        JASS_INDEXED_ID (modSlotTargetLegacy, ModMatrixConfig::kNumSlots, "modSlot", "Target")   // v4 and older
        JASS_INDEXED_ID (modSlotQuant,        ModMatrixConfig::kNumSlots, "modSlot", "Quant")    // LFO expansion: per-slot scale mask
        constexpr const char* modMatrixOn = "modMatrixOn";

        #undef JASS_INDEXED_ID

        // LAYERS (AD-16, Story 21.5) — instrument-level. Layer B's patch params are not listed
        // here: they are A's ids with Modules::kLayerBPrefix in front, made by the spec walk.
        constexpr const char* layerMode   = "layerMode";    // 0 = no split, 2 = Split (1 = the old Dual, read as 0)
        constexpr const char* splitNote   = "splitNote";    // SPLIT: below → B, from it up → A
        constexpr const char* layerFocus  = "layerFocus";   // 0 = A, 1 = B, 2 = A+B linked (which layer the rack edits)
        constexpr const char* layerALevel = "layerALevel";
        constexpr const char* layerBLevel = "layerBLevel";
        constexpr const char* layerAPan   = "layerAPan";    // balance −1..1
        constexpr const char* layerBPan   = "layerBPan";

        // Warm every indexed-ID static cache on the MESSAGE thread (call from prepareToPlay) so the
        // audio thread never triggers a first-call static initialisation (which would take a one-time
        // guard lock). After this, all indexed-ID lookups on the audio thread are alloc- and lock-free.
        inline void warmIndexedIds()
        {
            for (int i = 1; i <= kNumLFOs; ++i) { lfoOn(i); lfoWave(i); lfoRate(i); lfoDepth(i); lfoTarget(i); lfoSyncDiv(i); lfoGlobal(i); }
            for (int i = 1; i <= 3; ++i)        { oscOn(i); oscWave(i); oscFreq(i); oscAmp(i); oscUniVoices(i); oscUniDetune(i); oscFeedback(i); oscPan(i); }
            // (The STEP SEQ cells are no parameters since 18.5 — nothing to warm for them.)
            for (int i = 1; i <= 4; ++i)        { percNote(i); percLevel(i); percPan(i); }
            // (The PERC grid cells are no parameters since 18.5 — nothing to warm for them.)
            for (int n = 1; n <= ModMatrixConfig::kNumSlots; ++n)
                { modSlotSource(n); modSlotModule(n); modSlotParam(n); modSlotAmount(n); modSlotTargetLegacy(n); modSlotQuant(n); }
        }
    }

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        // ALL modules are spec-driven now: every APVTS parameter comes from a ModuleSpec
        // (Source/Modules/*Specs.h, gathered by Modules::all()), one parameter group per module.
        // See docs/MODULE_SYSTEM.md. The DSP wiring in applyToVoice + PresetIO still read the
        // ID:: strings (unchanged ids).
        return Modules::createParameterLayout();
    }

    // Story 21.4 (S0 of AD-16): the patch's parameters, resolved ONCE to their atomics instead of
    // looked up by string per voice per block (~1,700 hash-map lookups per block before). `prefix`
    // is empty for layer A (today's ids); layer B will resolve the same list with "b" (AD-16 rule 3).
    // Reading `*p.x` is exactly what `*apvts.getRawParameterValue(ID::x)` returned - the same atomic.
    // The lists hold what applyToVoice and the processor's per-patch block code read; instrument
    // modules (ARP, STEP SEQ, PERC, BASS, CHAOS, master bus) are not part of a patch.
    #define JASS_PATCH_SCALAR_PARAMS(X) \
        X(modMatrixOn) X(mixMode) X(mixSrcA) X(mixSrcB) X(adsrOn) X(mixModeOn) \
        X(pitchEnvOn) X(pitchEnvAmount) X(pitchEnvTime) X(attack) X(decay) X(sustain) \
        X(release) X(filterOn) X(filterType) X(filterCutoff) X(filterReso) X(filterTrack) \
        X(distortionOn) X(distortionType) X(distortionDrive) X(distortionMix) X(wavefoldOn) X(wavefoldDrive) \
        X(wavefoldSymmetry) X(wavefoldMix) X(bitcrushOn) X(bitcrushBits) X(bitcrushRate) X(bitcrushMix) \
        X(phaserOn) X(phaserType) X(phaserRate) X(phaserDepth) X(phaserFeedback) X(phaserMix) \
        X(delayOn) X(delayFeedback) X(delayMix) X(chorusOn) X(chorusRate) X(chorusDepth) \
        X(chorusMix) X(reverbOn) X(reverbRoom) X(reverbDamp) X(reverbMix) X(formantOn) \
        X(formantVowel) X(formantReso) X(formantMix) X(outputMode) X(subPan) X(noisePan) \
        X(karplusPan) X(wavetablePan) X(samplerPan) X(samplerSet) X(grainPan) X(noiseOn) \
        X(noiseType) X(noiseAmp) X(karplusOn) X(karplusFreq) X(karplusAmp) X(karplusDamping) \
        X(karplusStretch) X(samplerOn) X(samplerRoot) X(samplerStart) X(samplerEnd) X(samplerMode) \
        X(samplerLevel) X(samplerSpeed) X(samplerStretch) X(samplerRelease) X(grainOn) X(grainPos) \
        X(grainSpray) X(grainSize) X(grainDensity) X(grainPitch) X(grainQuant) X(grainAmp) \
        X(grainKey) X(wavetableOn) X(wavetableBank) X(wavetablePosition) X(wavetableFreq) X(wavetableAmp) \
        X(wavetableUniVoices) X(wavetableUniDetune) X(wavetableFeedback) X(subOn) X(subWave) X(subLevel) \
        X(subFeedback) X(subOctave) X(delaySyncDiv) X(delayTime) X(glideOn) X(glideTime) \
        X(glideMode) \

    #define JASS_PATCH_INDEXED_PARAMS(X) \
        X(oscOn, 3) X(oscWave, 3) X(oscFreq, 3) \
        X(oscAmp, 3) X(oscUniVoices, 3) X(oscUniDetune, 3) \
        X(oscFeedback, 3) X(oscPan, 3) X(lfoDepth, kNumLFOs) \
        X(lfoOn, kNumLFOs) X(lfoTarget, kNumLFOs) X(lfoWave, kNumLFOs) \
        X(lfoRate, kNumLFOs) X(lfoSyncDiv, kNumLFOs) X(lfoGlobal, kNumLFOs) \
        X(modSlotSource, ModMatrixConfig::kNumSlots) X(modSlotModule, ModMatrixConfig::kNumSlots) X(modSlotParam, ModMatrixConfig::kNumSlots) \
        X(modSlotAmount, ModMatrixConfig::kNumSlots) X(modSlotQuant, ModMatrixConfig::kNumSlots) \


    struct PatchParams
    {
        #define JASS_DECL_S(n)    std::atomic<float>* n = nullptr;
        #define JASS_DECL_I(n, c) std::array<std::atomic<float>*, (size_t) (c)> n {};
        JASS_PATCH_SCALAR_PARAMS (JASS_DECL_S)
        JASS_PATCH_INDEXED_PARAMS (JASS_DECL_I)
        #undef JASS_DECL_S
        #undef JASS_DECL_I

        // Message thread, once (processor constructor). A missing id is a startup assert, not a
        // silent zero on the audio thread.
        void resolve (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix)
        {
            auto get = [&] (const juce::String& id)
            {
                // AD-16: only patch-module params exist per layer. An instrument param the patch
                // reads (STEREO's outputMode) is the same atomic for both layers.
                const bool perLayer = Modules::isLayeredParamId (id);
                auto* a = apvts.getRawParameterValue (perLayer ? prefix + id : id);
                jassert (a != nullptr);
                return a;
            };
            #define JASS_RES_S(n)    n = get (ID::n);
            #define JASS_RES_I(n, c) for (int k = 0; k < (int) (c); ++k) n[(size_t) k] = get (ID::n (k + 1));
            JASS_PATCH_SCALAR_PARAMS (JASS_RES_S)
            JASS_PATCH_INDEXED_PARAMS (JASS_RES_I)
            #undef JASS_RES_S
            #undef JASS_RES_I
        }
    };

    // Apply all parameters to a voice
    inline void applyToVoice(const PatchParams& p,   // 21.4: resolved once, no string lookups
                              Oscillator* oscillators, AdsrEnvelope& env,
                              std::array<ChannelStrip, kMaxOutChannels>& strips,   // Epic 10: per-channel FX
                              LFO* lfos, NoiseGenerator& noise,
                              KarplusStrong& karplus, WavetableOscillator& wavetable,
                              SamplePlayer& sampler, double samplerLoopFrac,   // Story 12.1 (+ shared loop clock)
                              GrainEngine& grain,                              // Story 17.1
                              MixMode& mixMode, Oscillator& subOsc, int& subOctave,
                              bool& adsrOn, bool& mixModeOn, int& mixSrcA, int& mixSrcB,
                              PitchEnvelope& pitchEnv, double& pitchEnvAmount, bool& pitchEnvOn,
                              ModSlot* modSlots, bool& modMatrixOn,
                              int& outputModeOut, float* generatorPanOut,   // Epic 10: output mode + 7 pans
                              const double* lfoRateHz, double delayTimeSec,
                              double& filterKeytrackOut)   // filter keytracking amount 0..1
    {
        // Modulation matrix (Story 8.1): read the master enable + N slots into the voice.
        modMatrixOn = *p.modMatrixOn > 0.5f;
        for (int n = 0; n < ModMatrixConfig::kNumSlots; ++n)
        {
            const int mod = (int) *p.modSlotModule[n];
            const int par = (int) *p.modSlotParam[n];
            modSlots[n].source   = (int) *p.modSlotSource[n];
            modSlots[n].target   = (int) ModDest::targetOf(mod, par);   // (module,param) → LFOTarget
            modSlots[n].oscIndex = ModDest::oscIndexOf(mod);            // 0..2 per-OSC; -1 global
            modSlots[n].amount   = *p.modSlotAmount[n];
            modSlots[n].quant    = (int) *p.modSlotQuant[n];   // scale mask (0 = Off)
        }

        mixMode = static_cast<MixMode>(static_cast<int>(*p.mixMode));
        mixSrcA = static_cast<int>(*p.mixSrcA);   // Epic 5
        mixSrcB = static_cast<int>(*p.mixSrcB);
        // Behavioural gates (Story 2.4): ADSR off => bypass envelope; Mix-Mode off => additive.
        adsrOn    = *p.adsrOn    > 0.5f;
        mixModeOn = *p.mixModeOn > 0.5f;

        // Pitch envelope (one-shot pitch sweep). Amount/on are read by the voice per sample;
        // the decay time lives on the envelope object.
        pitchEnvOn     = *p.pitchEnvOn > 0.5f;
        pitchEnvAmount = *p.pitchEnvAmount;
        pitchEnv.setDecay(*p.pitchEnvTime);

        for (int o = 0; o < 3; ++o)
        {
            oscillators[o].setEnabled(*p.oscOn[o] > 0.5f);
            oscillators[o].setWaveform(static_cast<WaveformType>(
                static_cast<int>(*p.oscWave[o])));
            oscillators[o].setFrequency(*p.oscFreq[o]);
            oscillators[o].setAmplitude(*p.oscAmp[o]);
            oscillators[o].setUnisonCount(static_cast<int>(*p.oscUniVoices[o]));
            // Detune param is 0..1 (=±1 semitone); oscillator works in cents.
            oscillators[o].setDetuneAmount(*p.oscUniDetune[o] * 100.0);
            oscillators[o].setFeedback(*p.oscFeedback[o]);   // Self-FM
        }

        env.setAttack(*p.attack);
        env.setDecay(*p.decay);
        env.setSustain(*p.sustain);
        env.setRelease(*p.release);

        // Effect base config → set on EVERY channel strip identically (Epic 10). Local aliases keep the
        // existing lines verbatim; strip 0 is the mono / Pseudo-Stereo chain, strips 1.. the pan channels.
        for (auto& strip : strips)
        {
            auto& filter = strip.filter; auto& distortion = strip.distortion; auto& wavefolder = strip.wavefolder;
            auto& bitcrusher = strip.bitcrusher; auto& phaser = strip.phaser; auto& delay = strip.delay;
            auto& chorus = strip.chorus; auto& reverb = strip.reverb; auto& formant = strip.formant;

        // The choice params no longer carry an "Off" entry — a separate <x>On bool gates
        // them. When on, the choice index maps to the enum +1 (the enum keeps its Off=0).
        const bool filterOn = *p.filterOn > 0.5f;
        filter.setType(filterOn ? static_cast<FilterType>(static_cast<int>(*p.filterType) + 1)
                                 : FilterType::Off);
        filter.setCutoff(*p.filterCutoff);
        filter.setResonance(*p.filterReso);
        // Keytracking amount 0..1 (the voice applies it to baseCutoff with the played ratio, so
        // the note is factored in per block on top of this fixed cutoff, LFO/matrix and accent).
        filterKeytrackOut = *p.filterTrack / 100.0;

        const bool distortionOn = *p.distortionOn > 0.5f;
        distortion.type  = distortionOn ? static_cast<DistortionType>(static_cast<int>(*p.distortionType) + 1)
                                        : DistortionType::Off;
        distortion.drive = *p.distortionDrive;
        distortion.mix   = *p.distortionMix;

        wavefolder.enabled  = *p.wavefoldOn > 0.5f;
        wavefolder.drive    = *p.wavefoldDrive;
        wavefolder.symmetry = *p.wavefoldSymmetry;
        wavefolder.mix      = *p.wavefoldMix;

        bitcrusher.enabled = *p.bitcrushOn > 0.5f;
        bitcrusher.bits    = *p.bitcrushBits;
        bitcrusher.rate    = *p.bitcrushRate;
        bitcrusher.mix     = *p.bitcrushMix;

        phaser.enabled  = *p.phaserOn > 0.5f;
        phaser.type     = static_cast<PhaserType>(static_cast<int>(*p.phaserType));
        phaser.rate     = *p.phaserRate;
        phaser.depth    = *p.phaserDepth;
        phaser.feedback = *p.phaserFeedback;
        phaser.mix      = *p.phaserMix;

        delay.enabled  = *p.delayOn > 0.5f;
        delay.time     = delayTimeSec;   // Tempo-Sync: resolved in processBlock (Free => raw knob, else BPM division)
        delay.feedback = *p.delayFeedback;
        delay.mix      = *p.delayMix;

        chorus.enabled = *p.chorusOn > 0.5f;
        chorus.rate    = *p.chorusRate;
        chorus.depth   = *p.chorusDepth;
        chorus.mix     = *p.chorusMix;

        reverb.enabled  = *p.reverbOn > 0.5f;
        reverb.roomSize = *p.reverbRoom;
        reverb.damping  = *p.reverbDamp;
        reverb.mix      = *p.reverbMix;

        formant.enabled   = *p.formantOn > 0.5f;
        formant.vowel     = *p.formantVowel;
        formant.resonance = *p.formantReso;
        formant.mix       = *p.formantMix;
        }   // end per-strip effect config (Epic 10)

        // Spatialization (Epic 10): output mode + the 7 generator pans → the voice (used at the top of
        // renderNextBlock to build per-channel gains). Append-only; missing ⇒ Pseudo-Stereo / center.
        outputModeOut = (int) *p.outputMode;
        generatorPanOut[PanOsc1]      = *p.oscPan[0];
        generatorPanOut[PanOsc2]      = *p.oscPan[1];
        generatorPanOut[PanOsc3]      = *p.oscPan[2];
        generatorPanOut[PanSub]       = *p.subPan;
        generatorPanOut[PanNoise]     = *p.noisePan;
        generatorPanOut[PanKarplus]   = *p.karplusPan;
        generatorPanOut[PanWavetable] = *p.wavetablePan;
        // SAMPLER (12.1): a stereo set spreads its L/R sub-sources around the PAN knob (the whole
        // stereo image moves with the knob); a mono set sits at the plain pan on slot L.
        // Spread per mode (Störton fix 2026-08-04, MEASURED — scratchpad stoerton3.py): in the
        // gain-matrix STEREO-PAN mode a ±0.5 spread leaks 38% of the OTHER mic channel into each
        // ear (equal-power crossbleed) — the coherent partial sum comb-filters the recording,
        // key-dependently (Splendid A3: ±5–6 dB on the loudest partials = the user's "metallic"
        // tone; control key C4 almost clean — matched the ear exactly). A stereo RECORDING in a
        // gain-based stereo mode must render like a stereo TRACK: hard L/R, PAN = balance ⇒
        // spread 1.0. The binaural/Kunstkopf modes keep ±0.5: their sub-sources are decorrelated
        // by ITD/HRIR before summing (no coherent comb), and the ±0.5 image is the 12.1 choice.
        {
            const float sPan   = *p.samplerPan;
            const auto* set    = SampleBankStore::instance().getSet(
                                     static_cast<int>(*p.samplerSet));
            const auto  mode   = static_cast<OutputMode>(outputModeOut);
            const bool  hrir   = (mode == OutputMode::Binaural || mode == OutputMode::Kunstkopf);
            const float spread = (set != nullptr && set->isStereo()) ? (hrir ? 0.5f : 1.0f) : 0.0f;
            generatorPanOut[PanSamplerL] = juce::jlimit(-1.0f, 1.0f, sPan - spread);
            generatorPanOut[PanSamplerR] = juce::jlimit(-1.0f, 1.0f, sPan + spread);
            // GRAIN (17.1) reads the same set, so the same stereo rule applies to its L/R pair.
            const float gPan = *p.grainPan;
            generatorPanOut[PanGrainL] = juce::jlimit(-1.0f, 1.0f, gPan - spread);
            generatorPanOut[PanGrainR] = juce::jlimit(-1.0f, 1.0f, gPan + spread);
        }

        // LFOs — one loop for all (Tempo-Sync resolved per LFO in processBlock => lfoRateHz[i]).
        for (int i = 0; i < kNumLFOs; ++i)
        {
            lfos[i].setWaveform(static_cast<LFOWaveform>(static_cast<int>(*p.lfoWave[i])));
            lfos[i].setRate(lfoRateHz[i]);
            lfos[i].setDepth(*p.lfoDepth[i]);
            const bool on = *p.lfoOn[i] > 0.5f;
            lfos[i].setTarget(on ? static_cast<LFOTarget>(static_cast<int>(*p.lfoTarget[i]) + 1)
                                 : LFOTarget::Off);
        }

        const bool noiseOn = *p.noiseOn > 0.5f;
        noise.setType(noiseOn ? static_cast<NoiseType>(static_cast<int>(*p.noiseType) + 1)
                              : NoiseType::Off);
        noise.setAmplitude(*p.noiseAmp);

        karplus.setEnabled(*p.karplusOn > 0.5f);
        karplus.setFrequency(*p.karplusFreq);
        karplus.setAmplitude(*p.karplusAmp);
        karplus.setDamping(*p.karplusDamping);
        karplus.setStretch(*p.karplusStretch);

        // Sampler (Story 12.1): source set + playback params. The set pointer stays valid for the
        // whole block (never-freed store, same contract as the wavetable bank below).
        sampler.setEnabled(*p.samplerOn > 0.5f);
        sampler.setSource(SampleBankStore::instance().getSet(
            static_cast<int>(*p.samplerSet)));
        sampler.setRootKey(static_cast<int>(*p.samplerRoot));
        sampler.setRegion(*p.samplerStart,
                          *p.samplerEnd);
        sampler.setMode(static_cast<SamplePlayer::Mode>(
            static_cast<int>(*p.samplerMode)));
        sampler.setLevel(*p.samplerLevel);
        sampler.setSpeed(*p.samplerSpeed);
        sampler.setStretchMode(*p.samplerStretch > 0.5f);   // 12.3
        sampler.setReleaseFallback(*p.samplerRelease);      // 12.4
        sampler.setLoopSyncPhase(samplerLoopFrac);

        // GRAIN (Story 17.1): the cloud's per-block parameters. Its material (zone + pitch factor)
        // is set at note-on by the voice from the sampler's zone pick — nothing to do here.
        grain.setEnabled(*p.grainOn > 0.5f);
        grain.setPosition(*p.grainPos);
        grain.setSpray(*p.grainSpray);
        grain.setSizeMs(*p.grainSize);
        grain.setDensity(*p.grainDensity);
        grain.setPitchSpread(*p.grainPitch);
        grain.setQuant(static_cast<int>(*p.grainQuant));
        grain.setLevel(*p.grainAmp);
        grain.setKeyMode(*p.grainKey > 0.5f);   // 17.2

        wavetable.setEnabled(*p.wavetableOn > 0.5f);
        wavetable.setBank(WavetableBankStore::instance().getBank(
            static_cast<int>(*p.wavetableBank)));
        wavetable.setPosition(*p.wavetablePosition);
        wavetable.setFrequency(*p.wavetableFreq);
        wavetable.setAmplitude(*p.wavetableAmp);
        wavetable.setUnisonCount(static_cast<int>(*p.wavetableUniVoices));
        wavetable.setDetuneAmount(*p.wavetableUniDetune * 100.0);
        wavetable.setFeedback(*p.wavetableFeedback);

        subOsc.setEnabled(*p.subOn > 0.5f);
        subOsc.setWaveform(static_cast<int>(*p.subWave) == 0
                               ? WaveformType::Sine : WaveformType::Square);
        subOsc.setAmplitude(*p.subLevel);
        subOsc.setFeedback(*p.subFeedback);
        // Choice 0 -> -1 octave, 1 -> -2 octaves
        subOctave = -(static_cast<int>(*p.subOctave) + 1);
    }
}
