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
        m.defaultVisible = false;   // hidden until switched on: the stock rack is the Init set (maintainer 2026-10-03; LFO 1..3 were visible before)
        m.params = {
            { p + "On",      "Enabled",  "",       ParamSpec::Kind::Bool },
            { p + "Wave",    "Waveform", "WAVE",   ParamSpec::Kind::Choice, {}, 0.0f, { "Sine", "Triangle", "Square", "Sawtooth", "S&H", "One-Shot" } },   // append-only (index == LFOWaveform)
            // TARGET is now INTERNAL (no UI): the LFO is a pure MOD MATRIX source — route it there
            // instead. The param is kept (hidden) only to gate the LFO on/off and to migrate old
            // presets' built-in routings into matrix slots. See PresetIO::convertOldPresets.
            { p + "Target",  "Target",   "",       ParamSpec::Kind::Choice, {}, 0.0f, { "Frequency", "Amplitude", "Filter Cutoff", "Wavetable Pos", "Formant Vowel", "Filter Reso", "Wavefold Drive" } },
            { p + "SyncDiv", "SyncDiv",  "SYNC",   ParamSpec::Kind::Choice, {}, 0.0f, SyncDivision::kNames },
            // RATE floor 0.005 Hz (a 200 s period) since Story 20.1, was 0.1 Hz: a GLOBAL LFO on the
            // filter is a sweep of minutes, not a vibrato. Skew 0.3 keeps 2 Hz at mid-travel and the
            // old 0.1..20 Hz stretch on the upper ~75 % of the knob; the slow end reads as seconds.
            { p + "Rate",    "Rate",     "RATE",   ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.005f, 20.0f, 0.001f, 0.3f), 2.0f },
            { p + "Depth",   "Depth",    "DEPTH",  ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f },
            // GLOBAL (Story 20.1, appended): one phase for all voices, owned by the processor and
            // never restarted by a key — the CHAOS rule applied to an LFO. Off = the per-voice LFO
            // of old (retriggered on note-on), so every existing preset sounds as before.
            { p + "Global",  "Global",   "GLOBAL", ParamSpec::Kind::Bool, {}, 0.0f },
        };
        m.params[2].showInBody = false;   // TARGET: internal only, no rack control
        m.params[2].automatable = false;  // ...and no automation lane in a DAW either
        m.params[4].textFromValue = [] (double hz)   // RATE: below 0.1 Hz a period is the readable unit
        {
            if (hz > 0.0 && hz < 0.1) return juce::String (juce::roundToInt (1.0 / hz)) + " s";
            return juce::String (hz, hz < 1.0 ? 3 : 2) + " Hz";
        };
        return m;
    }
}
