#pragma once
#include "ModuleSpec.h"

// Central registry of module specs (decision 1: one central file first; split per-module later).
// PROOF STAGE: only FILTER lives here so far; other modules are still hand-written in
// Parameters.h / PluginEditor.cpp and will move here one at a time.
//
// The `id` strings MUST match Parameters::ID::filter* (kept in sync by hand for now — the DSP in
// applyToVoice still reads those ID constants). Ranges/defaults MUST match the originals so the
// default patch stays byte-identical.
namespace Modules
{
    inline ModuleSpec filter()
    {
        ModuleSpec m;
        m.id = "filter"; m.title = "FILTER"; m.persistObject = "Filter"; m.enableParamId = "filterOn";
        m.type = rack::ModuleType::Processor;
        m.zone = rack::Zone::Processing;
        m.size = rack::SizeClass::W4H1;
        m.defaultVisible = true;
        m.params = {
            { "filterOn",     "Enabled",   "",       ParamSpec::Kind::Bool },
            { "filterType",   "Type",      "TYPE",   ParamSpec::Kind::Choice, {}, 0.0f, { "Lowpass", "Highpass" } },
            { "filterCutoff", "Cutoff",    "CUTOFF", ParamSpec::Kind::Float, juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.3f), 550.0f,  {}, rack::ModTarget::FilterCutoff },
            { "filterReso",   "Resonance", "RESO",   ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.1f, 10.0f, 0.01f),          0.707f, {}, rack::ModTarget::FilterResonance },
        };
        return m;
    }
}
