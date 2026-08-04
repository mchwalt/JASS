#pragma once
#include <JuceHeader.h>
#include "../DSP/LFO.h"   // LFOTarget — audio-safe; rack::ModTarget mirrors its order (Off==None)
#include <vector>
#include <memory>

// AUDIO-safe half of the module-spec system (see docs/MODULE_SYSTEM.md): parameter
// declarations + APVTS generation only. NO UI includes here, so Parameters.h stays UI-free — the
// UI half (ModuleSpec + makeModuleDescriptor) lives in ModuleSpec.h and pulls the rack headers.

struct ParamSpec
{
    juce::String id;          // APVTS id — copied verbatim from the module's existing ID string
    juce::String persistKey;  // .synthy key inside the module object (for the future nested format)
    juce::String uiLabel;     // knob/combo caption ("" => none)

    enum class Kind { Float, Int, Bool, Choice };
    Kind kind = Kind::Float;

    juce::NormalisableRange<float> range {};    // Float / Int
    float defaultValue = 0.0f;                  // Float/Int value · Bool 0/1 · Choice index
    juce::StringArray choices;                  // Choice items — CANONICAL (APVTS + persistence)
    juce::StringArray displayChoices;           // optional UI-only combo labels (empty => use `choices`)

    LFOTarget modTarget = LFOTarget::Off;       // Off => no live mod-ring on this knob
    bool freqDisplay = false;                   // true => FREQ knob shows played frequency (base×ratio)
    bool showInBody  = true;                    // false => APVTS param exists but NO rack control (e.g. SUB octave)
};

// One APVTS parameter from a ParamSpec. Display NAME is cosmetic (state is matched by id).
inline std::unique_ptr<juce::RangedAudioParameter> makeParameter (const ParamSpec& p, const juce::String& namePrefix)
{
    const juce::String name = namePrefix + " " + (p.uiLabel.isNotEmpty() ? p.uiLabel : p.persistKey);
    switch (p.kind)
    {
        case ParamSpec::Kind::Bool:
            return std::make_unique<juce::AudioParameterBool>   (juce::ParameterID (p.id, 1), name, p.defaultValue > 0.5f);
        case ParamSpec::Kind::Choice:
            return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID (p.id, 1), name, p.choices, (int) p.defaultValue);
        case ParamSpec::Kind::Int:
            return std::make_unique<juce::AudioParameterInt>    (juce::ParameterID (p.id, 1), name, (int) p.range.start, (int) p.range.end, (int) p.defaultValue);
        case ParamSpec::Kind::Float:
        default:
            return std::make_unique<juce::AudioParameterFloat>  (juce::ParameterID (p.id, 1), name, p.range, p.defaultValue);
    }
}

inline void appendModuleParameters (const std::vector<ParamSpec>& params, const juce::String& namePrefix,
                                    std::vector<std::unique_ptr<juce::RangedAudioParameter>>& out)
{
    for (const auto& p : params)
        out.push_back (makeParameter (p, namePrefix));
}
