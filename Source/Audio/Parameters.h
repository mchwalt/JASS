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

        // STEP SEQ (Story 15.1) — an authored 16-step figure. Per-step params are indexed like the
        // oscillators/LFOs so the audio thread never builds a String (RT-safety, Story 11.1).
        constexpr const char* seqOn     = "seqOn";
        constexpr const char* seqSync   = "seqSync";
        constexpr const char* seqRate   = "seqRate";
        constexpr const char* seqLength = "seqLength";
        constexpr const char* seqGate   = "seqGate";     // ONE note length for the whole pattern
        JASS_INDEXED_ID (seqPitch, 32, "seqPitch", "")   // max index = StepSequencer::kMaxSteps
        JASS_INDEXED_ID (seqStep,  32, "seqStep",  "")   // per-step on/off (off = rest)

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
        JASS_INDEXED_ID (percStep1, 32, "percStep1_", "")
        JASS_INDEXED_ID (percStep2, 32, "percStep2_", "")
        JASS_INDEXED_ID (percStep3, 32, "percStep3_", "")
        JASS_INDEXED_ID (percStep4, 32, "percStep4_", "")
        // Lane (1..4) + step (1..32) -> id. Audio thread safe: pure array lookups, no String built.
        inline const juce::String& percStep (int lane, int step)
        {
            switch (juce::jlimit (1, 4, lane))
            {
                case 1:  return percStep1 (step);
                case 2:  return percStep2 (step);
                case 3:  return percStep3 (step);
                default: return percStep4 (step);
            }
        }

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
        constexpr const char* modMatrixOn = "modMatrixOn";

        #undef JASS_INDEXED_ID

        // Warm every indexed-ID static cache on the MESSAGE thread (call from prepareToPlay) so the
        // audio thread never triggers a first-call static initialisation (which would take a one-time
        // guard lock). After this, all indexed-ID lookups on the audio thread are alloc- and lock-free.
        inline void warmIndexedIds()
        {
            for (int i = 1; i <= kNumLFOs; ++i) { lfoOn(i); lfoWave(i); lfoRate(i); lfoDepth(i); lfoTarget(i); lfoSyncDiv(i); }
            for (int i = 1; i <= 3; ++i)        { oscOn(i); oscWave(i); oscFreq(i); oscAmp(i); oscUniVoices(i); oscUniDetune(i); oscFeedback(i); oscPan(i); }
            for (int i = 1; i <= 32; ++i)       { seqPitch(i); seqStep(i); }
            for (int i = 1; i <= 4; ++i)        { percNote(i); percLevel(i); percPan(i); }
            for (int i = 1; i <= 32; ++i)       { percStep1(i); percStep2(i); percStep3(i); percStep4(i); }
            for (int n = 1; n <= ModMatrixConfig::kNumSlots; ++n)
                { modSlotSource(n); modSlotModule(n); modSlotParam(n); modSlotAmount(n); modSlotTargetLegacy(n); }
        }
    }

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

        // ALL modules are spec-driven now: every APVTS parameter comes from a ModuleSpec
        // (Source/Modules/*Specs.h, gathered by Modules::all()). See docs/MODULE_SYSTEM.md.
        // The DSP wiring in applyToVoice + PresetIO still read the ID:: strings (unchanged ids).
        Modules::appendAllParameters(params);

        return { params.begin(), params.end() };
    }

    // Apply all parameters to a voice
    inline void applyToVoice(juce::AudioProcessorValueTreeState& apvts,
                              Oscillator* oscillators, AdsrEnvelope& env,
                              std::array<ChannelStrip, kMaxOutChannels>& strips,   // Epic 10: per-channel FX
                              LFO* lfos, NoiseGenerator& noise,
                              KarplusStrong& karplus, WavetableOscillator& wavetable,
                              SamplePlayer& sampler, double samplerLoopFrac,   // Story 12.1 (+ shared loop clock)
                              MixMode& mixMode, Oscillator& subOsc, int& subOctave,
                              bool& adsrOn, bool& mixModeOn, int& mixSrcA, int& mixSrcB,
                              PitchEnvelope& pitchEnv, double& pitchEnvAmount, bool& pitchEnvOn,
                              ModSlot* modSlots, bool& modMatrixOn,
                              int& outputModeOut, float* generatorPanOut,   // Epic 10: output mode + 7 pans
                              const double* lfoRateHz, double delayTimeSec)
    {
        // Modulation matrix (Story 8.1): read the master enable + N slots into the voice.
        modMatrixOn = *apvts.getRawParameterValue(ID::modMatrixOn) > 0.5f;
        for (int n = 0; n < ModMatrixConfig::kNumSlots; ++n)
        {
            const int mod = (int) *apvts.getRawParameterValue(ID::modSlotModule(n + 1));
            const int par = (int) *apvts.getRawParameterValue(ID::modSlotParam (n + 1));
            modSlots[n].source   = (int) *apvts.getRawParameterValue(ID::modSlotSource(n + 1));
            modSlots[n].target   = (int) ModDest::targetOf(mod, par);   // (module,param) → LFOTarget
            modSlots[n].oscIndex = ModDest::oscIndexOf(mod);            // 0..2 per-OSC; -1 global
            modSlots[n].amount   = *apvts.getRawParameterValue(ID::modSlotAmount(n + 1));
        }

        mixMode = static_cast<MixMode>(static_cast<int>(*apvts.getRawParameterValue(ID::mixMode)));
        mixSrcA = static_cast<int>(*apvts.getRawParameterValue(ID::mixSrcA));   // Epic 5
        mixSrcB = static_cast<int>(*apvts.getRawParameterValue(ID::mixSrcB));
        // Behavioural gates (Story 2.4): ADSR off => bypass envelope; Mix-Mode off => additive.
        adsrOn    = *apvts.getRawParameterValue(ID::adsrOn)    > 0.5f;
        mixModeOn = *apvts.getRawParameterValue(ID::mixModeOn) > 0.5f;

        // Pitch envelope (one-shot pitch sweep). Amount/on are read by the voice per sample;
        // the decay time lives on the envelope object.
        pitchEnvOn     = *apvts.getRawParameterValue(ID::pitchEnvOn) > 0.5f;
        pitchEnvAmount = *apvts.getRawParameterValue(ID::pitchEnvAmount);
        pitchEnv.setDecay(*apvts.getRawParameterValue(ID::pitchEnvTime));

        for (int o = 0; o < 3; ++o)
        {
            oscillators[o].setEnabled(*apvts.getRawParameterValue(ID::oscOn(o + 1)) > 0.5f);
            oscillators[o].setWaveform(static_cast<WaveformType>(
                static_cast<int>(*apvts.getRawParameterValue(ID::oscWave(o + 1)))));
            oscillators[o].setFrequency(*apvts.getRawParameterValue(ID::oscFreq(o + 1)));
            oscillators[o].setAmplitude(*apvts.getRawParameterValue(ID::oscAmp(o + 1)));
            oscillators[o].setUnisonCount(static_cast<int>(*apvts.getRawParameterValue(ID::oscUniVoices(o + 1))));
            // Detune param is 0..1 (=±1 semitone); oscillator works in cents.
            oscillators[o].setDetuneAmount(*apvts.getRawParameterValue(ID::oscUniDetune(o + 1)) * 100.0);
            oscillators[o].setFeedback(*apvts.getRawParameterValue(ID::oscFeedback(o + 1)));   // Self-FM
        }

        env.setAttack(*apvts.getRawParameterValue(ID::attack));
        env.setDecay(*apvts.getRawParameterValue(ID::decay));
        env.setSustain(*apvts.getRawParameterValue(ID::sustain));
        env.setRelease(*apvts.getRawParameterValue(ID::release));

        // Effect base config → set on EVERY channel strip identically (Epic 10). Local aliases keep the
        // existing lines verbatim; strip 0 is the mono / Pseudo-Stereo chain, strips 1.. the pan channels.
        for (auto& strip : strips)
        {
            auto& filter = strip.filter; auto& distortion = strip.distortion; auto& wavefolder = strip.wavefolder;
            auto& bitcrusher = strip.bitcrusher; auto& phaser = strip.phaser; auto& delay = strip.delay;
            auto& chorus = strip.chorus; auto& reverb = strip.reverb; auto& formant = strip.formant;

        // The choice params no longer carry an "Off" entry — a separate <x>On bool gates
        // them. When on, the choice index maps to the enum +1 (the enum keeps its Off=0).
        const bool filterOn = *apvts.getRawParameterValue(ID::filterOn) > 0.5f;
        filter.setType(filterOn ? static_cast<FilterType>(static_cast<int>(*apvts.getRawParameterValue(ID::filterType)) + 1)
                                 : FilterType::Off);
        filter.setCutoff(*apvts.getRawParameterValue(ID::filterCutoff));
        filter.setResonance(*apvts.getRawParameterValue(ID::filterReso));

        const bool distortionOn = *apvts.getRawParameterValue(ID::distortionOn) > 0.5f;
        distortion.type  = distortionOn ? static_cast<DistortionType>(static_cast<int>(*apvts.getRawParameterValue(ID::distortionType)) + 1)
                                        : DistortionType::Off;
        distortion.drive = *apvts.getRawParameterValue(ID::distortionDrive);
        distortion.mix   = *apvts.getRawParameterValue(ID::distortionMix);

        wavefolder.enabled  = *apvts.getRawParameterValue(ID::wavefoldOn) > 0.5f;
        wavefolder.drive    = *apvts.getRawParameterValue(ID::wavefoldDrive);
        wavefolder.symmetry = *apvts.getRawParameterValue(ID::wavefoldSymmetry);
        wavefolder.mix      = *apvts.getRawParameterValue(ID::wavefoldMix);

        bitcrusher.enabled = *apvts.getRawParameterValue(ID::bitcrushOn) > 0.5f;
        bitcrusher.bits    = *apvts.getRawParameterValue(ID::bitcrushBits);
        bitcrusher.rate    = *apvts.getRawParameterValue(ID::bitcrushRate);
        bitcrusher.mix     = *apvts.getRawParameterValue(ID::bitcrushMix);

        phaser.enabled  = *apvts.getRawParameterValue(ID::phaserOn) > 0.5f;
        phaser.type     = static_cast<PhaserType>(static_cast<int>(*apvts.getRawParameterValue(ID::phaserType)));
        phaser.rate     = *apvts.getRawParameterValue(ID::phaserRate);
        phaser.depth    = *apvts.getRawParameterValue(ID::phaserDepth);
        phaser.feedback = *apvts.getRawParameterValue(ID::phaserFeedback);
        phaser.mix      = *apvts.getRawParameterValue(ID::phaserMix);

        delay.enabled  = *apvts.getRawParameterValue(ID::delayOn) > 0.5f;
        delay.time     = delayTimeSec;   // Tempo-Sync: resolved in processBlock (Free => raw knob, else BPM division)
        delay.feedback = *apvts.getRawParameterValue(ID::delayFeedback);
        delay.mix      = *apvts.getRawParameterValue(ID::delayMix);

        chorus.enabled = *apvts.getRawParameterValue(ID::chorusOn) > 0.5f;
        chorus.rate    = *apvts.getRawParameterValue(ID::chorusRate);
        chorus.depth   = *apvts.getRawParameterValue(ID::chorusDepth);
        chorus.mix     = *apvts.getRawParameterValue(ID::chorusMix);

        reverb.enabled  = *apvts.getRawParameterValue(ID::reverbOn) > 0.5f;
        reverb.roomSize = *apvts.getRawParameterValue(ID::reverbRoom);
        reverb.damping  = *apvts.getRawParameterValue(ID::reverbDamp);
        reverb.mix      = *apvts.getRawParameterValue(ID::reverbMix);

        formant.enabled   = *apvts.getRawParameterValue(ID::formantOn) > 0.5f;
        formant.vowel     = *apvts.getRawParameterValue(ID::formantVowel);
        formant.resonance = *apvts.getRawParameterValue(ID::formantReso);
        formant.mix       = *apvts.getRawParameterValue(ID::formantMix);
        }   // end per-strip effect config (Epic 10)

        // Spatialization (Epic 10): output mode + the 7 generator pans → the voice (used at the top of
        // renderNextBlock to build per-channel gains). Append-only; missing ⇒ Pseudo-Stereo / center.
        outputModeOut = (int) *apvts.getRawParameterValue(ID::outputMode);
        generatorPanOut[PanOsc1]      = *apvts.getRawParameterValue(ID::oscPan(1));
        generatorPanOut[PanOsc2]      = *apvts.getRawParameterValue(ID::oscPan(2));
        generatorPanOut[PanOsc3]      = *apvts.getRawParameterValue(ID::oscPan(3));
        generatorPanOut[PanSub]       = *apvts.getRawParameterValue(ID::subPan);
        generatorPanOut[PanNoise]     = *apvts.getRawParameterValue(ID::noisePan);
        generatorPanOut[PanKarplus]   = *apvts.getRawParameterValue(ID::karplusPan);
        generatorPanOut[PanWavetable] = *apvts.getRawParameterValue(ID::wavetablePan);
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
            const float sPan   = *apvts.getRawParameterValue(ID::samplerPan);
            const auto* set    = SampleBankStore::instance().getSet(
                                     static_cast<int>(*apvts.getRawParameterValue(ID::samplerSet)));
            const auto  mode   = static_cast<OutputMode>(outputModeOut);
            const bool  hrir   = (mode == OutputMode::Binaural || mode == OutputMode::Kunstkopf);
            const float spread = (set != nullptr && set->isStereo()) ? (hrir ? 0.5f : 1.0f) : 0.0f;
            generatorPanOut[PanSamplerL] = juce::jlimit(-1.0f, 1.0f, sPan - spread);
            generatorPanOut[PanSamplerR] = juce::jlimit(-1.0f, 1.0f, sPan + spread);
        }

        // LFOs — one loop for all (Tempo-Sync resolved per LFO in processBlock => lfoRateHz[i]).
        for (int i = 0; i < kNumLFOs; ++i)
        {
            lfos[i].setWaveform(static_cast<LFOWaveform>(static_cast<int>(*apvts.getRawParameterValue(ID::lfoWave(i + 1)))));
            lfos[i].setRate(lfoRateHz[i]);
            lfos[i].setDepth(*apvts.getRawParameterValue(ID::lfoDepth(i + 1)));
            const bool on = *apvts.getRawParameterValue(ID::lfoOn(i + 1)) > 0.5f;
            lfos[i].setTarget(on ? static_cast<LFOTarget>(static_cast<int>(*apvts.getRawParameterValue(ID::lfoTarget(i + 1))) + 1)
                                 : LFOTarget::Off);
        }

        const bool noiseOn = *apvts.getRawParameterValue(ID::noiseOn) > 0.5f;
        noise.setType(noiseOn ? static_cast<NoiseType>(static_cast<int>(*apvts.getRawParameterValue(ID::noiseType)) + 1)
                              : NoiseType::Off);
        noise.setAmplitude(*apvts.getRawParameterValue(ID::noiseAmp));

        karplus.setEnabled(*apvts.getRawParameterValue(ID::karplusOn) > 0.5f);
        karplus.setFrequency(*apvts.getRawParameterValue(ID::karplusFreq));
        karplus.setAmplitude(*apvts.getRawParameterValue(ID::karplusAmp));
        karplus.setDamping(*apvts.getRawParameterValue(ID::karplusDamping));
        karplus.setStretch(*apvts.getRawParameterValue(ID::karplusStretch));

        // Sampler (Story 12.1): source set + playback params. The set pointer stays valid for the
        // whole block (never-freed store, same contract as the wavetable bank below).
        sampler.setEnabled(*apvts.getRawParameterValue(ID::samplerOn) > 0.5f);
        sampler.setSource(SampleBankStore::instance().getSet(
            static_cast<int>(*apvts.getRawParameterValue(ID::samplerSet))));
        sampler.setRootKey(static_cast<int>(*apvts.getRawParameterValue(ID::samplerRoot)));
        sampler.setRegion(*apvts.getRawParameterValue(ID::samplerStart),
                          *apvts.getRawParameterValue(ID::samplerEnd));
        sampler.setMode(static_cast<SamplePlayer::Mode>(
            static_cast<int>(*apvts.getRawParameterValue(ID::samplerMode))));
        sampler.setLevel(*apvts.getRawParameterValue(ID::samplerLevel));
        sampler.setSpeed(*apvts.getRawParameterValue(ID::samplerSpeed));
        sampler.setStretchMode(*apvts.getRawParameterValue(ID::samplerStretch) > 0.5f);   // 12.3
        sampler.setReleaseFallback(*apvts.getRawParameterValue(ID::samplerRelease));      // 12.4
        sampler.setLoopSyncPhase(samplerLoopFrac);

        wavetable.setEnabled(*apvts.getRawParameterValue(ID::wavetableOn) > 0.5f);
        wavetable.setBank(WavetableBankStore::instance().getBank(
            static_cast<int>(*apvts.getRawParameterValue(ID::wavetableBank))));
        wavetable.setPosition(*apvts.getRawParameterValue(ID::wavetablePosition));
        wavetable.setFrequency(*apvts.getRawParameterValue(ID::wavetableFreq));
        wavetable.setAmplitude(*apvts.getRawParameterValue(ID::wavetableAmp));
        wavetable.setUnisonCount(static_cast<int>(*apvts.getRawParameterValue(ID::wavetableUniVoices)));
        wavetable.setDetuneAmount(*apvts.getRawParameterValue(ID::wavetableUniDetune) * 100.0);

        subOsc.setEnabled(*apvts.getRawParameterValue(ID::subOn) > 0.5f);
        subOsc.setWaveform(static_cast<int>(*apvts.getRawParameterValue(ID::subWave)) == 0
                               ? WaveformType::Sine : WaveformType::Square);
        subOsc.setAmplitude(*apvts.getRawParameterValue(ID::subLevel));
        // Choice 0 -> -1 octave, 1 -> -2 octaves
        subOctave = -(static_cast<int>(*apvts.getRawParameterValue(ID::subOctave)) + 1);
    }
}
