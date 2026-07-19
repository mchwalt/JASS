#pragma once
#include "ModuleSpec.h"

// STEREO — pseudo-stereo master stage (mono engine → stereo). Values verbatim from Parameters.h.
namespace Modules
{
    inline ModuleSpec stereo()
    {
        ModuleSpec m;
        m.id = "stereo"; m.title = "STEREO"; m.persistObject = "Stereo"; m.enableParamId = "stereoOn";
        m.type = rack::ModuleType::Processor; m.zone = rack::Zone::MasterBus; m.size = rack::SizeClass::W3H1;
        m.params = {
            { "stereoOn",    "Enabled", "",      ParamSpec::Kind::Bool, {}, 1.0f },   // default ON (factory: pseudo-stereo on)
            { "stereoWidth", "Width",   "WIDTH", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),  0.5f },
            { "stereoTime",  "Time",    "TIME",  ParamSpec::Kind::Float, juce::NormalisableRange<float> (1.0f, 15.0f, 0.1f), 12.0f },
        };
        return m;
    }
}
