#pragma once
#include "ModuleSpec.h"

// COMPRESSOR — master-bus glue (runs on the summed mix in processBlock). Values verbatim from
// Parameters.h; ids match Parameters::ID::comp* (DSP/PresetIO still read those).
namespace Modules
{
    inline ModuleSpec compressor()
    {
        ModuleSpec m;
        m.id = "compressor"; m.title = "COMPRESSOR"; m.persistObject = "Compressor"; m.enableParamId = "compOn";
        m.type = rack::ModuleType::Processor; m.zone = rack::Zone::MasterBus; m.size = rack::SizeClass::W8H1;
        m.defaultVisible = false;   // hidden by default (show via MODULES menu) — keeps the MASTER BUS row compact
        m.alignRight = true;        // MASTER BUS: hug the right edge (PRESETS holds the left)
        m.params = {
            { "compOn",        "Enabled",   "",       ParamSpec::Kind::Bool },
            { "compThreshold", "Threshold", "THRESH", ParamSpec::Kind::Float, juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f),        -18.0f },
            { "compRatio",     "Ratio",     "RATIO",  ParamSpec::Kind::Float, juce::NormalisableRange<float> (1.0f, 20.0f, 0.1f, 0.5f),    2.0f },
            { "compAttack",    "Attack",    "ATK",    ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.1f, 100.0f, 0.1f, 0.4f),  10.0f },
            { "compRelease",   "Release",   "REL",    ParamSpec::Kind::Float, juce::NormalisableRange<float> (10.0f, 1000.0f, 1.0f, 0.4f), 120.0f },
            { "compMakeup",    "Makeup",    "GAIN",   ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f),          0.0f },
        };
        return m;
    }
}
