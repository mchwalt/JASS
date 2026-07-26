#pragma once
#include "ModuleSpec.h"

// SUB — sub-oscillator (tracks OSC 1 pitch, octave(s) down). NOTE: the Octave choice param exists
// in the APVTS but has NO rack control (showInBody=false) — it matches the current UI (WAVE + LEVEL
// only). Values verbatim from Parameters.h.
namespace Modules
{
    inline ModuleSpec sub()
    {
        ModuleSpec m;
        m.id = "sub"; m.title = "SUB"; m.persistObject = "Sub"; m.enableParamId = "subOn";
        m.type = rack::ModuleType::Generator; m.zone = rack::Zone::Generators; m.size = rack::SizeClass::W4H1;
        m.params = {
            { "subOn",     "Enabled",  "",      ParamSpec::Kind::Bool },
            { "subWave",   "Waveform", "WAVE",  ParamSpec::Kind::Choice, {}, 0.0f, { "Sine", "Square" } },
            { "subOctave", "Octave",   "",      ParamSpec::Kind::Choice, {}, 0.0f, { "-1 Oct", "-2 Oct" } },
            { "subLevel",  "Level",    "LEVEL", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f, {}, {}, LFOTarget::SubLevel },
            { "subPan",    "Pan",      "PAN",   ParamSpec::Kind::Float, juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f },   // Epic 10: stereo placement
        };
        m.params[2].showInBody = false;   // subOctave: APVTS param without a rack control (as today)
        return m;
    }
}
