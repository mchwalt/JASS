#pragma once
#include "ModuleSpec.h"

// FORMANT — vowel filter (VOWEL morphs A-E-I-O-U). Values verbatim from Parameters.h.
namespace Modules
{
    inline ModuleSpec formant()
    {
        ModuleSpec m;
        m.id = "formant"; m.title = "FORMANT"; m.persistObject = "Formant"; m.enableParamId = "formantOn";
        m.type = rack::ModuleType::Processor; m.zone = rack::Zone::Processing; m.size = rack::SizeClass::W3H1;
        m.params = {
            { "formantOn",    "Enabled",   "",      ParamSpec::Kind::Bool },
            { "formantVowel", "Vowel",     "VOWEL", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f, {}, {}, LFOTarget::FormantVowel },
            { "formantReso",  "Resonance", "RESO",  ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f },
            { "formantMix",   "Mix",       "MIX",   ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f },
        };
        return m;
    }
}
