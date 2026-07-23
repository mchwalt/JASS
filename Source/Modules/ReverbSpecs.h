#pragma once
#include "ModuleSpec.h"

// REVERB. Values verbatim from Parameters.h (persist keys mirror the PresetIO ReverbRoomSize etc.).
namespace Modules
{
    inline ModuleSpec reverb()
    {
        ModuleSpec m;
        m.id = "reverb"; m.title = "REVERB"; m.persistObject = "Reverb"; m.enableParamId = "reverbOn";
        m.type = rack::ModuleType::Processor; m.zone = rack::Zone::Processing; m.size = rack::SizeClass::W3H1;
        m.params = {
            { "reverbOn",   "Enabled",  "",     ParamSpec::Kind::Bool },
            { "reverbRoom", "RoomSize", "ROOM", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.7f, {}, {}, LFOTarget::ReverbRoom },
            { "reverbDamp", "Damping",  "DAMP", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f, {}, {}, LFOTarget::ReverbDamp },
            { "reverbMix",  "Mix",      "MIX",  ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f, {}, {}, LFOTarget::ReverbMix },
        };
        return m;
    }
}
