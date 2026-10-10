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
    juce::String legacyPersistKey;              // renamed key: old name still READ as fallback; writes use persistKey.

    // --- Host (DAW) presentation. Every spec initializes positionally, so new fields only ever go
    // at the END and are set by name afterwards (p.hostName = ..., as legacyPersistKey is).
    juce::String hostName;                      // name in the DAW's parameter list ("" => uiLabel, else persistKey).
                                                // For params whose caption only means something inside the
                                                // rack: a step knob captioned "17", a matrix row captioned "SRC".
    bool automatable = true;                    // false => exported WITHOUT kCanAutomate. The value still travels
                                                // in the plugin state; the DAW just offers no lane for it. Used
                                                // for the sequencer grids: thousands of authored cells, never
                                                // automated, that otherwise bury the sixty knobs worth a lane.
};

// The DAW-facing name: "<module title> <hostName|uiLabel|persistKey>". The title prefix stays even
// though the module is also the VST3 unit — a host that flattens the list (Bitwig does) still needs
// to tell "FILTER Resonance" from "FORMANT Resonance". Cosmetic only; state matches by id.
inline juce::String hostParameterName (const ParamSpec& p, const juce::String& namePrefix)
{
    const juce::String leaf = p.hostName.isNotEmpty() ? p.hostName
                            : p.uiLabel.isNotEmpty()  ? p.uiLabel
                                                      : p.persistKey;
    return namePrefix + " " + leaf;
}

// One APVTS parameter from a ParamSpec. Display NAME is cosmetic (state is matched by id).
inline std::unique_ptr<juce::RangedAudioParameter> makeParameter (const ParamSpec& p, const juce::String& namePrefix)
{
    const juce::String name = hostParameterName (p, namePrefix);
    switch (p.kind)
    {
        case ParamSpec::Kind::Bool:
            return std::make_unique<juce::AudioParameterBool>   (juce::ParameterID (p.id, 1), name, p.defaultValue > 0.5f,
                                                                 juce::AudioParameterBoolAttributes().withAutomatable (p.automatable));
        case ParamSpec::Kind::Choice:
            return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID (p.id, 1), name, p.choices, (int) p.defaultValue,
                                                                 juce::AudioParameterChoiceAttributes().withAutomatable (p.automatable));
        case ParamSpec::Kind::Int:
            return std::make_unique<juce::AudioParameterInt>    (juce::ParameterID (p.id, 1), name, (int) p.range.start, (int) p.range.end, (int) p.defaultValue,
                                                                 juce::AudioParameterIntAttributes().withAutomatable (p.automatable));
        case ParamSpec::Kind::Float:
        default:
            return std::make_unique<juce::AudioParameterFloat>  (juce::ParameterID (p.id, 1), name, p.range, p.defaultValue,
                                                                 juce::AudioParameterFloatAttributes().withAutomatable (p.automatable));
    }
}

// One module = one parameter GROUP (the VST3 wrapper exports it as a unit, so a host that honours
// units shows the rack's modules as folders). Group id = module id, group name = module title.
// The children keep the module's registration order, so the flat getParameters() order — which
// PluginProcessor snapshots by index at runtime — is exactly what the old flat vector produced.
inline std::unique_ptr<juce::AudioProcessorParameterGroup> makeModuleParameterGroup (const juce::String& groupId,
                                                                                     const juce::String& title,
                                                                                     const std::vector<ParamSpec>& params)
{
    auto group = std::make_unique<juce::AudioProcessorParameterGroup> (groupId, title, " | ");
    for (const auto& p : params)
        group->addChild (makeParameter (p, title));
    return group;
}
