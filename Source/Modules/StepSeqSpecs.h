#pragma once
#include "ModuleSpec.h"
#include "../DSP/SyncDivision.h"   // SyncDivision::kNames for the SYNC combo
#include "../DSP/StepSequencer.h"  // kMaxSteps — one definition for DSP and UI

// STEP SEQ (Story 15.1) — an authored 16-step figure, transposed by the key you hold. Unlike the
// ARPEGGIATOR (which can only re-order the notes of a held chord and runs free in Hz), each step
// carries its own semitone offset and gate, and the clock rides on the tempo.
//
// Body layout on the 30-column grid: 24 pitch knobs + 24 step toggles plus SYNC (2 slots), RATE,
// LEN and GATE = 53 content slots over 2 rack units => 27 cells per row, filled in LIST ORDER with
// a wrap at 27. The order below is therefore load-bearing, not cosmetic:
//     row 1 = pitch 1..24 | SYNC (2 cells) | RATE
//     row 2 = step  1..24 | LEN | GATE          (one trailing cell stays empty)
// so every step switch sits directly under its own pitch. Listing the blocks back to back does NOT
// work: cells left over at the end of row 1 swallow the first switches and shift the whole row.
// At 24 steps a cell is 69 px, so the knobs still reach their standard 62 px; 28 steps would be the
// hard limit.
//
// A step's switch is its REST: off means that step stays silent. Note length is the single GATE at
// the end — 1.0 holds the note into the next step (legato), which is how the measured reference is
// played and why it is the default.
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

        // ---- ROW 1: the 24 pitches, then SYNC and RATE ---------------------------------------
        for (int s = 1; s <= StepSequencer::kMaxSteps; ++s)
            m.params.push_back ({ "seqPitch" + juce::String (s), "Pitch" + juce::String (s),
                                  juce::String (s), ParamSpec::Kind::Int,
                                  juce::NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f });
        // SYNC and RATE sit HERE, not with the other globals at the end: cells are laid out in list
        // order and wrap at 27, so these three fill exactly the cells that would otherwise be taken
        // by the first step switches — which would push every switch off its own pitch.
        // SYNC is fed VERBATIM from SyncDivision::kNames, as DELAY and the LFOs do; retyping that
        // list is how this project has produced combo-index bugs twice. Default "1/8": the measured
        // reference runs eighths at 156 BPM (192.3 ms per step against a measured 192.0).
        m.params.push_back ({ "seqSync", "SyncDiv", "SYNC", ParamSpec::Kind::Choice, {}, 4.0f, SyncDivision::kNames });
        m.params.push_back ({ "seqRate", "Rate", "RATE", ParamSpec::Kind::Float,
                              juce::NormalisableRange<float> (0.5f, 32.0f, 0.1f), 5.2f });   // steps/s when SYNC = Free

        // ---- ROW 2: the 24 step switches under their pitches, then LEN and GATE ---------------
        for (int s = 1; s <= StepSequencer::kMaxSteps; ++s)
            m.params.push_back ({ "seqStep" + juce::String (s), "Step" + juce::String (s),
                                  juce::String (s), ParamSpec::Kind::Bool, {}, 1.0f });
        m.params.push_back ({ "seqLength", "Length", "LEN", ParamSpec::Kind::Int,
                              juce::NormalisableRange<float> (1.0f, (float) StepSequencer::kMaxSteps, 1.0f),
                              (float) StepSequencer::kMaxSteps });
        m.params.push_back ({ "seqGate", "Gate", "GATE", ParamSpec::Kind::Float,
                              juce::NormalisableRange<float> (0.05f, 1.0f, 0.01f), 1.0f });
        return m;
    }
}
