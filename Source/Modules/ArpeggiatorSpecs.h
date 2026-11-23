#pragma once
#include "ModuleSpec.h"
#include "../DSP/SyncDivision.h"   // SyncDivision::kNames for the SYNC combo

// ARPEGGIATOR. Body: MODE, SYNC, RATE, OCT, GATE (bodyOrder; SYNC is appended in registration).
namespace Modules
{
    inline ModuleSpec arpeggiator()
    {
        ModuleSpec m;
        m.id = "arpeggiator"; m.title = "ARPEGGIATOR"; m.persistObject = "Arp"; m.enableParamId = "arpOn";
        m.defaultVisible = false;   // hidden until switched on: the stock rack is the Init set (maintainer 2026-10-03)
        m.layered = true;           // Story 21.8: each layer has its own ARP, which plays that layer
        m.type = rack::ModuleType::Modulator; m.zone = rack::Zone::Modulation; m.size = rack::SizeClass::W8H1;   // W6 until SYNC (2 slots) came, 2026-11-23
        m.params = {
            { "arpOn",      "Enabled", "",     ParamSpec::Kind::Bool },
            { "arpMode",    "Mode",    "MODE", ParamSpec::Kind::Choice, {}, 0.0f, { "Up", "Down", "UpDown", "Random" } },
            { "arpRate",    "Rate",    "RATE", ParamSpec::Kind::Float, juce::NormalisableRange<float> (1.0f, 16.0f, 0.1f), 8.0f },
            { "arpOctaves", "Octaves", "OCT",  ParamSpec::Kind::Int,   juce::NormalisableRange<float> (1.0f, 4.0f, 1.0f),  2.0f },
            { "arpGate",    "Gate",    "GATE", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.05f, 1.0f, 0.01f), 0.5f },
            // (arpTarget, Story 21.7, sat here: since 21.8 the layer is the target.)
            // SYNC (Story 21.9, appended): the step as a note value against the tempo (MASTER, or the
            // host's), like STEP SEQ, PERC, the LFOs and DELAY; Free = RATE in steps/s. Default Free,
            // so every existing preset plays as before. Fed VERBATIM from SyncDivision::kNames.
            { "arpSync",    "SyncDiv", "SYNC", ParamSpec::Kind::Choice, {}, 0.0f, SyncDivision::kNames },
        };
        m.bodyOrder = { "arpMode", "arpSync", "arpRate", "arpOctaves", "arpGate" };
        return m;
    }
}
