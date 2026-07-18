#pragma once
#include "ModuleSpec.h"
#include "../DSP/SyncDivision.h"   // SyncDivision::kNames for the SYNC combo

// DELAY — TIME + SYNC (Tempo-Sync note division) + FB + MIX. Body order matches the descriptor
// (TIME, SYNC, FB, MIX). Values verbatim from Parameters.h.
namespace Modules
{
    inline ModuleSpec delay()
    {
        ModuleSpec m;
        m.id = "delay"; m.title = "DELAY"; m.persistObject = "Delay"; m.enableParamId = "delayOn";
        m.type = rack::ModuleType::Processor; m.zone = rack::Zone::Processing; m.size = rack::SizeClass::W5H1;
        m.params = {
            { "delayOn",       "Enabled",  "",     ParamSpec::Kind::Bool },
            { "delayTime",     "Time",     "TIME", ParamSpec::Kind::Float,  juce::NormalisableRange<float> (0.01f, 2.0f, 0.01f),  0.3f },
            { "delaySyncDiv",  "SyncDiv",  "SYNC", ParamSpec::Kind::Choice, {}, 0.0f, SyncDivision::kNames },
            { "delayFeedback", "Feedback", "FB",   ParamSpec::Kind::Float,  juce::NormalisableRange<float> (0.0f, 0.95f, 0.01f),  0.4f },
            { "delayMix",      "Mix",      "MIX",  ParamSpec::Kind::Float,  juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),   0.3f },
        };
        return m;
    }
}
