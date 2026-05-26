#pragma once
#include <JuceHeader.h>
#include "../DSP/Oscillator.h"
#include "../DSP/BiquadFilter.h"
#include "../DSP/Effects.h"
#include "../DSP/AdsrEnvelope.h"
#include "../DSP/LFO.h"
#include "../DSP/NoiseGenerator.h"
#include "../DSP/KarplusStrong.h"
#include "../DSP/WavetableOscillator.h"

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

        // Mix mode
        constexpr const char* mixMode = "mixMode";

        // ADSR
        constexpr const char* attack    = "attack";
        constexpr const char* decay     = "decay";
        constexpr const char* sustain   = "sustain";
        constexpr const char* release   = "release";

        // Filter
        constexpr const char* filterType   = "filterType";
        constexpr const char* filterCutoff = "filterCutoff";
        constexpr const char* filterReso   = "filterReso";

        // Distortion
        constexpr const char* distortionType  = "distortionType";
        constexpr const char* distortionDrive = "distortionDrive";
        constexpr const char* distortionMix   = "distortionMix";

        // Delay
        constexpr const char* delayOn       = "delayOn";
        constexpr const char* delayTime     = "delayTime";
        constexpr const char* delayFeedback = "delayFeedback";
        constexpr const char* delayMix      = "delayMix";

        // Chorus
        constexpr const char* chorusOn    = "chorusOn";
        constexpr const char* chorusRate  = "chorusRate";
        constexpr const char* chorusDepth = "chorusDepth";
        constexpr const char* chorusMix   = "chorusMix";

        // LFO
        constexpr const char* lfoWave   = "lfoWave";
        constexpr const char* lfoRate   = "lfoRate";
        constexpr const char* lfoDepth  = "lfoDepth";
        constexpr const char* lfoTarget = "lfoTarget";

        // Reverb
        constexpr const char* reverbOn   = "reverbOn";
        constexpr const char* reverbRoom = "reverbRoom";
        constexpr const char* reverbDamp = "reverbDamp";
        constexpr const char* reverbMix  = "reverbMix";

        // Noise
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
    }

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

        // Oscillators
        float defaultFreqs[] = { 261.63f, 329.63f, 392.0f };
        float defaultAmps[]  = { 0.5f, 0.0f, 0.0f };

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
        }

        // Mix mode
        params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ID::mixMode, 1), "Mix Mode", juce::StringArray{"Additive", "RingMod", "FM"}, 0));

        // ADSR
        auto timeRange = juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.4f);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::attack,  1), "Attack",  timeRange, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::decay,   1), "Decay",   timeRange, 0.3f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::sustain, 1), "Sustain", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.7f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::release, 1), "Release", timeRange, 1.0f));

        // Filter
        params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ID::filterType, 1), "Filter Type", juce::StringArray{"Off", "Lowpass", "Highpass"}, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::filterCutoff, 1), "Filter Cutoff", juce::NormalisableRange<float>(20.0f, 10000.0f, 1.0f, 0.3f), 5000.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::filterReso, 1), "Filter Resonance", juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f), 0.707f));

        // Distortion
        params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ID::distortionType, 1), "Distortion Type", juce::StringArray{"Off", "SoftClip", "HardClip", "Foldback"}, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::distortionDrive, 1), "Distortion Drive", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::distortionMix, 1), "Distortion Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

        // Delay
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ID::delayOn, 1), "Delay On", false));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::delayTime, 1), "Delay Time", juce::NormalisableRange<float>(0.01f, 2.0f, 0.01f), 0.3f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::delayFeedback, 1), "Delay Feedback", juce::NormalisableRange<float>(0.0f, 0.95f, 0.01f), 0.4f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::delayMix, 1), "Delay Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f));

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

        // LFO
        params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ID::lfoWave, 1), "LFO Wave", juce::StringArray{"Sine", "Triangle", "Square", "Sawtooth"}, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::lfoRate, 1), "LFO Rate", juce::NormalisableRange<float>(0.1f, 20.0f, 0.1f), 2.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::lfoDepth, 1), "LFO Depth", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ID::lfoTarget, 1), "LFO Target", juce::StringArray{"Off", "Frequency", "Amplitude", "Filter Cutoff"}, 0));

        // Noise
        params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ID::noiseType, 1), "Noise Type", juce::StringArray{"Off", "White", "Pink"}, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::noiseAmp, 1), "Noise Amount", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f));

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

        // Master
        params.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ID::masterVol, 1), "Master Volume", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.7f));

        return { params.begin(), params.end() };
    }

    // Apply all parameters to a voice
    inline void applyToVoice(juce::AudioProcessorValueTreeState& apvts,
                              Oscillator* oscillators, AdsrEnvelope& env,
                              BiquadFilter& filter, DistortionEffect& distortion,
                              DelayEffect& delay,
                              ChorusEffect& chorus, ReverbEffect& reverb,
                              LFO& lfo, NoiseGenerator& noise,
                              KarplusStrong& karplus, WavetableOscillator& wavetable,
                              MixMode& mixMode)
    {
        mixMode = static_cast<MixMode>(static_cast<int>(*apvts.getRawParameterValue(ID::mixMode)));

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
        }

        env.setAttack(*apvts.getRawParameterValue(ID::attack));
        env.setDecay(*apvts.getRawParameterValue(ID::decay));
        env.setSustain(*apvts.getRawParameterValue(ID::sustain));
        env.setRelease(*apvts.getRawParameterValue(ID::release));

        filter.setType(static_cast<FilterType>(static_cast<int>(*apvts.getRawParameterValue(ID::filterType))));
        filter.setCutoff(*apvts.getRawParameterValue(ID::filterCutoff));
        filter.setResonance(*apvts.getRawParameterValue(ID::filterReso));

        distortion.type  = static_cast<DistortionType>(static_cast<int>(*apvts.getRawParameterValue(ID::distortionType)));
        distortion.drive = *apvts.getRawParameterValue(ID::distortionDrive);
        distortion.mix   = *apvts.getRawParameterValue(ID::distortionMix);

        delay.enabled  = *apvts.getRawParameterValue(ID::delayOn) > 0.5f;
        delay.time     = *apvts.getRawParameterValue(ID::delayTime);
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

        lfo.setWaveform(static_cast<LFOWaveform>(static_cast<int>(*apvts.getRawParameterValue(ID::lfoWave))));
        lfo.setRate(*apvts.getRawParameterValue(ID::lfoRate));
        lfo.setDepth(*apvts.getRawParameterValue(ID::lfoDepth));
        lfo.setTarget(static_cast<LFOTarget>(static_cast<int>(*apvts.getRawParameterValue(ID::lfoTarget))));

        noise.setType(static_cast<NoiseType>(static_cast<int>(*apvts.getRawParameterValue(ID::noiseType))));
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
    }
}
