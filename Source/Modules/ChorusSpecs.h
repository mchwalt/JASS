#pragma once
#include "ModuleSpec.h"

// CHORUS. Values verbatim from Parameters.h.
namespace Modules
{
    inline ModuleSpec chorus()
    {
        ModuleSpec m;
        m.id = "chorus"; m.title = "CHORUS"; m.persistObject = "Chorus"; m.enableParamId = "chorusOn";
        m.type = rack::ModuleType::Processor; m.zone = rack::Zone::Processing; m.size = rack::SizeClass::W3H1;
        m.params = {
            { "chorusOn",    "Enabled", "",      ParamSpec::Kind::Bool },
            { "chorusRate",  "Rate",    "RATE",  ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.1f, 5.0f, 0.01f),      1.5f, {}, {}, LFOTarget::ChorusRate },
            { "chorusDepth", "Depth",   "DEPTH", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.001f, 0.02f, 0.001f), 0.005f, {}, {}, LFOTarget::ChorusDepth },
            { "chorusMix",   "Mix",     "MIX",   ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),      0.5f, {}, {}, LFOTarget::ChorusMix },
        };
        return m;
    }
}
