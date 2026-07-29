#pragma once
#include "ModuleSpec.h"

// STEREO — pseudo-stereo master stage (mono engine → stereo). Values verbatim from Parameters.h.
namespace Modules
{
    inline ModuleSpec stereo()
    {
        ModuleSpec m;
        m.id = "stereo"; m.title = "STEREO"; m.persistObject = "Stereo"; m.enableParamId = "stereoOn";
        m.type = rack::ModuleType::Processor; m.zone = rack::Zone::MasterBus; m.size = rack::SizeClass::W5H1;
        m.alignRight = true;   // MASTER BUS: hug the right edge (PRESETS holds the left)
        m.params = {
            { "stereoOn",    "Enabled", "",      ParamSpec::Kind::Bool, {}, 1.0f },   // default ON (factory: pseudo-stereo on)
            // Global OUTPUT MODE (Epic 10). Mono = the raw mono sum to both channels; Pseudo-Stereo (default,
            // unchanged) = the Haas WIDTH/TIME widener below; Stereo-Pan = per-generator PAN into true L/R.
            // WIDTH/TIME only apply in Pseudo-Stereo. Append-only (Surround/Binaural appended later).
            { "outputMode",  "Mode",    "MODE",  ParamSpec::Kind::Choice, {}, 1.0f, { "Mono", "Pseudo-Stereo", "Stereo-Pan", "Binaural", "Kunstkopf (HRTF)" } },   // Binaural = parametric 3D on headphones; Kunstkopf = measured MIT-KEMAR HRIR convolution
            { "stereoWidth", "Width",   "WIDTH", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),  0.5f, {}, {}, LFOTarget::StereoWidth },
            { "stereoTime",  "Time",    "TIME",  ParamSpec::Kind::Float, juce::NormalisableRange<float> (1.0f, 15.0f, 0.1f), 12.0f, {}, {}, LFOTarget::StereoTime },
            // Kunstkopf externalization (Story 10.4): binaural early-reflection amount, only read in
            // Kunstkopf mode (BinauralRoom on the bus). Append-only, default 0 = dry/previous
            // behaviour — existing presets and the factory state sound exactly as before (format
            // stays v6). Deliberately NOT a mod-matrix target (global bus param, story AC7).
            { "hrtfRoom",    "Room",    "ROOM",  ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),  0.0f },
        };
        return m;
    }
}
