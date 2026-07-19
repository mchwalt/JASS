#pragma once
#include "ModuleSpec.h"
#include "../DSP/ModMatrix.h"   // ModMatrixConfig::kNumSlots

// MOD MATRIX — six routing slots (SRC combo · DEST combo · AMT knob) built in a loop, laid out
// 2 rows × 3 slots (W16H2: each slot = 5 layout units, 2 rows => ceil(30/2)=15 cols => 3 slots/row).
// SRC/DEST item order MUST match the ModSource / LFOTarget order (append-only). Amount is bipolar.
namespace Modules
{
    inline ModuleSpec modMatrix()
    {
        ModuleSpec m;
        m.id = "modmatrix"; m.title = "MOD MATRIX"; m.persistObject = "ModMatrix"; m.enableParamId = "modMatrixOn";
        m.type = rack::ModuleType::Modulator; m.zone = rack::Zone::Modulation; m.size = rack::SizeClass::W16H2;

        const juce::StringArray src { "LFO 1", "Envelope", "Velocity", "LFO 2", "LFO 3", "LFO 4" };   // == ModSource
        const juce::StringArray tgt { "Off", "Pitch", "Amplitude", "Cutoff", "WT Pos", "Vowel", "Resonance", "Wavefold" };   // == LFOTarget

        m.params.push_back ({ "modMatrixOn", "On", "", ParamSpec::Kind::Bool, {}, 1.0f });   // default on
        for (int n = 1; n <= ModMatrixConfig::kNumSlots; ++n)
        {
            const juce::String s = "modSlot" + juce::String (n);
            const juce::String k = "Slot" + juce::String (n);   // unique persist keys within the ModMatrix object
            m.params.push_back ({ s + "Source", k + "Source", "SRC",  ParamSpec::Kind::Choice, {}, 0.0f, src });
            m.params.push_back ({ s + "Target", k + "Target", "DEST", ParamSpec::Kind::Choice, {}, 0.0f, tgt });
            m.params.push_back ({ s + "Amount", k + "Amount", "AMT",  ParamSpec::Kind::Float, juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.5f });   // default +0.5: a freshly-routed slot modulates audibly (0 == off)
        }
        return m;
    }
}
