#pragma once
#include "ModuleSpec.h"

// PITCH ENV — one-shot pitch sweep at note-on. Body order matches the descriptor (AMOUNT, TIME).
namespace Modules
{
    inline ModuleSpec pitchEnv()
    {
        ModuleSpec m;
        m.id = "pitchenv"; m.title = "PITCH ENV"; m.persistObject = "PitchEnv"; m.enableParamId = "pitchEnvOn";
        m.type = rack::ModuleType::Modulator; m.zone = rack::Zone::Modulation; m.size = rack::SizeClass::W3H1;
        m.defaultVisible = false;   // Story 7.3: used by no preset — see SubSpecs.h for the reasoning
        m.params = {
            { "pitchEnvOn",     "Enabled", "",       ParamSpec::Kind::Bool },
            { "pitchEnvAmount", "Amount",  "AMOUNT", ParamSpec::Kind::Float, juce::NormalisableRange<float> (-48.0f, 48.0f, 0.1f),        0.0f, {}, {}, LFOTarget::PitchEnvAmount },
            { "pitchEnvTime",   "Time",    "TIME",   ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.005f, 2.0f, 0.001f, 0.4f), 0.3f },
        };
        return m;
    }
}
