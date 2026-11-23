#pragma once
#include "ModuleSpec.h"

// LAYERS (AD-16, Stories 21.5/21.6) — the instrument-level switches of the bitimbral JASS: split
// on / off (layerMode), the split point, which layer the rack edits and hears, and level + balance
// per layer. The editor hand-builds the LAYERS rack module (MASTER BUS zone) from these params.
// Defaults are the pre-AD-16 instrument: Single on A, A at unity.
namespace Modules
{
    inline ModuleSpec layers()
    {
        ModuleSpec m;
        m.id = "layers"; m.title = "LAYERS"; m.persistObject = "Layers";
        m.type = rack::ModuleType::Processor; m.zone = rack::Zone::MasterBus; m.size = rack::SizeClass::W6H1;
        m.defaultVisible = false;
        m.params = {
            // Split on / off since 2026-11-23 (the MODE knob is gone; the keyboard's SPLIT button
            // writes 0 or 2): 0 = no split, the A / B selection plays; 2 = SPLIT, below the split
            // key B, from it up A. 1 = the old "Dual", kept so the append-only list keeps its
            // indices — read as no split (a file, and the processor at run time).
            { "layerMode",   "Mode",   "MODE",  ParamSpec::Kind::Choice, {}, 0.0f, { "Single", "Dual", "Split" } },
            // SPLIT: notes BELOW this go to B (the left hand), from it up to A. 48 = C3.
            { "splitNote",   "Split",  "SPLIT", ParamSpec::Kind::Int,   juce::NormalisableRange<float> (0.0f, 127.0f, 1.0f), 48.0f },
            // Which layer the rack edits (S2). A view choice, not sound: no automation lane.
            // "A+B" (21.6, maintainer 2026-11-16): the rack shows A, and every edit is mirrored
            // to B — relative for knobs, so B keeps its offset (linked editing). Append-only.
            { "layerFocus",  "Focus",  "EDIT",  ParamSpec::Kind::Choice, {}, 0.0f, { "A", "B", "A+B" } },
            { "layerALevel", "ALevel", "A LVL", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f },
            { "layerBLevel", "BLevel", "B LVL", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f },
            // Balance, not a mono pan: a layer is already stereo. 0 = both channels at unity.
            { "layerAPan",   "APan",   "A PAN", ParamSpec::Kind::Float, juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f },
            { "layerBPan",   "BPan",   "B PAN", ParamSpec::Kind::Float, juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f },
        };
        m.params[2].automatable = false;   // layerFocus
        return m;
    }
}
