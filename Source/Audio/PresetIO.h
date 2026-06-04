#pragma once
#include <JuceHeader.h>
#include "Parameters.h"

// Reads/writes the shared ".synthy" JSON preset format used by both the C#
// and the C++ Synthy. Field names and enum strings match the C# Preset class
// exactly so a preset saved in one app loads in the other.
//
// Canonical enum strings == C# enum member names (no display spaces).
namespace PresetIO
{
    inline const juce::StringArray kWaveform   { "Sine", "Sawtooth", "Square", "Triangle" };
    inline const juce::StringArray kMixMode    { "Additive", "RingMod", "FM" };
    inline const juce::StringArray kFilterType { "Off", "Lowpass", "Highpass" };
    inline const juce::StringArray kDistortion { "Off", "SoftClip", "HardClip", "Foldback" };
    inline const juce::StringArray kLfoWave    { "Sine", "Triangle", "Square", "Sawtooth" };
    inline const juce::StringArray kLfoTarget  { "Off", "Frequency", "Amplitude", "FilterCutoff" };
    inline const juce::StringArray kNoiseType  { "Off", "White", "Pink" };
    inline const juce::StringArray kSubWave     { "Sine", "Square" };

    constexpr int kFormatVersion = 1;

    // Shared root: %AppData%\Roaming\Synthy (same as the C# app).
    inline juce::File synthyFolder()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("Synthy");
        dir.createDirectory();
        return dir;
    }

    // Named presets folder: %AppData%\Roaming\Synthy\Presets.
    inline juce::File presetsFolder()
    {
        auto dir = synthyFolder().getChildFile("Presets");
        dir.createDirectory();
        return dir;
    }

    // Shared live state: auto-loaded on start, auto-saved on change by BOTH apps,
    // so switching between C# and C++ keeps the current settings for A/B testing.
    inline juce::File liveStateFile()
    {
        return synthyFolder().getChildFile("LiveState.synthy");
    }

    namespace detail
    {
        using APVTS = juce::AudioProcessorValueTreeState;

        inline double rawF(APVTS& a, const juce::String& id) { return (double) *a.getRawParameterValue(id); }
        inline int    rawI(APVTS& a, const juce::String& id) { return (int)    *a.getRawParameterValue(id); }
        inline bool   rawB(APVTS& a, const juce::String& id) { return *a.getRawParameterValue(id) > 0.5f; }
        inline juce::String rawChoice(APVTS& a, const juce::String& id, const juce::StringArray& names)
        {
            int i = rawI(a, id);
            return juce::isPositiveAndBelow(i, names.size()) ? names[i] : names[0];
        }

        // Set a parameter from an engineering (raw) value via its normalisable range.
        inline void setRaw(APVTS& a, const juce::String& id, float rawValue)
        {
            if (auto* p = a.getParameter(id))
                p->setValueNotifyingHost(p->convertTo0to1(rawValue));
        }
        inline void setChoice(APVTS& a, const juce::String& id, const juce::StringArray& names, const juce::var& v, int fallback)
        {
            int idx = names.indexOf(v.toString());
            setRaw(a, id, (float) (idx >= 0 ? idx : fallback));
        }

        // JSON readers with fallback to the current value (missing fields keep state).
        inline double jnum (const juce::var& o, const char* k, double def) { return o.hasProperty(k) ? (double) o[k] : def; }
        inline bool   jbool(const juce::var& o, const char* k, bool def)   { return o.hasProperty(k) ? (bool)   o[k] : def; }
        inline int    jint (const juce::var& o, const char* k, int def)    { return o.hasProperty(k) ? (int)    o[k] : def; }
    }

    using APVTS = juce::AudioProcessorValueTreeState;
    namespace ID = Parameters::ID;

    // ── Export ──
    inline juce::var toVar(APVTS& a, const juce::String& name)
    {
        using namespace detail;
        auto* root = new juce::DynamicObject();
        root->setProperty("FormatVersion", kFormatVersion);
        root->setProperty("Name", name);

        juce::Array<juce::var> oscs;
        for (int o = 1; o <= 3; ++o)
        {
            auto* od = new juce::DynamicObject();
            od->setProperty("Enabled",      rawB(a, ID::oscOn(o)));
            od->setProperty("Waveform",     rawChoice(a, ID::oscWave(o), kWaveform));
            od->setProperty("Frequency",    rawF(a, ID::oscFreq(o)));
            od->setProperty("Amplitude",    rawF(a, ID::oscAmp(o)));
            od->setProperty("UnisonVoices", rawI(a, ID::oscUniVoices(o)));
            od->setProperty("UnisonDetune", rawF(a, ID::oscUniDetune(o)));
            oscs.add(juce::var(od));
        }
        root->setProperty("Oscillators", oscs);

        root->setProperty("MasterVolume", rawF(a, ID::masterVol));
        root->setProperty("MixMode",      rawChoice(a, ID::mixMode, kMixMode));

        root->setProperty("Attack",  rawF(a, ID::attack));
        root->setProperty("Decay",   rawF(a, ID::decay));
        root->setProperty("Sustain", rawF(a, ID::sustain));
        root->setProperty("Release", rawF(a, ID::release));

        root->setProperty("FilterType",      rawChoice(a, ID::filterType, kFilterType));
        root->setProperty("FilterCutoff",    rawF(a, ID::filterCutoff));
        root->setProperty("FilterResonance", rawF(a, ID::filterReso));

        root->setProperty("DistortionType",  rawChoice(a, ID::distortionType, kDistortion));
        root->setProperty("DistortionDrive", rawF(a, ID::distortionDrive));
        root->setProperty("DistortionMix",   rawF(a, ID::distortionMix));

        root->setProperty("WavefoldEnabled",  rawB(a, ID::wavefoldOn));
        root->setProperty("WavefoldDrive",    rawF(a, ID::wavefoldDrive));
        root->setProperty("WavefoldSymmetry", rawF(a, ID::wavefoldSymmetry));
        root->setProperty("WavefoldMix",      rawF(a, ID::wavefoldMix));

        root->setProperty("BitcrushEnabled", rawB(a, ID::bitcrushOn));
        root->setProperty("BitcrushBits",    rawI(a, ID::bitcrushBits));
        root->setProperty("BitcrushRate",    rawI(a, ID::bitcrushRate));
        root->setProperty("BitcrushMix",     rawF(a, ID::bitcrushMix));

        root->setProperty("SubEnabled",  rawB(a, ID::subOn));
        root->setProperty("SubWaveform", rawChoice(a, ID::subWave, kSubWave));
        root->setProperty("SubOctave",   -(rawI(a, ID::subOctave) + 1));  // -1 or -2
        root->setProperty("SubLevel",    rawF(a, ID::subLevel));

        root->setProperty("LfoWaveform", rawChoice(a, ID::lfoWave, kLfoWave));
        root->setProperty("LfoTarget",   rawChoice(a, ID::lfoTarget, kLfoTarget));
        root->setProperty("LfoRate",     rawF(a, ID::lfoRate));
        root->setProperty("LfoDepth",    rawF(a, ID::lfoDepth));

        root->setProperty("DelayEnabled",  rawB(a, ID::delayOn));
        root->setProperty("DelayTime",     rawF(a, ID::delayTime));
        root->setProperty("DelayFeedback", rawF(a, ID::delayFeedback));
        root->setProperty("DelayMix",      rawF(a, ID::delayMix));

        root->setProperty("ChorusEnabled", rawB(a, ID::chorusOn));
        root->setProperty("ChorusRate",    rawF(a, ID::chorusRate));
        root->setProperty("ChorusDepth",   rawF(a, ID::chorusDepth));
        root->setProperty("ChorusMix",     rawF(a, ID::chorusMix));

        root->setProperty("ReverbEnabled",  rawB(a, ID::reverbOn));
        root->setProperty("ReverbRoomSize", rawF(a, ID::reverbRoom));
        root->setProperty("ReverbDamping",  rawF(a, ID::reverbDamp));
        root->setProperty("ReverbMix",      rawF(a, ID::reverbMix));

        root->setProperty("KarplusEnabled",   rawB(a, ID::karplusOn));
        root->setProperty("KarplusFrequency", rawF(a, ID::karplusFreq));
        root->setProperty("KarplusAmplitude", rawF(a, ID::karplusAmp));
        root->setProperty("KarplusDamping",   rawF(a, ID::karplusDamping));
        root->setProperty("KarplusStretch",   rawF(a, ID::karplusStretch));

        root->setProperty("NoiseType",      rawChoice(a, ID::noiseType, kNoiseType));
        root->setProperty("NoiseAmplitude", rawF(a, ID::noiseAmp));

        root->setProperty("WavetableEnabled",      rawB(a, ID::wavetableOn));
        root->setProperty("WavetableBankIndex",    rawI(a, ID::wavetableBank));
        root->setProperty("WavetablePosition",     rawF(a, ID::wavetablePosition));
        root->setProperty("WavetableFrequency",    rawF(a, ID::wavetableFreq));
        root->setProperty("WavetableAmplitude",    rawF(a, ID::wavetableAmp));
        root->setProperty("WavetableUnisonVoices", rawI(a, ID::wavetableUniVoices));
        root->setProperty("WavetableUnisonDetune", rawF(a, ID::wavetableUniDetune));

        return juce::var(root);
    }

    inline bool saveToFile(APVTS& a, const juce::File& file, const juce::String& name)
    {
        return file.replaceWithText(juce::JSON::toString(toVar(a, name), false));
    }

    // ── Import ──
    inline void applyVar(APVTS& a, const juce::var& v)
    {
        using namespace detail;
        if (! v.isObject())
            return;

        if (auto oscs = v["Oscillators"]; oscs.isArray())
        {
            for (int o = 0; o < juce::jmin(3, oscs.size()); ++o)
            {
                auto od = oscs[o];
                setRaw   (a, ID::oscOn(o + 1),        jbool(od, "Enabled", rawB(a, ID::oscOn(o + 1))) ? 1.f : 0.f);
                setChoice(a, ID::oscWave(o + 1), kWaveform, od["Waveform"], rawI(a, ID::oscWave(o + 1)));
                setRaw   (a, ID::oscFreq(o + 1),      (float) jnum(od, "Frequency", rawF(a, ID::oscFreq(o + 1))));
                setRaw   (a, ID::oscAmp(o + 1),       (float) jnum(od, "Amplitude", rawF(a, ID::oscAmp(o + 1))));
                setRaw   (a, ID::oscUniVoices(o + 1), (float) jint(od, "UnisonVoices", rawI(a, ID::oscUniVoices(o + 1))));
                setRaw   (a, ID::oscUniDetune(o + 1), (float) jnum(od, "UnisonDetune", rawF(a, ID::oscUniDetune(o + 1))));
            }
        }

        setRaw   (a, ID::masterVol, (float) jnum(v, "MasterVolume", rawF(a, ID::masterVol)));
        setChoice(a, ID::mixMode, kMixMode, v["MixMode"], rawI(a, ID::mixMode));

        setRaw(a, ID::attack,  (float) jnum(v, "Attack",  rawF(a, ID::attack)));
        setRaw(a, ID::decay,   (float) jnum(v, "Decay",   rawF(a, ID::decay)));
        setRaw(a, ID::sustain, (float) jnum(v, "Sustain", rawF(a, ID::sustain)));
        setRaw(a, ID::release, (float) jnum(v, "Release", rawF(a, ID::release)));

        setChoice(a, ID::filterType, kFilterType, v["FilterType"], rawI(a, ID::filterType));
        setRaw(a, ID::filterCutoff, (float) jnum(v, "FilterCutoff",    rawF(a, ID::filterCutoff)));
        setRaw(a, ID::filterReso,   (float) jnum(v, "FilterResonance", rawF(a, ID::filterReso)));

        setChoice(a, ID::distortionType, kDistortion, v["DistortionType"], rawI(a, ID::distortionType));
        setRaw(a, ID::distortionDrive, (float) jnum(v, "DistortionDrive", rawF(a, ID::distortionDrive)));
        setRaw(a, ID::distortionMix,   (float) jnum(v, "DistortionMix",   rawF(a, ID::distortionMix)));

        setRaw(a, ID::wavefoldOn,       jbool(v, "WavefoldEnabled", rawB(a, ID::wavefoldOn)) ? 1.f : 0.f);
        setRaw(a, ID::wavefoldDrive,    (float) jnum(v, "WavefoldDrive",    rawF(a, ID::wavefoldDrive)));
        setRaw(a, ID::wavefoldSymmetry, (float) jnum(v, "WavefoldSymmetry", rawF(a, ID::wavefoldSymmetry)));
        setRaw(a, ID::wavefoldMix,      (float) jnum(v, "WavefoldMix",      rawF(a, ID::wavefoldMix)));

        setRaw(a, ID::bitcrushOn,   jbool(v, "BitcrushEnabled", rawB(a, ID::bitcrushOn)) ? 1.f : 0.f);
        setRaw(a, ID::bitcrushBits, (float) jint(v, "BitcrushBits", rawI(a, ID::bitcrushBits)));
        setRaw(a, ID::bitcrushRate, (float) jint(v, "BitcrushRate", rawI(a, ID::bitcrushRate)));
        setRaw(a, ID::bitcrushMix,  (float) jnum(v, "BitcrushMix",  rawF(a, ID::bitcrushMix)));

        setRaw   (a, ID::subOn, jbool(v, "SubEnabled", rawB(a, ID::subOn)) ? 1.f : 0.f);
        setChoice(a, ID::subWave, kSubWave, v["SubWaveform"], rawI(a, ID::subWave));
        // Stored as -1 / -2; map back to choice index 0 / 1.
        setRaw   (a, ID::subOctave, (float) (-jint(v, "SubOctave", -(rawI(a, ID::subOctave) + 1)) - 1));
        setRaw   (a, ID::subLevel, (float) jnum(v, "SubLevel", rawF(a, ID::subLevel)));

        setChoice(a, ID::lfoWave,   kLfoWave,   v["LfoWaveform"], rawI(a, ID::lfoWave));
        setChoice(a, ID::lfoTarget, kLfoTarget, v["LfoTarget"],   rawI(a, ID::lfoTarget));
        setRaw(a, ID::lfoRate,  (float) jnum(v, "LfoRate",  rawF(a, ID::lfoRate)));
        setRaw(a, ID::lfoDepth, (float) jnum(v, "LfoDepth", rawF(a, ID::lfoDepth)));

        setRaw(a, ID::delayOn,       jbool(v, "DelayEnabled", rawB(a, ID::delayOn)) ? 1.f : 0.f);
        setRaw(a, ID::delayTime,     (float) jnum(v, "DelayTime",     rawF(a, ID::delayTime)));
        setRaw(a, ID::delayFeedback, (float) jnum(v, "DelayFeedback", rawF(a, ID::delayFeedback)));
        setRaw(a, ID::delayMix,      (float) jnum(v, "DelayMix",      rawF(a, ID::delayMix)));

        setRaw(a, ID::chorusOn,    jbool(v, "ChorusEnabled", rawB(a, ID::chorusOn)) ? 1.f : 0.f);
        setRaw(a, ID::chorusRate,  (float) jnum(v, "ChorusRate",  rawF(a, ID::chorusRate)));
        setRaw(a, ID::chorusDepth, (float) jnum(v, "ChorusDepth", rawF(a, ID::chorusDepth)));
        setRaw(a, ID::chorusMix,   (float) jnum(v, "ChorusMix",   rawF(a, ID::chorusMix)));

        setRaw(a, ID::reverbOn,   jbool(v, "ReverbEnabled", rawB(a, ID::reverbOn)) ? 1.f : 0.f);
        setRaw(a, ID::reverbRoom, (float) jnum(v, "ReverbRoomSize", rawF(a, ID::reverbRoom)));
        setRaw(a, ID::reverbDamp, (float) jnum(v, "ReverbDamping",  rawF(a, ID::reverbDamp)));
        setRaw(a, ID::reverbMix,  (float) jnum(v, "ReverbMix",      rawF(a, ID::reverbMix)));

        setRaw(a, ID::karplusOn,      jbool(v, "KarplusEnabled", rawB(a, ID::karplusOn)) ? 1.f : 0.f);
        setRaw(a, ID::karplusFreq,    (float) jnum(v, "KarplusFrequency", rawF(a, ID::karplusFreq)));
        setRaw(a, ID::karplusAmp,     (float) jnum(v, "KarplusAmplitude", rawF(a, ID::karplusAmp)));
        setRaw(a, ID::karplusDamping, (float) jnum(v, "KarplusDamping",   rawF(a, ID::karplusDamping)));
        setRaw(a, ID::karplusStretch, (float) jnum(v, "KarplusStretch",   rawF(a, ID::karplusStretch)));

        setChoice(a, ID::noiseType, kNoiseType, v["NoiseType"], rawI(a, ID::noiseType));
        setRaw(a, ID::noiseAmp, (float) jnum(v, "NoiseAmplitude", rawF(a, ID::noiseAmp)));

        setRaw(a, ID::wavetableOn,        jbool(v, "WavetableEnabled", rawB(a, ID::wavetableOn)) ? 1.f : 0.f);
        setRaw(a, ID::wavetableBank,      (float) jint(v, "WavetableBankIndex",    rawI(a, ID::wavetableBank)));
        setRaw(a, ID::wavetablePosition,  (float) jnum(v, "WavetablePosition",     rawF(a, ID::wavetablePosition)));
        setRaw(a, ID::wavetableFreq,      (float) jnum(v, "WavetableFrequency",    rawF(a, ID::wavetableFreq)));
        setRaw(a, ID::wavetableAmp,       (float) jnum(v, "WavetableAmplitude",    rawF(a, ID::wavetableAmp)));
        setRaw(a, ID::wavetableUniVoices, (float) jint(v, "WavetableUnisonVoices", rawI(a, ID::wavetableUniVoices)));
        setRaw(a, ID::wavetableUniDetune, (float) jnum(v, "WavetableUnisonDetune", rawF(a, ID::wavetableUniDetune)));
    }

    inline bool loadFromFile(APVTS& a, const juce::File& file)
    {
        if (! file.existsAsFile())
            return false;
        auto v = juce::JSON::parse(file.loadFileAsString());
        if (! v.isObject())
            return false;
        applyVar(a, v);
        return true;
    }
}
