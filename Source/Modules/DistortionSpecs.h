#pragma once
#include "ModuleSpec.h"

// DISTORTION — TYPE display labels ("Soft Clip"…) differ from the canonical param/.synthy strings
// ("SoftClip"…), so displayChoices carries the pretty labels while choices stays canonical.
namespace Modules
{
    inline ModuleSpec distortion()
    {
        ModuleSpec m;
        m.id = "distortion"; m.title = "DISTORTION"; m.persistObject = "Distortion"; m.enableParamId = "distortionOn";
        m.type = rack::ModuleType::Processor; m.zone = rack::Zone::Processing; m.size = rack::SizeClass::W4H1;
        m.params = {
            { "distortionOn",    "Enabled", "",      ParamSpec::Kind::Bool },
            { "distortionType",  "Type",    "TYPE",  ParamSpec::Kind::Choice, {}, 0.0f, { "SoftClip", "HardClip", "Foldback" }, { "Soft Clip", "Hard Clip", "Foldback" } },
            { "distortionDrive", "Drive",   "DRIVE", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f, {}, {}, LFOTarget::DistortionDrive },
            { "distortionMix",   "Mix",     "MIX",   ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f },
        };
        return m;
    }
}
