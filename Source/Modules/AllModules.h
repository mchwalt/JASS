#pragma once
#include "ModuleSpec.h"

// UI-side aggregation: pulls every per-component header and lists all spec-driven modules. Used by
// the editor (to build rack descriptors) and by ModuleRegistry.cpp (to add APVTS params). Add a
// module here + its <Name>Specs.h include as it is migrated off the hand-written code.
#include "FilterSpecs.h"

namespace Modules
{
    inline std::vector<ModuleSpec> all()
    {
        return {
            filter(),
        };
    }
}
