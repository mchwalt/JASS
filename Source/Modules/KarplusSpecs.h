#pragma once
#include "ModuleSpec.h"

// STRING - KARPLUS — plucked-string model. The PLUCK Action button is injected by the editor at
// the FRONT of the body (it captures the processor). Body params order: FREQ, AMP, DAMP, STR.
namespace Modules
{
    inline ModuleSpec string()
    {
        ModuleSpec m;
        m.id = "stringkarplus"; m.title = "STRING - KARPLUS"; m.persistObject = "Karplus"; m.enableParamId = "karplusOn";
        m.type = rack::ModuleType::Generator; m.zone = rack::Zone::Generators; m.size = rack::SizeClass::W6H1;
        m.params = {
            { "karplusOn",      "Enabled",   "",     ParamSpec::Kind::Bool },
            { "karplusFreq",    "Frequency", "FREQ", ParamSpec::Kind::Float, juce::NormalisableRange<float> (20.0f, 2000.0f, 1.0f, 0.3f), 261.63f },
            { "karplusAmp",     "Amplitude", "AMP",  ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f, {}, {}, LFOTarget::KarplusAmp },
            { "karplusDamping", "Damping",   "DAMP", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f, {}, {}, LFOTarget::KarplusDamping },
            { "karplusStretch", "Stretch",   "STR",  ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f, {}, {}, LFOTarget::KarplusStretch },
            { "karplusPan",     "Pan",       "PAN",  ParamSpec::Kind::Float, juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f },   // Epic 10: stereo placement
        };
        return m;
    }
}
