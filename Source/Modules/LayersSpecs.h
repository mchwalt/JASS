#pragma once
#include "ModuleSpec.h"

// LAYERS (AD-16, Story 21.5) — the instrument-level switches of the bitimbral JASS: layer B on,
// LAYER or SPLIT, the split point, which layer the rack edits, and level + balance per layer.
// Not a rack module: the editor builds its rack from explicit addRackModule calls, and S2 puts
// these controls into the header. Defaults are the pre-AD-16 instrument (B off, A at unity).
namespace Modules
{
    inline ModuleSpec layers()
    {
        ModuleSpec m;
        m.id = "layers"; m.title = "LAYERS"; m.persistObject = "Layers"; m.enableParamId = "layerBOn";
        m.type = rack::ModuleType::Processor; m.zone = rack::Zone::MasterBus; m.size = rack::SizeClass::W6H1;
        m.defaultVisible = false;
        m.params = {
            { "layerBOn",    "BOn",    "B ON",  ParamSpec::Kind::Bool },
            { "layerMode",   "Mode",   "MODE",  ParamSpec::Kind::Choice, {}, 0.0f, { "Layer", "Split" } },
            // SPLIT: notes BELOW this go to B (the left hand), from it up to A. 48 = C3.
            { "splitNote",   "Split",  "SPLIT", ParamSpec::Kind::Int,   juce::NormalisableRange<float> (0.0f, 127.0f, 1.0f), 48.0f },
            // Which layer the rack edits (S2). A view choice, not sound: no automation lane.
            { "layerFocus",  "Focus",  "EDIT",  ParamSpec::Kind::Choice, {}, 0.0f, { "A", "B" } },
            { "layerALevel", "ALevel", "A LVL", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f },
            { "layerBLevel", "BLevel", "B LVL", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f },
            // Balance, not a mono pan: a layer is already stereo. 0 = both channels at unity.
            { "layerAPan",   "APan",   "A PAN", ParamSpec::Kind::Float, juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f },
            { "layerBPan",   "BPan",   "B PAN", ParamSpec::Kind::Float, juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f },
        };
        m.params[3].automatable = false;   // layerFocus
        return m;
    }
}
