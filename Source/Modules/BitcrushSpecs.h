#pragma once
#include "ModuleSpec.h"

// BITCRUSH — lo-fi (bit-depth + sample-rate reduction). BITS/RATE are Int params. Verbatim.
namespace Modules
{
    inline ModuleSpec bitcrush()
    {
        ModuleSpec m;
        m.id = "bitcrush"; m.title = "BITCRUSH"; m.persistObject = "Bitcrush"; m.enableParamId = "bitcrushOn";
        m.type = rack::ModuleType::Processor; m.zone = rack::Zone::Processing; m.size = rack::SizeClass::W3H1;
        m.params = {
            { "bitcrushOn",   "Enabled", "",     ParamSpec::Kind::Bool },
            { "bitcrushBits", "Bits",    "BITS", ParamSpec::Kind::Int, juce::NormalisableRange<float> (1.0f, 16.0f, 1.0f), 8.0f },
            { "bitcrushRate", "Rate",    "RATE", ParamSpec::Kind::Int, juce::NormalisableRange<float> (1.0f, 50.0f, 1.0f), 1.0f },
            { "bitcrushMix",  "Mix",     "MIX",  ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f, {}, {}, LFOTarget::BitcrushMix },
        };
        return m;
    }
}
