#pragma once
#include "ModuleSpec.h"

// Display-only modules (OSCILLOSCOPE, SPECTRUM, KEYBOARD): a single enable param, no other params.
// The editor injects the actual Display component (which it owns) + any onReset after building the
// descriptor from the spec. Enables default TRUE.
namespace Modules
{
    inline ModuleSpec oscilloscope()
    {
        ModuleSpec m;
        m.id = "oscilloscope"; m.title = "OSCILLOSCOPE"; m.persistObject = "Scope"; m.enableParamId = "scopeOn";
        m.type = rack::ModuleType::Processor; m.zone = rack::Zone::Visualization; m.size = rack::SizeClass::W12H2;
        m.params = { { "scopeOn", "Enabled", "", ParamSpec::Kind::Bool, {}, 1.0f } };
        return m;
    }

    inline ModuleSpec spectrum()
    {
        ModuleSpec m;
        m.id = "spectrum"; m.title = "SPECTRUM"; m.persistObject = "Spectrum"; m.enableParamId = "spectrumOn";
        m.type = rack::ModuleType::Processor; m.zone = rack::Zone::Visualization; m.size = rack::SizeClass::W12H2;
        m.params = { { "spectrumOn", "Enabled", "", ParamSpec::Kind::Bool, {}, 1.0f } };
        return m;
    }

    inline ModuleSpec keyboard()
    {
        ModuleSpec m;
        m.id = "keyboard"; m.title = "KEYBOARD"; m.persistObject = "Keyboard"; m.enableParamId = "keyboardOn";
        m.type = rack::ModuleType::Generator; m.zone = rack::Zone::Input; m.size = rack::SizeClass::W24H1;
        m.params = { { "keyboardOn", "Enabled", "", ParamSpec::Kind::Bool, {}, 1.0f } };
        return m;
    }
}
