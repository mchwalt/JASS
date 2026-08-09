#pragma once
#include "ModuleSpec.h"
#include "../DSP/SyncDivision.h"   // SyncDivision::kNames for the SYNC combo
#include "../DSP/StepSequencer.h"  // kMaxSteps — one definition for DSP and UI

// STEP SEQ (Story 15.1) — an authored 16-step figure, transposed by the key you hold. Unlike the
// ARPEGGIATOR (which can only re-order the notes of a held chord and runs free in Hz), each step
// carries its own semitone offset and gate, and the clock rides on the tempo.
//
// Body layout on the 30-column grid: 16 pitch + 16 gate knobs plus SYNC (2 slots), RATE and LENGTH
// = 36 content slots over 2 rack units => 18 cells per row. That is deliberate, not incidental: it
// puts steps 1..16 in columns 0..15 of BOTH rows, so a step's gate sits directly under its pitch,
// and parks the three global controls in the two columns at the right edge. Reordering this list
// breaks that alignment.
//
// Gate is a fraction of the step: 0 = rest (the step is silent), 1 = legato — the note is held into
// the next step, which is how the reference figure is played and why 1.0 is the default.
namespace Modules
{
    inline ModuleSpec stepSeq()
    {
        ModuleSpec m;
        m.id = "stepseq"; m.title = "STEP SEQ"; m.persistObject = "StepSeq"; m.enableParamId = "seqOn";
        m.type = rack::ModuleType::Modulator; m.zone = rack::Zone::Modulation; m.size = rack::SizeClass::W30H2;
        // Default-hidden like COMPRESSOR: a special-purpose module should not spend rack height
        // (a hard budget since Story 7.3) until it is used. A preset that switches it on reveals
        // it automatically via revealEnabledModules().
        m.defaultVisible = false;

        m.params.push_back ({ "seqOn", "Enabled", "", ParamSpec::Kind::Bool, {}, 0.0f });

        // ---- steps: pitch row, then gate row (see the layout note above) --------------------
        for (int s = 1; s <= StepSequencer::kMaxSteps; ++s)
            m.params.push_back ({ "seqPitch" + juce::String (s), "Pitch" + juce::String (s),
                                  juce::String (s), ParamSpec::Kind::Int,
                                  juce::NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f });
        for (int s = 1; s <= StepSequencer::kMaxSteps; ++s)
            m.params.push_back ({ "seqGate" + juce::String (s), "Gate" + juce::String (s),
                                  "G" + juce::String (s), ParamSpec::Kind::Float,
                                  juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f });

        // ---- global controls (right-hand columns) -------------------------------------------
        // SYNC is fed VERBATIM from SyncDivision::kNames, as DELAY and the LFOs do — retyping the
        // list is how this project has produced combo-index bugs twice. Default "1/8": the measured
        // reference runs eighths at 156 BPM (192.3 ms per step against a measured 192.0).
        m.params.push_back ({ "seqSync", "SyncDiv", "SYNC", ParamSpec::Kind::Choice, {}, 4.0f, SyncDivision::kNames });
        m.params.push_back ({ "seqRate", "Rate", "RATE", ParamSpec::Kind::Float,
                              juce::NormalisableRange<float> (0.5f, 32.0f, 0.1f), 5.2f });   // steps/s when SYNC = Free
        m.params.push_back ({ "seqLength", "Length", "LEN", ParamSpec::Kind::Int,
                              juce::NormalisableRange<float> (1.0f, (float) StepSequencer::kMaxSteps, 1.0f),
                              (float) StepSequencer::kMaxSteps });
        return m;
    }
}
