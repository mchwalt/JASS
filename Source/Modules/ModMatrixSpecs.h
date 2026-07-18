#pragma once
#include "ModuleSpec.h"

// MOD MATRIX — four routing rows (SRC combo · DEST combo · AMT knob) built in a loop. SRC/DEST
// item order MUST match the ModSource / LFOTarget order (append-only). Amount is bipolar.
namespace Modules
{
    inline ModuleSpec modMatrix()
    {
        ModuleSpec m;
        m.id = "modmatrix"; m.title = "MOD MATRIX"; m.persistObject = "ModMatrix"; m.enableParamId = "modMatrixOn";
        m.type = rack::ModuleType::Modulator; m.zone = rack::Zone::Modulation; m.size = rack::SizeClass::W12H2;

        const juce::StringArray src { "LFO 1", "Envelope", "Velocity", "LFO 2" };            // == ModSource
        const juce::StringArray tgt { "Off", "Pitch", "Amplitude", "Cutoff", "WT Pos", "Vowel", "Resonance", "Wavefold" };   // == LFOTarget

        m.params.push_back ({ "modMatrixOn", "On", "", ParamSpec::Kind::Bool, {}, 1.0f });   // default on
        for (int n = 1; n <= 4; ++n)   // kNumSlots (ModMatrixConfig::kNumSlots)
        {
            const juce::String s = "modSlot" + juce::String (n);
            m.params.push_back ({ s + "Source", "Source", "SRC",  ParamSpec::Kind::Choice, {}, 0.0f, src });
            m.params.push_back ({ s + "Target", "Target", "DEST", ParamSpec::Kind::Choice, {}, 0.0f, tgt });
            m.params.push_back ({ s + "Amount", "Amount", "AMT",  ParamSpec::Kind::Float, juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f });
        }
        return m;
    }
}
