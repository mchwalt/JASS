#pragma once
#include "ModuleSpec.h"

// CHAOS — a Lorenz attractor as a pure MOD MATRIX source (Chaos X / Chaos Y), the
// stepped-and-chaotic sibling of the LFOs. No built-in target, no audio: route it
// in the matrix. Free-running and global (one orbit for all voices) — see
// DSP/ChaosLorenz.h for the reasoning.
namespace Modules
{
    inline ModuleSpec chaos()
    {
        ModuleSpec m;
        m.id = "chaos"; m.title = "CHAOS"; m.persistObject = "Chaos"; m.enableParamId = "chaosOn";
        m.type = rack::ModuleType::Modulator; m.zone = rack::Zone::Modulation; m.size = rack::SizeClass::W3H1;
        m.defaultVisible = false;   // used by no preset — same reasoning as GLIDE (Story 7.3)
        m.params = {
            { "chaosOn",   "Enabled", "",     ParamSpec::Kind::Bool },
            { "chaosRate", "Rate",    "RATE", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.01f, 10.0f, 0.01f, 0.4f), 1.0f },
        };
        return m;
    }
}
