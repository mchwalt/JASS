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
#include "../DSP/SyncDivision.h"
#include "../DSP/ModMatrix.h"
#include "../Modules/ModuleRegistry.h"   // spec-driven modules — generates APVTS params (audio-safe)

namespace Parameters
{
    // Parameter IDs
    namespace ID
    {
        // Oscillators
        inline juce::String oscOn(int i)    { return "osc" + juce::String(i) + "On"; }
        inline juce::String oscWave(int i)  { return "osc" + juce::String(i) + "Wave"; }
        inline juce::String oscFreq(int i)  { return "osc" + juce::String(i) + "Freq"; }
        inline juce::String oscAmp(int i)   { return "osc" + juce::String(i) + "Amp"; }
        inline juce::String oscUniVoices(int i) { return "osc" + juce::String(i) + "UniVoices"; }
        inline juce::String oscUniDetune(int i) { return "osc" + juce::String(i) + "UniDetune"; }
        inline juce::String oscFeedback(int i)  { return "osc" + juce::String(i) + "Feedback"; }  // Self-FM depth (append-only)

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
        inline juce::String lfoOn(int i)      { return "lfo" + juce::String(i) + "On"; }
        inline juce::String lfoWave(int i)    { return "lfo" + juce::String(i) + "Wave"; }
        inline juce::String lfoRate(int i)    { return "lfo" + juce::String(i) + "Rate"; }
        inline juce::String lfoDepth(int i)   { return "lfo" + juce::String(i) + "Depth"; }
        inline juce::String lfoTarget(int i)  { return "lfo" + juce::String(i) + "Target"; }
        inline juce::String lfoSyncDiv(int i) { return "lfo" + juce::String(i) + "SyncDiv"; }

        // Reverb
        constexpr const char* reverbOn   = "reverbOn";
        constexpr const char* reverbRoom = "reverbRoom";
        constexpr const char* reverbDamp = "reverbDamp";
        constexpr const char* reverbMix  = "reverbMix";

        // Noise
        constexpr const char* noiseOn   = "noiseOn";
        constexpr const char* noiseType = "noiseType";
        constexpr const char* noiseAmp  = "noiseAmp";

        // Karplus-Strong
        constexpr const char* karplusOn      = "karplusOn";
        constexpr const char* karplusFreq    = "karplusFreq";
        constexpr const char* karplusAmp     = "karplusAmp";
        constexpr const char* karplusDamping = "karplusDamping";
        constexpr const char* karplusStretch = "karplusStretch";

        // Wavetable
        constexpr const char* wavetableOn        = "wavetableOn";
        constexpr const char* wavetableBank      = "wavetableBank";
        constexpr const char* wavetablePosition  = "wavetablePosition";
        constexpr const char* wavetableFreq      = "wavetableFreq";
        constexpr const char* wavetableAmp       = "wavetableAmp";
        constexpr const char* wavetableUniVoices = "wavetableUniVoices";
        constexpr const char* wavetableUniDetune = "wavetableUniDetune";

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

        // Modulation matrix (Story 8.1 / Epic 8). N routing slots, each {Source, Target,
        // Amount}, plus a master enable. Append-only, indexed helpers (mirror oscFreq(i)).
        inline juce::String modSlotSource(int n) { return "modSlot" + juce::String(n) + "Source"; }
        inline juce::String modSlotTarget(int n) { return "modSlot" + juce::String(n) + "Target"; }
        inline juce::String modSlotAmount(int n) { return "modSlot" + juce::String(n) + "Amount"; }
        constexpr const char* modMatrixOn = "modMatrixOn";
    }

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

        // Oscillators (OSC 1-3, octave stack C4/C5/C3) + CROSS MOD — now spec-driven
        // (OscSpecs.h / CrossModSpecs.h via Modules::appendAllParameters).

