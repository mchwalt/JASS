#pragma once
#include "ModuleSpec.h"

// GRAIN (Story 17.1) — granular cloud on the SAMPLER's material. No set selector of its own: the
// SAMPLER's SET is the material (the note picks the zone exactly as the sampler does), so there is
// one loader, one "File" side channel, one place to pick a sound. A texture generator, not a
// pitch-shifter (Story 12.3 measured that road and chose STRETCH for it).
//
// Params are append-only. Matrix targets (GrainPosition/Size/Pitch/Amp) arrive with the target
// table (ModTargets.h) — the modTarget fields below light the knobs' rings once they exist.
namespace Modules
{
    inline ModuleSpec grain()
    {
        ModuleSpec m;
        m.id = "grain"; m.title = "GRAIN"; m.persistObject = "Grain"; m.enableParamId = "grainOn";
        m.type = rack::ModuleType::Generator; m.zone = rack::Zone::Generators; m.size = rack::SizeClass::W12H1;
        m.defaultVisible = false;   // the rack is full; shown from the rack menu like PERC/CHAOS
        m.params = {
            { "grainOn",      "Enabled",  "",      ParamSpec::Kind::Bool, {}, 0.0f },
            // WHERE: centre of the grain start positions, as a fraction of the zone (like START/END).
            { "grainPos",     "Position", "POS",   ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f },
            // Random spread of the start position around POS, fraction of the zone. 0 = every grain
            // reads the same spot (stutter; at high DENS the density becomes the pitch).
            { "grainSpray",   "Spray",    "SPRAY", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.1f },
            // HOW LONG: 5 ms reads as metallic buzz, 300 ms as an echo-like smear. Log-skewed.
            { "grainSize",    "Size",     "SIZE",  ParamSpec::Kind::Float, juce::NormalisableRange<float> (5.0f, 300.0f, 0.1f, 0.4f), 60.0f },
            // Grains per second. Overlap = SIZE · DENS; the engine holds 32 grains at once and drops
            // the rest (never steals), so DENS saturates at 32 / SIZE. Log-skewed.
            { "grainDensity", "Density",  "DENS",  ParamSpec::Kind::Float, juce::NormalisableRange<float> (1.0f, 200.0f, 0.1f, 0.35f), 20.0f },
            // HOW FAR: random per-grain transposition spread in semitones (± this). With QUANT on,
            // grains land on scale degrees relative to the played note, every degree equally likely.
            { "grainPitch",   "Pitch",    "PITCH", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 24.0f, 1.0f), 0.0f },
            // Same five choices, same order, as the MOD MATRIX's QUANT (ModMatrixSpecs.h) — the
            // index IS the quant code the engine reads (0 Off … 4 Penta).
            { "grainQuant",   "Quant",    "QUANT", ParamSpec::Kind::Choice, {}, 0.0f, { "Off", "Chrom", "Major", "Minor", "Penta" } },
            { "grainAmp",     "Amp",      "AMP",   ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f },
            { "grainPan",     "Pan",      "PAN",   ParamSpec::Kind::Float, juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f },
        };
        return m;
    }
}
