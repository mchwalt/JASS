#pragma once
#include "ModuleSpec.h"

// MASTER — output volume + the global Sync Tempo (BPM) that drives Tempo-Sync (host BPM overrides
// in a DAW). masterOn default TRUE. Values verbatim from Parameters.h.
namespace Modules
{
    inline ModuleSpec master()
    {
        ModuleSpec m;
        m.id = "master"; m.title = "MASTER"; m.persistObject = "Master"; m.enableParamId = "masterOn";
        m.type = rack::ModuleType::Processor; m.zone = rack::Zone::MasterBus; m.size = rack::SizeClass::W3H1;
        m.alignRight = true;   // MASTER BUS: hug the right edge (PRESETS holds the left)
        m.params = {
            { "masterOn",  "On",     "",      ParamSpec::Kind::Bool,  {}, 1.0f },   // default ON
            { "masterVol", "Volume", "VOL",   ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),   0.5f,   {}, {}, LFOTarget::MasterVol },
            { "syncTempo", "Tempo",  "TEMPO", ParamSpec::Kind::Float, juce::NormalisableRange<float> (40.0f, 250.0f, 1.0f), 130.0f, {}, {}, LFOTarget::MasterTempo },
        };
        return m;
    }
}
