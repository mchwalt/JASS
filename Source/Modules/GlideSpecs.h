#pragma once
#include "ModuleSpec.h"

// GLIDE / portamento. Body order matches the descriptor (MODE, TIME). Verbatim.
namespace Modules
{
    inline ModuleSpec glide()
    {
        ModuleSpec m;
        m.id = "glide"; m.title = "GLIDE"; m.persistObject = "Glide"; m.enableParamId = "glideOn";
        m.type = rack::ModuleType::Modulator; m.zone = rack::Zone::Modulation; m.size = rack::SizeClass::W3H1;
        m.params = {
            { "glideOn",   "Enabled", "",     ParamSpec::Kind::Bool },
            { "glideMode", "Mode",    "MODE", ParamSpec::Kind::Choice, {}, 0.0f, { "Mono", "Poly" } },
            { "glideTime", "Time",    "TIME", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f, 0.5f), 0.3f },
        };
        return m;
    }
}
