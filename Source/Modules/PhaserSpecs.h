#pragma once
#include "ModuleSpec.h"

// PHASER / FLANGER. Values verbatim from Parameters.h.
namespace Modules
{
    inline ModuleSpec phaser()
    {
        ModuleSpec m;
        m.id = "phaser"; m.title = "PHASER"; m.persistObject = "Phaser"; m.enableParamId = "phaserOn";
        m.type = rack::ModuleType::Processor; m.zone = rack::Zone::Processing; m.size = rack::SizeClass::W6H1;
        m.defaultVisible = false;   // Story 7.3: used by no preset — see SubSpecs.h for the reasoning
        m.params = {
            { "phaserOn",       "Enabled",  "",      ParamSpec::Kind::Bool },
            { "phaserType",     "Type",     "TYPE",  ParamSpec::Kind::Choice, {}, 0.0f, { "Phaser", "Flanger" } },
            { "phaserRate",     "Rate",     "RATE",  ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.05f, 5.0f, 0.01f, 0.4f), 0.5f, {}, {}, LFOTarget::PhaserRate },
            { "phaserDepth",    "Depth",    "DEPTH", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),        0.7f, {}, {}, LFOTarget::PhaserDepth },
            { "phaserFeedback", "Feedback", "FB",    ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 0.95f, 0.01f),       0.5f, {}, {}, LFOTarget::PhaserFeedback },
            { "phaserMix",      "Mix",      "MIX",   ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),        0.5f, {}, {}, LFOTarget::PhaserMix },
        };
        return m;
    }
}
