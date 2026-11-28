#pragma once
#include "ModuleSpec.h"

// GLIDE / portamento. Body order matches the descriptor (MODE, TIME). Verbatim.
namespace Modules
{
    inline ModuleSpec glide()
    {
        ModuleSpec m;
        m.layered = true;   // AD-16: part of the patch — exists for layer A and (prefix b) layer B
        m.id = "glide"; m.title = "GLIDE"; m.persistObject = "Glide"; m.enableParamId = "glideOn";
        // W4 since BEND (22.2): a combo (2 slots) and two knobs no longer fit W3's three.
        m.type = rack::ModuleType::Modulator; m.zone = rack::Zone::Modulation; m.size = rack::SizeClass::W4H1;
        m.defaultVisible = false;   // Story 7.3: used by no preset — see SubSpecs.h for the reasoning
        m.params = {
            { "glideOn",   "Enabled", "",     ParamSpec::Kind::Bool },
            { "glideMode", "Mode",    "MODE", ParamSpec::Kind::Choice, {}, 0.0f, { "Mono", "Poly" } },
            { "glideTime", "Time",    "TIME", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f, 0.5f), 0.3f },
            // BEND (Story 22.2): the layer's pitch-bend range in semitones, up and down alike.
            // It lives here because GLIDE is the module for pitch movement and is layered already;
            // the engine reads it whether the module is on or off (the switch is GLIDE's alone).
            // Appended: a file without it reads 2, the usual keyboard default.
            { "bendRange", "BendRange", "BEND", ParamSpec::Kind::Int, juce::NormalisableRange<float> (0.0f, 24.0f, 1.0f), 2.0f },
        };
        return m;
    }
}
