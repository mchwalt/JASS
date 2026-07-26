#pragma once
#include "ModuleSpec.h"

// NOISE — White/Pink/Brown/Blue noise. Values verbatim from Parameters.h.
namespace Modules
{
    inline ModuleSpec noise()
    {
        ModuleSpec m;
        m.id = "noise"; m.title = "NOISE"; m.persistObject = "Noise"; m.enableParamId = "noiseOn";
        m.type = rack::ModuleType::Generator; m.zone = rack::Zone::Generators; m.size = rack::SizeClass::W3H1;
        m.params = {
            { "noiseOn",   "Enabled", "",    ParamSpec::Kind::Bool },
            { "noiseType", "Type",    "TYPE", ParamSpec::Kind::Choice, {}, 0.0f, { "White", "Pink", "Brown", "Blue" } },
            { "noiseAmp",  "Amount",  "AMP",  ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f, {}, {}, LFOTarget::NoiseLevel },
        };
        return m;
    }
}
