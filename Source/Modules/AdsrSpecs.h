#pragma once
#include "ModuleSpec.h"

// ENVELOPE - ADSR — the editor appends the real EnvelopeDisplay curve (a Display body element it
// owns) after the 4 knobs. adsrOn default TRUE. Time range/skew verbatim from Parameters.h.
namespace Modules
{
    inline ModuleSpec adsr()
    {
        const juce::NormalisableRange<float> timeRange (0.001f, 5.0f, 0.001f, 0.4f);
        ModuleSpec m;
        m.id = "envelopeadsr"; m.title = "ENVELOPE - ADSR"; m.persistObject = "Adsr"; m.enableParamId = "adsrOn";
        m.type = rack::ModuleType::Modulator; m.zone = rack::Zone::Modulation; m.size = rack::SizeClass::W4H2;
        // Snappier defaults (user 2026-07-19): fast attack, short release so a released note
        // stops promptly instead of ringing ~1 s. (Longer values still available on the knobs.)
        m.params = {
            { "adsrOn",   "Enabled", "",    ParamSpec::Kind::Bool, {}, 1.0f },   // default ON
            { "attack",   "Attack",  "ATK", ParamSpec::Kind::Float, timeRange, 0.01f },
            { "decay",    "Decay",   "DEC", ParamSpec::Kind::Float, timeRange, 0.3f },
            { "sustain",  "Sustain", "SUS", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.7f },
            { "release",  "Release", "REL", ParamSpec::Kind::Float, timeRange, 0.25f },
        };
        return m;
    }
}
