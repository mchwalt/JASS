#pragma once
#include "ModuleSpec.h"

// PRESETS quick-access bank (MASTER BUS): 12 slots F1..F12, each assignable to a named preset.
// Like the display modules (OSCILLOSCOPE/SPECTRUM/KEYBOARD) it carries only a single enable Bool
// so it fits the uniform module anatomy; the actual UI is a custom PresetBankPanel that the editor
// owns and injects as a Display. The 12 assignments live in a GLOBAL app file (PresetBanks.json,
// see PresetIO), NOT in the preset — so there are no per-slot APVTS params here. Enable default TRUE.
namespace Modules
{
    inline ModuleSpec presetBank()
    {
        ModuleSpec m;
        m.id = "presets"; m.title = "PRESETS"; m.persistObject = "PresetBank"; m.enableParamId = "presetBankOn";
        m.type = rack::ModuleType::Processor; m.zone = rack::Zone::MasterBus; m.size = rack::SizeClass::W8H1;
        m.params = { { "presetBankOn", "Enabled", "", ParamSpec::Kind::Bool, {}, 1.0f } };
        return m;
    }
}
