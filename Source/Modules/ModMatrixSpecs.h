#pragma once
#include "ModuleSpec.h"
#include "../DSP/ModMatrix.h"          // ModMatrixConfig::kNumSlots
#include "../DSP/ModMatrixCatalog.h"   // ModDest: MODULE list + per-module PARAM list

// MOD MATRIX — six routing slots (SRC combo · MOD combo · PARAM combo · AMT knob) built in a loop.
// The DEST is chosen in two steps: MODULE (which module) then PARAM (which parameter within it) —
// so "FREQ" reads as "OSC 2 · FREQ", not an abstract global "Pitch". The PARAM combo is DEPENDENT
// on the MODULE selection; the editor swaps its items via a provider + ComboDependency (a static
// spec can't read APVTS). Here PARAM ships placeholder labels only (the editor overrides them).
//
// SRC item order MUST match ModSource; MOD item order MUST match ModDest::modules (both append-only,
// ComboBoxAttachment maps by index). Amount is bipolar. Layout: W24H2 = 3 slots/row × 2 rows.
namespace Modules
{
    inline ModuleSpec modMatrix()
    {
        ModuleSpec m;
        m.id = "modmatrix"; m.title = "MOD MATRIX"; m.persistObject = "ModMatrix"; m.enableParamId = "modMatrixOn";
        m.type = rack::ModuleType::Modulator; m.zone = rack::Zone::Modulation; m.size = rack::SizeClass::W24H2;

        const juce::StringArray src { "LFO 1", "Envelope", "Velocity", "LFO 2", "LFO 3", "LFO 4" };   // == ModSource
        juce::StringArray mod;   // MOD labels — generated from the single source (ModMatrixCatalog.h)
        for (int i = 0; i < ModDest::kNumModules; ++i) mod.add (ModDest::moduleLabel (i));

        m.params.push_back ({ "modMatrixOn", "On", "", ParamSpec::Kind::Bool, {}, 1.0f });   // default on
        for (int n = 1; n <= ModMatrixConfig::kNumSlots; ++n)
        {
            const juce::String s = "modSlot" + juce::String (n);
            const juce::String k = "Slot" + juce::String (n);   // unique persist keys within the ModMatrix object
            m.params.push_back ({ s + "Source", k + "Source", "SRC",   ParamSpec::Kind::Choice, {}, 0.0f, src });
            m.params.push_back ({ s + "Module", k + "Module", "MOD",   ParamSpec::Kind::Choice, {}, 0.0f, mod });
            // PARAM is an INT index into the selected module's param list (persists as a number so it
            // survives per-module label differences). The editor renders it as a dependent combo.
            m.params.push_back ({ s + "Param",  k + "Param",  "PARAM", ParamSpec::Kind::Int, juce::NormalisableRange<float> (0.0f, (float) (ModDest::kMaxParams - 1), 1.0f), 0.0f });
            // AMT step is 0.001, not 0.01 (user report 2026-08-09): one AMT unit means something
            // different per target — FREQ is ±1 octave, so a 0.01 step was 12 CENTS and the whole
            // usable range for a subtle analog-style pitch drift (±10..25 ct) collapsed onto two
            // knob positions. FILTER CUTOFF spans ±3 octaves per unit and never noticed. The finer
            // interval costs nothing: stored values are unchanged (0.5 stays 0.5, no migration),
            // only the read-out gains a decimal.
            m.params.push_back ({ s + "Amount", k + "Amount", "AMT",   ParamSpec::Kind::Float, juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.5f });   // default +0.5: a freshly-routed slot modulates audibly (0 == off)
        }
        return m;
    }
}
