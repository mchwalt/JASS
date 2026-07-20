#pragma once
#include "ModuleSpec.h"
#include "../DSP/SyncDivision.h"   // SyncDivision::kNames for the SYNC combo

// DELAY — SYNC (Tempo-Sync note division) + TIME + FB + MIX. The SYNC combo leads (leftmost),
// then the TIME knob it gates, then FB + MIX. Values verbatim from Parameters.h. (Body order is
// display-only; params are keyed by id, so reordering is safe for APVTS/preset round-trips.)
namespace Modules
{
    inline ModuleSpec delay()
    {
        ModuleSpec m;
        m.id = "delay"; m.title = "DELAY"; m.persistObject = "Delay"; m.enableParamId = "delayOn";
        m.type = rack::ModuleType::Processor; m.zone = rack::Zone::Processing; m.size = rack::SizeClass::W5H1;
        m.params = {
            { "delayOn",       "Enabled",  "",     ParamSpec::Kind::Bool },
            { "delaySyncDiv",  "SyncDiv",  "SYNC", ParamSpec::Kind::Choice, {}, 0.0f, SyncDivision::kNames },
            { "delayTime",     "Time",     "TIME", ParamSpec::Kind::Float,  juce::NormalisableRange<float> (0.01f, 2.0f, 0.01f),  0.3f },
            { "delayFeedback", "Feedback", "FB",   ParamSpec::Kind::Float,  juce::NormalisableRange<float> (0.0f, 0.95f, 0.01f),  0.4f },
            { "delayMix",      "Mix",      "MIX",  ParamSpec::Kind::Float,  juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),   0.3f },
        };
        return m;
    }
}
