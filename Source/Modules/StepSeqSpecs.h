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
        m.defaultVisible = false;   // hidden until switched on: the stock rack is the Init set (maintainer 2026-10-03)

        m.params.push_back ({ "seqOn", "Enabled", "", ParamSpec::Kind::Bool, {}, 0.0f });

        // The STEPS are NOT parameters (AD-14, Story 18.5 stage 2). From 15.1 to 18.4 each step
        // contributed FOUR params (pitch, on/off, ACCENT, GATE) — 3072 of them — registered in
        // append-only blocks so old DAW state kept its indices, and that is precisely what every
        // DAW listed. A figure is content: the cells live in PatternStore::step, the rack binds
        // the step knobs to it (ModuleDescriptor::Knob::patternStep — pitch knob, the three-state
        // corner switch off → on → accented, and the GATE alt row all read and write the store),
        // PresetIO writes the preset's `Steps` array from it, and the DAW state carries it as one
        // XML element. The step cells of the BODY are declared in bodyOrder below as "step:<n>".
        // Removing the params keeps the append-only contract for the REST: VST3 ids are hashes of
        // the id strings, so every remaining parameter keeps its id.
        //
        // ---- REGISTRATION order of what remains: SYNC, RATE, LEN, GATE, ACCENT (do not reorder!)
        // SYNC is fed VERBATIM from SyncDivision::kNames, as DELAY and the LFOs do; retyping that
        // list is how this project has produced combo-index bugs twice. Default "1/8": the measured
        // reference runs eighths at 156 BPM (192.3 ms per step against a measured 192.0).
        m.params.push_back ({ "seqSync", "SyncDiv", "SYNC", ParamSpec::Kind::Choice, {}, 4.0f, SyncDivision::kNames });
        m.params.push_back ({ "seqRate", "Rate", "RATE", ParamSpec::Kind::Float,
                              juce::NormalisableRange<float> (0.5f, 32.0f, 0.1f), 5.2f });   // steps/s when SYNC = Free
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

        // ---- DISPLAY order: two rows of 24, the globals directly after the figure -------------
        // The body shows ONE PAGE (kPageSteps knobs); "step:<n>" is a pattern cell (a knob bound
        // to PatternStore::step, not to a parameter — makeModuleDescriptor turns it into a Knob
        // with patternStep = n-1). ModuleFrame rebinds these cells to the shown page's steps
        // (16.3) — the A/B/C/D window onto the kMaxSteps pattern.
        for (int s = 1;  s <= 24; ++s) m.bodyOrder.push_back ("step:" + juce::String (s));
        m.bodyOrder.insert (m.bodyOrder.end(), { "seqSync", "seqRate" });
        for (int s = 25; s <= StepSequencer::kPageSteps; ++s) m.bodyOrder.push_back ("step:" + juce::String (s));
        m.bodyOrder.insert (m.bodyOrder.end(), { "seqLength", "seqGate", "seqAccent" });
        return m;
    }
}
