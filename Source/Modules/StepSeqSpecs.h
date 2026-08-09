#pragma once
#include "ModuleSpec.h"
#include "../DSP/SyncDivision.h"   // SyncDivision::kNames for the SYNC combo
#include "../DSP/StepSequencer.h"  // kMaxSteps — one definition for DSP and UI

// STEP SEQ (Story 15.1) — an authored 16-step figure, transposed by the key you hold. Unlike the
// ARPEGGIATOR (which can only re-order the notes of a held chord and runs free in Hz), each step
// carries its own semitone offset and gate, and the clock rides on the tempo.
//
// Body layout on the 30-column grid: 16 pitch + 16 gate knobs plus SYNC (2 slots), RATE and LENGTH
// = 36 content slots over 2 rack units => 18 cells per row, filled in LIST ORDER with a wrap at 18.
// The order below is therefore load-bearing, not cosmetic:
//     row 1 = pitch 1..16 | SYNC (2 cells)
//     row 2 = gate  1..16 | RATE | LEN
// so every gate sits directly under its own pitch. Listing the two blocks back to back does NOT
// work: the two cells left over in row 1 swallow G1 and G2 and shift every gate two columns.
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

        // ---- ROW 1: the 16 pitches, then SYNC in the last two cells --------------------------
        for (int s = 1; s <= StepSequencer::kMaxSteps; ++s)
            m.params.push_back ({ "seqPitch" + juce::String (s), "Pitch" + juce::String (s),
                                  juce::String (s), ParamSpec::Kind::Int,
                                  juce::NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f });
        // SYNC sits HERE, not with the other globals at the end: cells are laid out in list order
        // and wrap at 18, so the two cells it fills are exactly the ones that would otherwise be
        // taken by G1 and G2 — which would push every gate two columns off its own pitch.
        // Fed VERBATIM from SyncDivision::kNames, as DELAY and the LFOs do; retyping that list is
        // how this project has produced combo-index bugs twice. Default "1/8": the measured
        // reference runs eighths at 156 BPM (192.3 ms per step against a measured 192.0).
        m.params.push_back ({ "seqSync", "SyncDiv", "SYNC", ParamSpec::Kind::Choice, {}, 4.0f, SyncDivision::kNames });

        // ---- ROW 2: the 16 gates under their pitches, then RATE and LEN ----------------------
        for (int s = 1; s <= StepSequencer::kMaxSteps; ++s)
            m.params.push_back ({ "seqGate" + juce::String (s), "Gate" + juce::String (s),
                                  "G" + juce::String (s), ParamSpec::Kind::Float,
                                  juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f });
        m.params.push_back ({ "seqRate", "Rate", "RATE", ParamSpec::Kind::Float,
                              juce::NormalisableRange<float> (0.5f, 32.0f, 0.1f), 5.2f });   // steps/s when SYNC = Free
        m.params.push_back ({ "seqLength", "Length", "LEN", ParamSpec::Kind::Int,
                              juce::NormalisableRange<float> (1.0f, (float) StepSequencer::kMaxSteps, 1.0f),
                              (float) StepSequencer::kMaxSteps });
        return m;
    }
}
