#pragma once
#include "ModuleSpec.h"
#include "../DSP/SyncDivision.h"   // SyncDivision::kNames for the SYNC combo

// LFO 1..kNumLFOs — indexed factory. LFO 1 keeps the stable id "lfo" (help/layout) and is visible;
// further LFOs are "lfo2"/"lfo3"… and hidden by default. Body order: WAVE, TARGET(hidden), SYNC, RATE,
// DEPTH — selector combos (WAVE, SYNC) first, then knobs (RATE, DEPTH), matching every other module.
namespace Modules
{
    inline ModuleSpec lfo (int i)
    {
        const juce::String p = "lfo" + juce::String (i);
        ModuleSpec m;
        m.id     = (i == 1) ? juce::String ("lfo") : p;   // id "lfo" for LFO 1 (stable layout key)
        m.helpId = "lfo";                                 // LFO 1..4 share ONE help text (lfo.md)
        m.title  = "LFO " + juce::String (i);
        m.persistObject = "Lfo" + juce::String (i);
        m.enableParamId = p + "On";
        // W6, not W7 (Story 7.3, user 2026-08-09): the body is WAVE + SYNC + RATE + DEPTH, which
        // W7 gave more room than it needed. Not just cosmetics — three LFOs one column narrower
        // let the MODULATION zone pack into 3 rows instead of 4 (measured: rack 1608 → 1484 px,
        // fit scale 0.79 → 0.85). W5 packs no better, so 6 is the point where narrowing stops
        // paying and only starts squeezing the two combos.
        m.type = rack::ModuleType::Modulator; m.zone = rack::Zone::Modulation; m.size = rack::SizeClass::W6H1;
        m.defaultVisible = (i <= 3);   // LFO 1..3 visible by default; LFO 4 hidden (show via MODULES)
        m.params = {
            { p + "On",      "Enabled",  "",       ParamSpec::Kind::Bool },
            { p + "Wave",    "Waveform", "WAVE",   ParamSpec::Kind::Choice, {}, 0.0f, { "Sine", "Triangle", "Square", "Sawtooth", "S&H", "One-Shot" } },   // append-only (index == LFOWaveform)
            // TARGET is now INTERNAL (no UI): the LFO is a pure MOD MATRIX source — route it there
            // instead. The param is kept (hidden) only to gate the LFO on/off and to migrate old
            // presets' built-in routings into matrix slots. See PresetIO::convertOldPresets.
            { p + "Target",  "Target",   "",       ParamSpec::Kind::Choice, {}, 0.0f, { "Frequency", "Amplitude", "Filter Cutoff", "Wavetable Pos", "Formant Vowel", "Filter Reso", "Wavefold Drive" } },
            { p + "SyncDiv", "SyncDiv",  "SYNC",   ParamSpec::Kind::Choice, {}, 0.0f, SyncDivision::kNames },
            { p + "Rate",    "Rate",     "RATE",   ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.1f, 20.0f, 0.1f), 2.0f },
            { p + "Depth",   "Depth",    "DEPTH",  ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f },
        };
        m.params[2].showInBody = false;   // TARGET: internal only, no rack control
        return m;
    }
}
