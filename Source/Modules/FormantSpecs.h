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
        // Defaults tuned so the vowel effect is instantly recognisable when enabled (user 2026-07-19):
        // VOWEL 0.5 = "I"/"ee" (bright, distinct formants), RESO 0.7 (pronounced), MIX 0.8 (mostly
        // wet but keeps some dry so it never goes fully silent on thin/sine input).
        m.params = {
            { "formantOn",    "Enabled",   "",      ParamSpec::Kind::Bool },
            { "formantVowel", "Vowel",     "VOWEL", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f, {}, {}, LFOTarget::FormantVowel },
            { "formantReso",  "Resonance", "RESO",  ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.7f },
            { "formantMix",   "Mix",       "MIX",   ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.8f },
        };
        return m;
    }
}
