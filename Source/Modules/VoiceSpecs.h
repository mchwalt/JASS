#pragma once
#include "ModuleSpec.h"

// VOICE (Story 14.1) — the per-voice error that makes polyphony sound like an instrument
// instead of one waveform played several times. HUMANIZE draws a small offset per NOTE (fixed
// while it sounds); DRIFT is a slow, free-running movement per VOICE. Both are also matrix
// sources (ModSource::VoiceRandom / VoiceDrift), so they can reach any target — what the knobs
// wire up directly is only pitch and oscillator start phase (design decision D2).
// Both default to 0 = off, so an existing patch is bit-identical until a knob is turned.
namespace Modules
{
    inline ModuleSpec voice()
    {
        ModuleSpec m;
        m.id = "voice"; m.title = "VOICE"; m.persistObject = "Voice"; m.enableParamId = "voiceOn";
        m.type = rack::ModuleType::Modulator; m.zone = rack::Zone::Modulation; m.size = rack::SizeClass::W3H1;
        m.params = {
            { "voiceOn",       "Enabled",  "",         ParamSpec::Kind::Bool },
            // Per-note draw. 1.0 = ±8 cents of detune and a fully randomised start phase (D3) —
            // real analogue spread is small, more just sounds out of tune.
            { "voiceHumanize", "Humanize", "HUMANIZE", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f },
            // Slow per-voice movement. The rate is NOT a knob (D4): each voice picks its own
            // between 0.05 and 0.5 Hz, so voices never lock into a common vibrato.
            { "voiceDrift",    "Drift",    "DRIFT",    ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f },
        };
        return m;
    }
}
