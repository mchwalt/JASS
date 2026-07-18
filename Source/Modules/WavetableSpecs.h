#pragma once
#include "ModuleSpec.h"
#include "../DSP/WavetableBank.h"   // WavetableBankStore::MaxBanks for the BANK int range

// WAVETABLE — params only (the UI is hand-built in the editor because BANK is a dynamic combo and
// LOAD WAV is a file action mid-body). BANK is an Int param shown as a bank-name combo.
namespace Modules
{
    inline ModuleSpec wavetable()
    {
        ModuleSpec m;
        m.id = "wavetable"; m.title = "WAVETABLE"; m.persistObject = "Wavetable"; m.enableParamId = "wavetableOn";
        m.type = rack::ModuleType::Generator; m.zone = rack::Zone::Generators; m.size = rack::SizeClass::W8H1;
        m.params = {
            { "wavetableOn",        "Enabled",      "",       ParamSpec::Kind::Bool },
            { "wavetableBank",      "BankIndex",    "BANK",   ParamSpec::Kind::Int,   juce::NormalisableRange<float> (0.0f, (float) (WavetableBankStore::MaxBanks - 1), 1.0f), 0.0f },
            { "wavetablePosition",  "Position",     "POS",    ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, {}, {}, LFOTarget::WavetablePosition },
            { "wavetableFreq",      "Frequency",    "FREQ",   ParamSpec::Kind::Float, juce::NormalisableRange<float> (20.0f, 10000.0f, 1.0f, 0.3f), 261.63f },
            { "wavetableAmp",       "Amplitude",    "AMP",    ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f },
            { "wavetableUniVoices", "UnisonVoices", "VOICES", ParamSpec::Kind::Int,   juce::NormalisableRange<float> (1.0f, 7.0f, 1.0f), 1.0f },
            { "wavetableUniDetune", "UnisonDetune", "DETUNE", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.2f },
        };
        return m;
    }
}
