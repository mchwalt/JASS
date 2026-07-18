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

        // Oscillators — default tuning is an OCTAVE STACK (not a chord): OSC1
        // at C4 (= the played note, since pitch transposes relative to C4),
        // OSC2 one octave up (brilliance), OSC3 one octave down (body/sub).
        // Octaves (2:1) stay consonant with ANY note or chord the user plays,
        // so all three together sound full but never "off" — unlike the old
        // C-E-G major triad, which forced a major chord onto every key.
        float defaultFreqs[] = { 261.63f, 523.25f, 130.81f };  // C4, C5, C3
        float defaultAmps[]  = { 0.5f, 0.5f, 0.5f };  // all amp knobs default to half

        for (int i = 1; i <= 3; ++i)
        {
            params.push_back(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID(ID::oscOn(i), 1),
                "OSC " + juce::String(i) + " On", false));

            params.push_back(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID(ID::oscWave(i), 1),
                "OSC " + juce::String(i) + " Wave",
                juce::StringArray{"Sine", "Sawtooth", "Square", "Triangle"}, 0));

            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(ID::oscFreq(i), 1),
                "OSC " + juce::String(i) + " Freq",
                juce::NormalisableRange<float>(20.0f, 10000.0f, 1.0f, 0.3f),
                defaultFreqs[i - 1]));

            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(ID::oscAmp(i), 1),
                "OSC " + juce::String(i) + " Amp",
                juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
                defaultAmps[i - 1]));

            // Per-oscillator unison (detune 0..1 = ±1 semitone, matching C#)
            params.push_back(std::make_unique<juce::AudioParameterInt>(
                juce::ParameterID(ID::oscUniVoices(i), 1),
                "OSC " + juce::String(i) + " Uni Voices", 1, 7, 1));
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(ID::oscUniDetune(i), 1),
                "OSC " + juce::String(i) + " Uni Detune",
                juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.2f));

            // Self-FM / feedback (append-only; default 0 = off, so existing
            // presets/users are unchanged until the knob is turned up).
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(ID::oscFeedback(i), 1),
                "OSC " + juce::String(i) + " Feedback",
                juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
        }

        // Mix mode
        // CROSS MOD (Epic 5 / Option B): the mode holds only the real couplings; "Additive"
        // (no coupling) is the module being disabled (mixModeOn=false). PresetIO maps the on-disk
        // "Additive"/"RingMod"/"FM" via choiceOrOff, so old presets still round-trip.
        params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ID::mixMode, 1), "Cross Mod", juce::StringArray{"RingMod", "FM"}, 0));
        // Epic 5: selectable RingMod/FM operands (append-only; defaults OSC1/OSC2 = prior behaviour).
        params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ID::mixSrcA, 1), "Mix Source A", juce::StringArray{"OSC 1", "OSC 2", "OSC 3"}, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ID::mixSrcB, 1), "Mix Source B", juce::StringArray{"OSC 1", "OSC 2", "OSC 3"}, 1));

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

        // Formant / vowel filter (append-only; default off => existing presets unchanged)
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::formantOn, 1), "Formant On", false));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::formantVowel, 1), "Formant Vowel", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::formantReso, 1), "Formant Resonance", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::formantMix, 1), "Formant Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

        // Distortion
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::distortionOn, 1), "Distortion On", false));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ID::distortionType, 1), "Distortion Type", juce::StringArray{"SoftClip", "HardClip", "Foldback"}, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::distortionDrive, 1), "Distortion Drive", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::distortionMix, 1), "Distortion Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

        // Wavefolder
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::wavefoldOn, 1), "Wavefold On", false));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::wavefoldDrive, 1), "Wavefold Drive", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::wavefoldSymmetry, 1), "Wavefold Symmetry", juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::wavefoldMix, 1), "Wavefold Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

        // Delay
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::delayOn, 1), "Delay On", false));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::delayTime, 1), "Delay Time", juce::NormalisableRange<float>(0.01f, 2.0f, 0.01f), 0.3f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::delayFeedback, 1), "Delay Feedback", juce::NormalisableRange<float>(0.0f, 0.95f, 0.01f), 0.4f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::delayMix, 1), "Delay Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f));
        // Tempo-Sync: 0 = Free (use Delay Time knob), else a note division. Append-only; default Free = unchanged.
        params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ID::delaySyncDiv, 1), "Delay Sync", SyncDivision::kNames, 0));

        // Chorus
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::chorusOn, 1), "Chorus On", false));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::chorusRate, 1), "Chorus Rate", juce::NormalisableRange<float>(0.1f, 5.0f, 0.01f), 1.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::chorusDepth, 1), "Chorus Depth", juce::NormalisableRange<float>(0.001f, 0.02f, 0.001f), 0.005f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::chorusMix, 1), "Chorus Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

        // Reverb
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::reverbOn, 1), "Reverb On", false));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::reverbRoom, 1), "Reverb Room", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.7f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::reverbDamp, 1), "Reverb Damping", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::reverbMix, 1), "Reverb Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f));

        // LFOs — indexed like the oscillators (one loop for all kNumLFOs). SYNC 0 = Free (use RATE
        // knob), else a note division. All default off/Free so an unused LFO is silent.
        for (int i = 1; i <= kNumLFOs; ++i)
        {
            const juce::String pre = "LFO " + juce::String(i) + " ";
            params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ID::lfoWave(i), 1), pre + "Wave", juce::StringArray{"Sine", "Triangle", "Square", "Sawtooth"}, 0));
            params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::lfoRate(i), 1), pre + "Rate", juce::NormalisableRange<float>(0.1f, 20.0f, 0.1f), 2.0f));
            params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::lfoDepth(i), 1), pre + "Depth", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
            params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::lfoOn(i), 1), pre + "On", false));
            params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ID::lfoTarget(i), 1), pre + "Target", juce::StringArray{"Frequency", "Amplitude", "Filter Cutoff", "Wavetable Pos", "Formant Vowel", "Filter Reso", "Wavefold Drive"}, 0));
            params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ID::lfoSyncDiv(i), 1), pre + "Sync", SyncDivision::kNames, 0));
        }

        // Noise
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::noiseOn, 1), "Noise On", false));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ID::noiseType, 1), "Noise Type", juce::StringArray{"White", "Pink", "Brown", "Blue"}, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::noiseAmp, 1), "Noise Amount", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

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

        // Bitcrusher
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::bitcrushOn, 1), "Bitcrush On", false));
        params.push_back(std::make_unique<juce::AudioParameterInt>(juce::ParameterID(ID::bitcrushBits, 1), "Bitcrush Bits", 1, 16, 8));
        params.push_back(std::make_unique<juce::AudioParameterInt>(juce::ParameterID(ID::bitcrushRate, 1), "Bitcrush Rate", 1, 50, 1));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::bitcrushMix, 1), "Bitcrush Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

        // Phaser / Flanger (append-only; default off => existing presets unchanged)
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::phaserOn, 1), "Phaser On", false));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ID::phaserType, 1), "Phaser Type", juce::StringArray{"Phaser", "Flanger"}, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::phaserRate, 1), "Phaser Rate", juce::NormalisableRange<float>(0.05f, 5.0f, 0.01f, 0.4f), 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::phaserDepth, 1), "Phaser Depth", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.7f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::phaserFeedback, 1), "Phaser Feedback", juce::NormalisableRange<float>(0.0f, 0.95f, 0.01f), 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::phaserMix, 1), "Phaser Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

        // Sub oscillator
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::subOn, 1), "Sub On", false));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ID::subWave, 1), "Sub Wave", juce::StringArray{"Sine", "Square"}, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ID::subOctave, 1), "Sub Octave", juce::StringArray{"-1 Oct", "-2 Oct"}, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::subLevel, 1), "Sub Level", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

        // Arpeggiator
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::arpOn, 1), "Arp On", false));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::arpRate, 1), "Arp Rate", juce::NormalisableRange<float>(1.0f, 16.0f, 0.1f), 8.0f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ID::arpMode, 1), "Arp Mode", juce::StringArray{"Up", "Down", "UpDown", "Random"}, 0));
        params.push_back(std::make_unique<juce::AudioParameterInt>(juce::ParameterID(ID::arpOctaves, 1), "Arp Octaves", 1, 4, 2));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::arpGate, 1), "Arp Gate", juce::NormalisableRange<float>(0.05f, 1.0f, 0.01f), 0.5f));

        // Portamento / glide (append-only; default off => existing presets unchanged)
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::glideOn, 1), "Glide On", false));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::glideTime, 1), "Glide Time", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 0.5f), 0.3f));
        // Mono (default) = only the newest note sounds, gliding from the previous → distinct
        // classic portamento. Poly = every note glides in its own voice (subtler, experimental).
        params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ID::glideMode, 1), "Glide Mode", juce::StringArray{"Mono", "Poly"}, 0));

        // Stereo width (pseudo-stereo master stage)
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::stereoOn, 1), "Stereo On", false));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::stereoWidth, 1), "Stereo Width", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::stereoTime, 1), "Stereo Time", juce::NormalisableRange<float>(1.0f, 15.0f, 0.1f), 12.0f));

        // Master
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::masterVol, 1), "Master Volume", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

        // Global sync tempo (Standalone/base BPM; host tempo overrides in a DAW). Append-only.
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::syncTempo, 1), "Sync Tempo", juce::NormalisableRange<float>(40.0f, 250.0f, 1.0f), 130.0f));

        // Module enables for the formerly always-on modules (Story 2.4). Default TRUE
        // (on) so behaviour is unchanged until the user toggles; append-only.
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::masterOn,  1), "Master On",   true));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::adsrOn,    1), "Envelope On", true));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::mixModeOn, 1), "Cross Mod On", false));   // off => additive (default)
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::scopeOn,    1), "Scope On",    true));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::spectrumOn, 1), "Spectrum On", true));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::keyboardOn, 1), "Keyboard On", true));

        // Pitch envelope (append-only; default off => existing presets unchanged)
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::pitchEnvOn, 1), "Pitch Env On", false));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::pitchEnvAmount, 1), "Pitch Env Amount", juce::NormalisableRange<float>(-48.0f, 48.0f, 0.1f), 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::pitchEnvTime, 1), "Pitch Env Time", juce::NormalisableRange<float>(0.005f, 2.0f, 0.001f, 0.4f), 0.3f));

        // Master-bus compressor (append-only; default off => existing presets unchanged)
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::compOn, 1), "Comp On", false));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::compThreshold, 1), "Comp Threshold", juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), -18.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::compRatio, 1), "Comp Ratio", juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f, 0.5f), 2.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::compAttack, 1), "Comp Attack", juce::NormalisableRange<float>(0.1f, 100.0f, 0.1f, 0.4f), 10.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::compRelease, 1), "Comp Release", juce::NormalisableRange<float>(10.0f, 1000.0f, 1.0f, 0.4f), 120.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::compMakeup, 1), "Comp Makeup", juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f), 0.0f));

        // Modulation matrix (Story 8.1 / Epic 8; append-only). N slots, each a {Source, Target,
        // Amount} routing. Source = {LFO 1, Envelope, Velocity}; Target = {Off + the 7 modulation
        // destinations} with "Off" at index 0 == slot inactive (so the Target index maps 1:1 to
        // LFOTarget). Amount is bipolar (-1..+1), default 0 = no effect. The Target display names
        // are cosmetic — the ComboBoxAttachment maps by INDEX (persistence uses canonical names).
        for (int n = 1; n <= ModMatrixConfig::kNumSlots; ++n)
        {
            params.push_back(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID(ID::modSlotSource(n), 1), "Mod " + juce::String(n) + " Source",
                juce::StringArray{ "LFO 1", "Envelope", "Velocity", "LFO 2" }, 0));   // ORDER == ModSource (append-only)
            params.push_back(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID(ID::modSlotTarget(n), 1), "Mod " + juce::String(n) + " Target",
                juce::StringArray{ "Off", "Pitch", "Amplitude", "Cutoff", "WT Pos", "Vowel", "Resonance", "Wavefold" }, 0));   // ORDER == LFOTarget (0=Off..7)
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(ID::modSlotAmount(n), 1), "Mod " + juce::String(n) + " Amount",
                juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
        }
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(ID::modMatrixOn, 1), "Mod Matrix On", true));

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
