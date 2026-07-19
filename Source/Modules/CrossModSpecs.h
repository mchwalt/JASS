#pragma once
#include "ModuleSpec.h"

// CROSS MOD (ex MIX MODE) — RingMod/FM between two selectable OSCs. id stays "mixmode" (layout
// key). mixSrcB defaults to OSC 2 (index 1). The derived lit/dim state (enabledWhen) reads apvts
// atomics, so the EDITOR injects it after makeModuleDescriptor (a static spec can't capture apvts).
namespace Modules
{
    inline ModuleSpec crossmod()
    {
        ModuleSpec m;
        m.id = "mixmode"; m.title = "CROSS MOD"; m.persistObject = "CrossMod"; m.enableParamId = "mixModeOn";
        // CROSS MOD shapes the oscillators (RingMod/FM) — it is a modulator, not a sound source,
        // so it lives in the MODULATION zone (user decision 2026-07-19).
        m.type = rack::ModuleType::Modulator; m.zone = rack::Zone::Modulation; m.size = rack::SizeClass::W5H1;
        m.params = {
            { "mixModeOn", "On",   "",      ParamSpec::Kind::Bool },   // default off => additive
            { "mixMode",   "Mode", "MODE",  ParamSpec::Kind::Choice, {}, 0.0f, { "RingMod", "FM" } },
            // A = modulator, B = carrier: in FM, A modulates B's frequency (B is the DEST). In
            // RingMod (A×B) it is symmetric, so the SRC/DEST naming is just cosmetic there.
            { "mixSrcA",   "SrcA", "SRC",  ParamSpec::Kind::Choice, {}, 0.0f, { "OSC 1", "OSC 2", "OSC 3" } },
            { "mixSrcB",   "SrcB", "DEST", ParamSpec::Kind::Choice, {}, 1.0f, { "OSC 1", "OSC 2", "OSC 3" } },   // default OSC 2
        };
        return m;
    }
}
