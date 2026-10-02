#pragma once
#include "ModuleSpec.h"

// WAVEFOLD — West-Coast wavefolder. Values verbatim from Parameters.h.
namespace Modules
{
    inline ModuleSpec wavefold()
    {
        ModuleSpec m;
        m.id = "wavefold"; m.title = "WAVEFOLD"; m.persistObject = "Wavefold"; m.enableParamId = "wavefoldOn";
        m.defaultVisible = false;   // hidden until switched on: the stock rack is the Init set (maintainer 2026-10-03)
        m.type = rack::ModuleType::Processor; m.zone = rack::Zone::Processing; m.size = rack::SizeClass::W3H1;
        m.params = {
            { "wavefoldOn",       "Enabled",  "",      ParamSpec::Kind::Bool },
            { "wavefoldDrive",    "Drive",    "DRIVE", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),  0.3f, {}, {}, LFOTarget::WavefolderDrive },
            { "wavefoldSymmetry", "Symmetry", "SYM",   ParamSpec::Kind::Float, juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f, {}, {}, LFOTarget::WavefolderSym },
            { "wavefoldMix",      "Mix",      "MIX",   ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),  1.0f, {}, {}, LFOTarget::WavefolderMix },
        };
        return m;
    }
}
