#pragma once
#include "ModuleSpec.h"

// ARPEGGIATOR. Body order matches the descriptor (MODE, RATE, OCT, GATE). Verbatim.
namespace Modules
{
    inline ModuleSpec arpeggiator()
    {
        ModuleSpec m;
        m.id = "arpeggiator"; m.title = "ARPEGGIATOR"; m.persistObject = "Arp"; m.enableParamId = "arpOn";
        m.defaultVisible = false;   // hidden until switched on: the stock rack is the Init set (maintainer 2026-10-03)
        m.type = rack::ModuleType::Modulator; m.zone = rack::Zone::Modulation; m.size = rack::SizeClass::W6H1;
        m.params = {
            { "arpOn",      "Enabled", "",     ParamSpec::Kind::Bool },
            { "arpMode",    "Mode",    "MODE", ParamSpec::Kind::Choice, {}, 0.0f, { "Up", "Down", "UpDown", "Random" } },
            { "arpRate",    "Rate",    "RATE", ParamSpec::Kind::Float, juce::NormalisableRange<float> (1.0f, 16.0f, 0.1f), 8.0f },
            { "arpOctaves", "Octaves", "OCT",  ParamSpec::Kind::Int,   juce::NormalisableRange<float> (1.0f, 4.0f, 1.0f),  2.0f },
            { "arpGate",    "Gate",    "GATE", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.05f, 1.0f, 0.01f), 0.5f },
            // TARGET (AD-16 S3, Story 21.7, appended): the layer(s) the ARP plays. Auto = the
            // layers that play (Single: the selected one, Dual: both, Split: B, the left hand);
            // in a Split only the target layer's keys feed the ARP, the other hand plays live.
            { "arpTarget",  "LayerTarget", "TARGET", ParamSpec::Kind::Choice, {}, 0.0f, { "Auto", "A", "B", "A+B" } },
        };
        return m;
    }
}
