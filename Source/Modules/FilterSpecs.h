#pragma once
#include "ModuleSpec.h"

// FILTER module — one header per component (decision: FilterSpecs.h, DistortionSpecs.h, …).
// IDs are copied verbatim from Parameters::ID::filter* (the DSP/PresetIO still use those). Ranges
// and defaults MUST match the originals so the default patch stays byte-identical.
namespace Modules
{
    inline ModuleSpec filter()
    {
        ModuleSpec m;
        m.id = "filter"; m.title = "FILTER"; m.persistObject = "Filter"; m.enableParamId = "filterOn";
        m.type = rack::ModuleType::Processor;
        m.zone = rack::Zone::Processing;
        m.size = rack::SizeClass::W4H1;
        m.params = {
            { "filterOn",     "Enabled",   "",       ParamSpec::Kind::Bool },
            { "filterType",   "Type",      "TYPE",   ParamSpec::Kind::Choice, {}, 0.0f, { "Lowpass", "Highpass" } },
            { "filterCutoff", "Cutoff",    "CUTOFF", ParamSpec::Kind::Float, juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.3f), 550.0f,  {}, {}, LFOTarget::FilterCutoff },
            { "filterReso",   "Resonance", "RESO",   ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.1f, 10.0f, 0.01f),          0.707f, {}, {}, LFOTarget::FilterResonance },
        };
        return m;
    }
}
