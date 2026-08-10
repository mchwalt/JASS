#pragma once
#include "ModuleSpec.h"
#include "../DSP/SyncDivision.h"   // SyncDivision::kNames for the SYNC combo
#include "../DSP/StepSequencer.h"  // kMaxSteps — one definition for DSP and UI

// STEP SEQ (Story 15.1) — an authored 16-step figure, transposed by the key you hold. Unlike the
// ARPEGGIATOR (which can only re-order the notes of a held chord and runs free in Hz), each step
// carries its own semitone offset and gate, and the clock rides on the tempo.
//
// Body layout: 32 pitch knobs + SYNC (2 slots), RATE, LEN and GATE = 37 content slots over 2 rack
// units => 19 cells per row. Two rows of SIXTEEN steps, not one long row (user 2026-08-10) — and
// that is the cheaper shape in every direction: 24 steps in a single row needed the full W30 to
// keep 62 px knobs, while 32 steps split over two rows fit into W20. More steps, ten columns less.
// The order below is load-bearing:
//     row 1 = pitch  1..16 | SYNC (2 cells) | RATE
//     row 2 = pitch 17..32 | LEN | GATE          (one trailing cell stays empty)
// so step 17 sits under step 1 and the bar structure is readable. Interleaving the globals with the
// steps, or listing all 32 pitches first, scatters the second half across the wrong columns.
//
// Each step has an ON switch in the top-right corner of its own knob — off is a rest, and the knob
// greys out the way every other inactive control in the rack does. It lives in the knob's cell, not
// in a row of its own: a separate row of checkboxes was tried first and thrown out (it doubled the
// module height for nothing, and a silent step in a legato figure reads as the sound breaking off
// rather than as rhythm). Targeted gaps are worth having for percussion patterns, which is why the
// switch came back in a form that costs no space.
//
// Pattern length is LEN, note length is the single GATE: 1.0 holds each note into the next step
// (legato), which is how the measured reference is played and why it is the default.
namespace Modules
{
    inline ModuleSpec stepSeq()
    {
        ModuleSpec m;
        m.id = "stepseq"; m.title = "STEP SEQ"; m.persistObject = "StepSeq"; m.enableParamId = "seqOn";
        m.type = rack::ModuleType::Modulator; m.zone = rack::Zone::Modulation; m.size = rack::SizeClass::W20H2;
        // Default-hidden like COMPRESSOR: a special-purpose module should not spend rack height
        // (a hard budget since Story 7.3) until it is used. A preset that switches it on reveals
        // it automatically via revealEnabledModules().
        m.defaultVisible = false;

        m.params.push_back ({ "seqOn", "Enabled", "", ParamSpec::Kind::Bool, {}, 0.0f });

        // Each step contributes TWO params: the pitch knob and its on/off. The switch is declared
        // with showInBody = false — it must not claim a grid cell, the editor pins it into the
        // corner of its knob (ModuleDescriptor::Knob::toggleParamId).
        auto pitchParam = [&m] (int s)
        {
            m.params.push_back ({ "seqPitch" + juce::String (s), "Pitch" + juce::String (s),
                                  juce::String (s), ParamSpec::Kind::Int,
                                  juce::NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f });
            ParamSpec on { "seqStep" + juce::String (s), "Step" + juce::String (s), "",
                           ParamSpec::Kind::Bool, {}, 1.0f };
            on.showInBody = false;
            m.params.push_back (on);
        };
        const int half = StepSequencer::kMaxSteps / 2;

        // ---- ROW 1: steps 1..16, then SYNC and RATE -------------------------------------------
        for (int s = 1; s <= half; ++s) pitchParam (s);
        // SYNC is fed VERBATIM from SyncDivision::kNames, as DELAY and the LFOs do; retyping that
        // list is how this project has produced combo-index bugs twice. Default "1/8": the measured
        // reference runs eighths at 156 BPM (192.3 ms per step against a measured 192.0).
        m.params.push_back ({ "seqSync", "SyncDiv", "SYNC", ParamSpec::Kind::Choice, {}, 4.0f, SyncDivision::kNames });
        m.params.push_back ({ "seqRate", "Rate", "RATE", ParamSpec::Kind::Float,
                              juce::NormalisableRange<float> (0.5f, 32.0f, 0.1f), 5.2f });   // steps/s when SYNC = Free

        // ---- ROW 2: steps 17..32 under them, then LEN and GATE --------------------------------
        for (int s = half + 1; s <= StepSequencer::kMaxSteps; ++s) pitchParam (s);
        m.params.push_back ({ "seqLength", "Length", "LEN", ParamSpec::Kind::Int,
                              juce::NormalisableRange<float> (1.0f, (float) StepSequencer::kMaxSteps, 1.0f),
                              16.0f });   // default 16: one bar of eighths, the common case
        m.params.push_back ({ "seqGate", "Gate", "GATE", ParamSpec::Kind::Float,
                              juce::NormalisableRange<float> (0.05f, 1.0f, 0.01f), 1.0f });
        return m;
    }
}
