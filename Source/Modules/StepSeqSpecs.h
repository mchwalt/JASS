#pragma once
#include "ModuleSpec.h"
#include "../DSP/SyncDivision.h"   // SyncDivision::kNames for the SYNC combo
#include "../DSP/StepSequencer.h"  // kMaxSteps — one definition for DSP and UI

// STEP SEQ (Story 15.1) — an authored 16-step figure, transposed by the key you hold. Unlike the
// ARPEGGIATOR (which can only re-order the notes of a held chord and runs free in Hz), each step
// carries its own semitone offset and gate, and the clock rides on the tempo.
//
// Body layout (16.2: 48 steps at W28): 48 pitch knobs + SYNC (2 slots), RATE, LEN, GATE and
// ACCENT = 54 content slots over 2 rack units => 27 cells per row on W28. A first cut used W30
// with a spacer cell between figure and globals; the maintainer saw the gap and asked for the
// tighter shape instead ("Breite 28 sollte möglich sein", 2026-08-31) — the cell width lands a
// hair under the old 19-on-W20 and the knob itself is the fixed Small size either way.
//     row 1 = pitch  1..24 | SYNC (2 cells) | RATE
//     row 2 = pitch 25..48 | LEN | GATE | ACCENT
// DISPLAY order is bodyOrder below; the params vector keeps the historical REGISTRATION order
// (steps 1..32 + globals as shipped, steps 33..48 appended at the end) — the append-only
// contract, so old DAW state keeps its parameter indices. That split is the whole reason
// ModuleSpec::bodyOrder exists.
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
        m.type = rack::ModuleType::Modulator; m.zone = rack::Zone::Modulation; m.size = rack::SizeClass::W28U7;   // 16.2: two rows of 24 steps, 27 cells per row
        // Visible by default (maintainer 2026-08-11), overriding the "special-purpose modules stay
        // hidden until used" rule this shipped with. It is a deliberate trade, made with the price
        // on the table: a factory-VISIBLE module always counts towards the may-appear worst case
        // (Rack::maxHeight), so the display-fit scale must accommodate it whether or not it is on
        // screen — the whole rack draws about a fifth smaller than it would with STEP SEQ and PERC
        // hidden. Story 7.4 bought most of that back by shortening the two-row modules.
        m.defaultVisible = true;

        m.params.push_back ({ "seqOn", "Enabled", "", ParamSpec::Kind::Bool, {}, 0.0f });

        // Each step contributes FOUR params: the pitch knob, its on/off, its ACCENT (15.2), and
        // its GATE (15.7). The two switches are declared with showInBody = false — they must not
        // claim a grid cell; the editor pins them into the corner of the knob as ONE three-state
        // switch (ModuleDescriptor::Knob::toggleParamId + accentParamId: off → on → accented, the
        // TR-909's second-press gesture). The gate is showInBody = false too: it shares the pitch
        // knob's CELL via the ROW toggle (Knob::altParamId) — the BeatStep's "the knob row cycles
        // its meaning" gesture, so 32 gates cost no rack space either.
        auto pitchParam = [&m] (int s)
        {
            m.params.push_back ({ "seqPitch" + juce::String (s), "Pitch" + juce::String (s),
                                  juce::String (s), ParamSpec::Kind::Int,
                                  juce::NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f });
            ParamSpec on { "seqStep" + juce::String (s), "Step" + juce::String (s), "",
                           ParamSpec::Kind::Bool, {}, 1.0f };
            on.showInBody = false;
            m.params.push_back (on);
            ParamSpec acc { "seqAcc" + juce::String (s), "Accent" + juce::String (s), "",
                            ParamSpec::Kind::Bool, {}, 0.0f };   // plain is the default — old figures unchanged
            acc.showInBody = false;
            m.params.push_back (acc);
            // 15.7: per-step gate as ONE continuum (the BeatStep model): 5..100 = percent of the
            // step (scaled by the global GATE), 101 = TIE (held through the boundary, the next
            // step takes over without a retrigger), 102 = SLIDE (like TIE, but the pitch glides —
            // the 303). Default 100 ⇒ exactly the pre-15.7 behaviour, so old figures are untouched.
            ParamSpec sg { "seqSGate" + juce::String (s), "Gate" + juce::String (s), "",
                           ParamSpec::Kind::Int,
                           juce::NormalisableRange<float> (5.0f, 102.0f, 1.0f), 100.0f };
            sg.showInBody = false;
            m.params.push_back (sg);
        };
        // ---- REGISTRATION order: exactly as shipped through 15.7, then 33..48 appended --------
        // (steps 1..16, SYNC, RATE, steps 17..32, LEN, GATE, ACCENT — do not reorder!)
        for (int s = 1; s <= 16; ++s) pitchParam (s);
        // SYNC is fed VERBATIM from SyncDivision::kNames, as DELAY and the LFOs do; retyping that
        // list is how this project has produced combo-index bugs twice. Default "1/8": the measured
        // reference runs eighths at 156 BPM (192.3 ms per step against a measured 192.0).
        m.params.push_back ({ "seqSync", "SyncDiv", "SYNC", ParamSpec::Kind::Choice, {}, 4.0f, SyncDivision::kNames });
        m.params.push_back ({ "seqRate", "Rate", "RATE", ParamSpec::Kind::Float,
                              juce::NormalisableRange<float> (0.5f, 32.0f, 0.1f), 5.2f });   // steps/s when SYNC = Free
        for (int s = 17; s <= 32; ++s) pitchParam (s);
        m.params.push_back ({ "seqLength", "Length", "LEN", ParamSpec::Kind::Int,
                              juce::NormalisableRange<float> (1.0f, (float) StepSequencer::kMaxSteps, 1.0f),
                              16.0f });   // default 16: one bar of eighths, the common case
        m.params.push_back ({ "seqGate", "Gate", "GATE", ParamSpec::Kind::Float,
                              juce::NormalisableRange<float> (0.05f, 1.0f, 0.01f), 1.0f });
        // ACCENT (15.2): what an accented step DOES — how much its higher velocity opens the
        // filter and gains the note up. One knob for the whole pattern (the TD-3/808 model: flags
        // per step, depth global). 0 = accents inaudible; presets saved before 15.2 have
        // no accents anyway, so the audible default only greets NEW figures.
        m.params.push_back ({ "seqAccent", "Accent", "ACCENT", ParamSpec::Kind::Float,
                              juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f });
        // 16.2: steps 33..48 — REGISTERED here at the end (append-only), DISPLAYED in place below.
        for (int s = 33; s <= StepSequencer::kMaxSteps; ++s) pitchParam (s);

        // ---- DISPLAY order: two rows of 24, the globals directly after the figure -------------
        for (int s = 1;  s <= 24; ++s) m.bodyOrder.push_back ("seqPitch" + juce::String (s));
        m.bodyOrder.insert (m.bodyOrder.end(), { "seqSync", "seqRate" });
        for (int s = 25; s <= StepSequencer::kMaxSteps; ++s) m.bodyOrder.push_back ("seqPitch" + juce::String (s));
        m.bodyOrder.insert (m.bodyOrder.end(), { "seqLength", "seqGate", "seqAccent" });
        return m;
    }
}
