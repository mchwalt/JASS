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
        // Story 7.3: default-hidden. Enabled in NONE of the 17 presets we ship or keep locally, and
        // rack height is a hard budget (see the fit-scale floor in PluginEditor). Nothing is lost:
        // "hidden ⇒ silent" is already the invariant, and revealEnabledModules() brings it straight
        // back the moment a preset switches it on. Show it any time via the MODULES panel.
        m.defaultVisible = false;
        m.params = {
            { "subOn",     "Enabled",  "",      ParamSpec::Kind::Bool },
            { "subWave",   "Waveform", "WAVE",  ParamSpec::Kind::Choice, {}, 0.0f, { "Sine", "Square" } },
            { "subOctave", "Octave",   "",      ParamSpec::Kind::Choice, {}, 0.0f, { "-1 Oct", "-2 Oct" } },
            { "subFeedback","Feedback","FB",    ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, {}, {}, LFOTarget::SubFeedback },   // Self-FM depth, same scale as the OSC FB knob
            { "subLevel",  "Amp",      "AMP",   ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f, {}, {}, LFOTarget::SubLevel },   // renamed from Level/LEVEL (generator standard); APVTS id stays subLevel
            { "subPan",    "Pan",      "PAN",   ParamSpec::Kind::Float, juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f, {}, {}, LFOTarget::SubPan },   // Epic 10: stereo placement + auto-pan target. AMP·PAN grouped last (rack-wide convention)
        };
        m.params[2].showInBody = false;   // subOctave: APVTS param without a rack control (as today)
        m.params[4].legacyPersistKey = "Level";   // subLevel: pre-2026-08 presets store Sub.Level; still read
        return m;
    }
}
