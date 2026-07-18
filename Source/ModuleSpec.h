#pragma once
#include <JuceHeader.h>
#include "UI/rack/ModuleDescriptor.h"
#include <vector>
#include <memory>

// Declarative module specification — see docs/Modul_Architektur_Konzept.md.
//
// A ModuleSpec is the SINGLE place a module is defined: its parameters + UI placement. From it we
// GENERATE the APVTS layout and the rack UI descriptor, instead of hand-writing them in three
// files. The DSP process() logic stays hand-written (a header can't generate audio maths).
//
// PROOF STAGE (2026-07-18): only FILTER is spec-driven; every other module is still hand-written.
// Persistence (PresetIO) is NOT yet generated — that (nested .synthy) is the coordinated final step.
//
// NOTE (layering finding): this header pulls the UI ModuleDescriptor.h into the audio layer via
// Parameters.h. Acceptable for the proof; if we proceed, split the param-generation (audio) from
// the descriptor-generation (UI) so Parameters.h stays UI-free.

struct ParamSpec
{
    juce::String id;          // APVTS id — MUST equal the Parameters::ID string (kept in sync by hand)
    juce::String persistKey;  // .synthy key inside the module object (used once persistence is nested)
    juce::String uiLabel;     // knob/combo caption ("" => none)

    enum class Kind { Float, Int, Bool, Choice };
    Kind kind = Kind::Float;

    juce::NormalisableRange<float> range {};    // Float / Int
    float defaultValue = 0.0f;                  // Float/Int value · Bool 0/1 · Choice index
    juce::StringArray choices;                  // Choice items
    rack::ModTarget modTarget = rack::ModTarget::None;   // optional live mod-ring on the knob
};

struct ModuleSpec
{
    juce::String id;             // stable slug ("filter") — help id + layout key
    juce::String title;          // display title ("FILTER")
    juce::String persistObject;  // .synthy object key ("Filter") — for the nested-persistence step
    juce::String enableParamId;  // id of the Bool param that is the header on/off ("" => always on)
    rack::ModuleType type {};
    rack::Zone       zone {};
    rack::SizeClass  size {};
    bool defaultVisible = true;
    std::vector<ParamSpec> params;
};

// --- Generators ------------------------------------------------------------

// One APVTS parameter from a ParamSpec. The display NAME is cosmetic (state is matched by id).
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

// Append all of a module's parameters to the APVTS layout vector (replaces the hand-written
// params.push_back(...) block for that module).
inline void appendModuleParameters (const ModuleSpec& m, std::vector<std::unique_ptr<juce::RangedAudioParameter>>& out)
{
    for (const auto& p : m.params)
        out.push_back (makeParameter (p, m.title));
}

// Build the rack UI descriptor from a spec: the enable Bool becomes the header toggle; every other
// param becomes a body element (Choice => Combo, Bool => Toggle, Float/Int => Knob + optional ring).
inline rack::ModuleDescriptor makeModuleDescriptor (const ModuleSpec& m)
{
    rack::ModuleDescriptor d;
    d.sizeClass = m.size; d.type = m.type; d.defaultZone = m.zone;
    d.defaultVisible = m.defaultVisible; d.id = m.id; d.title = m.title;
    d.enableParam = m.enableParamId;

    for (const auto& p : m.params)
    {
        if (p.id == m.enableParamId) continue;   // the enable bool is the header toggle, not a body cell
        if (p.kind == ParamSpec::Kind::Choice)
            d.body.push_back (rack::Combo { p.id, p.uiLabel, p.choices });
        else if (p.kind == ParamSpec::Kind::Bool)
            d.body.push_back (rack::Toggle { p.id, p.uiLabel });
        else
        {
            rack::Knob k { p.id, p.uiLabel };
            k.modTarget = p.modTarget;
            d.body.push_back (k);
        }
    }
    return d;
}
