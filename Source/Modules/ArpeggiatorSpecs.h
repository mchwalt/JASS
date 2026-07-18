#pragma once
#include "ModuleSpec.h"

// ARPEGGIATOR. Body order matches the descriptor (MODE, RATE, OCT, GATE). Verbatim.
namespace Modules
{
    inline ModuleSpec arpeggiator()
    {
        ModuleSpec m;
        m.id = "arpeggiator"; m.title = "ARPEGGIATOR"; m.persistObject = "Arp"; m.enableParamId = "arpOn";
        m.type = rack::ModuleType::Modulator; m.zone = rack::Zone::Modulation; m.size = rack::SizeClass::W6H1;
        m.params = {
            { "arpOn",      "Enabled", "",     ParamSpec::Kind::Bool },
            { "arpMode",    "Mode",    "MODE", ParamSpec::Kind::Choice, {}, 0.0f, { "Up", "Down", "UpDown", "Random" } },
            { "arpRate",    "Rate",    "RATE", ParamSpec::Kind::Float, juce::NormalisableRange<float> (1.0f, 16.0f, 0.1f), 8.0f },
            { "arpOctaves", "Octaves", "OCT",  ParamSpec::Kind::Int,   juce::NormalisableRange<float> (1.0f, 4.0f, 1.0f),  2.0f },
            { "arpGate",    "Gate",    "GATE", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.05f, 1.0f, 0.01f), 0.5f },
        };
        return m;
    }
}
