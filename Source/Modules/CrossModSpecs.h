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
        m.type = rack::ModuleType::Generator; m.zone = rack::Zone::Generators; m.size = rack::SizeClass::W5H1;
        m.params = {
            { "mixModeOn", "On",   "",      ParamSpec::Kind::Bool },   // default off => additive
            { "mixMode",   "Mode", "MODE",  ParamSpec::Kind::Choice, {}, 0.0f, { "RingMod", "FM" } },
            { "mixSrcA",   "SrcA", "SRC A", ParamSpec::Kind::Choice, {}, 0.0f, { "OSC 1", "OSC 2", "OSC 3" } },
            { "mixSrcB",   "SrcB", "SRC B", ParamSpec::Kind::Choice, {}, 1.0f, { "OSC 1", "OSC 2", "OSC 3" } },   // default OSC 2
        };
        return m;
    }
}