        // ADSR
        auto timeRange = juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.4f);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::attack,  1), "Attack",  timeRange, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::decay,   1), "Decay",   timeRange, 0.3f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::sustain, 1), "Sustain", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.7f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::release, 1), "Release", timeRange, 1.0f));

        // Spec-driven modules — GENERATED from their ModuleSpecs (see docs/Modul_Architektur_Konzept.md).
        // Adds the APVTS params for every migrated module (currently FILTER). The DSP wiring in
        // applyToVoice + PresetIO still read the ID:: strings (same ids). Remaining modules below
        // are still hand-written and move here one at a time.
        Modules::appendAllParameters(params);

        // Formant, Distortion, Wavefold, Delay, Chorus, Reverb — now spec-driven.

        // LFOs (indexed), OSC 1-3, CROSS MOD, MOD MATRIX, and the pure effect/generator modules
        // are all spec-driven now — see Modules::appendAllParameters() (Source/Modules/*Specs.h).

        // Karplus-Strong
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::karplusOn, 1), "Karplus On", false));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::karplusFreq, 1), "Karplus Freq", juce::NormalisableRange<float>(20.0f, 2000.0f, 1.0f, 0.3f), 261.63f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::karplusAmp, 1), "Karplus Amp", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::karplusDamping, 1), "Karplus Damping", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::karplusStretch, 1), "Karplus Stretch", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

        // Wavetable
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::wavetableOn, 1), "Wavetable On", false));
        params.push_back(std::make_unique<juce::AudioParameterInt>(juce::ParameterID(ID::wavetableBank, 1), "Wavetable Bank", 0, WavetableBankStore::MaxBanks - 1, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::wavetablePosition, 1), "Wavetable Position", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::wavetableFreq, 1), "Wavetable Freq", juce::NormalisableRange<float>(20.0f, 10000.0f, 1.0f, 0.3f), 261.63f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::wavetableAmp, 1), "Wavetable Amp", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterInt>(juce::ParameterID(ID::wavetableUniVoices, 1), "Wavetable Uni Voices", 1, 7, 1));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::wavetableUniDetune, 1), "Wavetable Uni Detune", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.2f));

        // Bitcrush, Phaser, Arpeggiator, Glide — now spec-driven.

        // Module enables for the formerly always-on modules (Story 2.4). Default TRUE
        // (on) so behaviour is unchanged until the user toggles; append-only.
        // (masterOn is now spec-driven via MasterSpecs.h.)
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::adsrOn,    1), "Envelope On", true));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::scopeOn,    1), "Scope On",    true));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::spectrumOn, 1), "Spectrum On", true));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::keyboardOn, 1), "Keyboard On", true));

        // Pitch envelope + MOD MATRIX — now spec-driven.

        return { params.begin(), params.end() };
    }

    // Apply all parameters to a voice
    inline void applyToVoice(juce::AudioProcessorValueTreeState& apvts,
                              Oscillator* oscillators, AdsrEnvelope& env,
                              BiquadFilter& filter, DistortionEffect& distortion,
                              WavefolderEffect& wavefolder, BitcrusherEffect& bitcrusher,
                              PhaserEffect& phaser,
                              DelayEffect& delay,
                              ChorusEffect& chorus, ReverbEffect& reverb,
                              FormantFilter& formant,
                              LFO* lfos, NoiseGenerator& noise,
                              KarplusStrong& karplus, WavetableOscillator& wavetable,
                              MixMode& mixMode, Oscillator& subOsc, int& subOctave,
                              bool& adsrOn, bool& mixModeOn, int& mixSrcA, int& mixSrcB,
                              PitchEnvelope& pitchEnv, double& pitchEnvAmount, bool& pitchEnvOn,
                              ModSlot* modSlots, bool& modMatrixOn,
                              const double* lfoRateHz, double delayTimeSec)
    {
        // Modulation matrix (Story 8.1): read the master enable + N slots into the voice.
        modMatrixOn = *apvts.getRawParameterValue(ID::modMatrixOn) > 0.5f;
        for (int n = 0; n < ModMatrixConfig::kNumSlots; ++n)
        {
            modSlots[n].source = (int) *apvts.getRawParameterValue(ID::modSlotSource(n + 1));
            modSlots[n].target = (int) *apvts.getRawParameterValue(ID::modSlotTarget(n + 1));
            modSlots[n].amount = *apvts.getRawParameterValue(ID::modSlotAmount(n + 1));
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
