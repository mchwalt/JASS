#pragma once
#include "ModuleSpec.h"
#include "../DSP/SyncDivision.h"    // SYNC combo, fed verbatim
#include "../DSP/PercSequencer.h"   // kLanes / kMaxSteps — one definition for DSP and UI

// PERC (Story 16.1) — four percussion tracks on a 48-step grid (32 until 16.2), rendered straight into the master
// bus. Params only: the body is assembled in the editor, because the step grid is a custom
// component (128 switches at a 62 px grid cell would be six rack units) and the KIT combo is a
// dynamic list, like the SAMPLER's SET.
//
// What this module is, in one line: a SECOND SAMPLER INSTANCE at processor level, pointed at a drum
// kit. Not a second note sequencer — JASS is monotimbral, so MIDI notes would drag the drums through
// the bass's filter and effects (see PercSequencer.h for the full reasoning).
//
// Body layout, filled in this order after the editor puts the grid in front:
//     row 1 = the step grid (a Display spanning the full row)
//     row 2 = KIT | SYNC | RATE | LEN | NOTE 1 | LVL 1 | … | NOTE 4 | LVL 4
// Fifteen cells in a nineteen-cell row, so there is room to grow.
namespace Modules
{
    inline ModuleSpec perc()
    {
        ModuleSpec m;
        m.id = "perc"; m.title = "PERC"; m.persistObject = "Perc"; m.enableParamId = "percOn";
        m.type = rack::ModuleType::Generator;   // it IS a sound source, even though it sits here
        // Next to STEP SEQ rather than in a zone of its own: a PERCUSSION zone for a single module
        // is an empty drawer, and the point of the layer-B framing is that PERC is not a new
        // category but a second instance of something the rack already has (decision 2026-08-10).
        m.zone = rack::Zone::Modulation; m.size = rack::SizeClass::W24U7;   // 16.2: 48-step grid plus one knob row at W24 — approved by eye ("das sah gut aus", 2026-08-31)
        m.defaultVisible = true;    // maintainer 2026-08-11; see the same note in StepSeqSpecs.h —
                                    // a factory-visible module is always in the worst-case height

        m.params.push_back ({ "percOn", "Enabled", "", ParamSpec::Kind::Bool, {}, 0.0f });

        // KIT = an index into the session's SampleBankStore, its own and independent of the
        // SAMPLER's SET — otherwise a sampled instrument and a drum kit could not coexist, which is
        // exactly the case this module exists for. Not shown from the spec: the editor builds the
        // dynamic combo (indexIsValue), as it does for the SAMPLER.
        // 0 = NO KIT, 1..32 = store index + 1. A module that ships pointing at whatever sample sits
        // at index 0 is simply wrong — the maintainer's fresh install offered "Drums_110BPM", a
        // one-shot loop, as its drum kit. With no kit chosen PERC has nothing to play and says so.
        ParamSpec kit { "percKit", "Kit", "", ParamSpec::Kind::Float,
                        juce::NormalisableRange<float> (0.0f, 32.0f, 1.0f), 0.0f };
        kit.showInBody = false;
        m.params.push_back (kit);

        // Same clock the LFOs, DELAY and STEP SEQ ride on, so a bass on eighths and a beat on
        // sixteenths cannot drift apart. Default 1/16 = the classic one-bar drum grid at LEN 16.
        // The kit's own master. Two jobs, two controls: the per-lane AMPs set the BALANCE (kick
        // against hats), this one sets how loud the whole kit stands against the rest of the rack.
        // Without it that second job would mean moving four knobs and wrecking the balance on the
        // way (maintainer 2026-08-11).
        //
        // 0..1 like every other level knob in the rack — a knob that reads 0..4 in one module and
        // 0..1 everywhere else teaches nothing but mistrust (maintainer: "Standard ist 0 bis 1").
        // The headroom moved INSIDE instead: PercSequencer multiplies by kAmpScale, so full scale
        // is +12 dB over the kit as recorded and unity sits at a quarter turn. A drum kit is
        // mastered with reserve — SamsSonor even ships group_volume=-3.8 — while three oscillators
        // with unison sit near full scale, which is why PERC sounded quiet at maximum before.
        m.params.push_back ({ "percAmp", "Amp", "AMP", ParamSpec::Kind::Float,
                              juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f });
        m.params.push_back ({ "percSync", "SyncDiv", "SYNC", ParamSpec::Kind::Choice, {}, 5.0f, SyncDivision::kNames });
        m.params.push_back ({ "percRate", "Rate", "RATE", ParamSpec::Kind::Float,
                              juce::NormalisableRange<float> (0.5f, 32.0f, 0.1f), 8.0f });   // steps/s when Free
        m.params.push_back ({ "percLength", "Length", "LEN", ParamSpec::Kind::Int,
                              juce::NormalisableRange<float> (1.0f, (float) PercSequencer::kMaxSteps, 1.0f),
                              16.0f });   // one bar of sixteenths — the common case

        // Per lane: WHICH instrument of the kit it fires, and how loud. The note number is the
        // stored value on purpose (never a list position): a kit still loading in the background,
        // or a different kit entirely, must not silently move a lane to another drum. The knob
        // READS OUT the instrument's name — the editor resolves it from the loaded set, falling
        // back to the General MIDI drum map and then to the plain note name.
        static constexpr int   kDefaultNote[PercSequencer::kLanes]  = { 36, 38, 42, 46 };  // GM: kick, snare, closed + open hat
        for (int l = 1; l <= PercSequencer::kLanes; ++l)
        {
            // The caption carries the LANE NUMBER: four knobs captioned "NOTE" and four captioned
            // "LVL" gave no clue which row of the grid they belonged to (maintainer, first use).
            m.params.push_back ({ "percNote" + juce::String (l), "Note" + juce::String (l),
                                  "NOTE " + juce::String (l),
                                  ParamSpec::Kind::Int, juce::NormalisableRange<float> (24.0f, 96.0f, 1.0f),
                                  (float) kDefaultNote[l - 1] });
            // AMP, not LVL: every other module in the rack calls a level knob AMP, and PERC is not
            // the place to invent a second word for it (maintainer 2026-08-11).
            // Balance only, 0..1 — the boost lives on the module's own AMP above. A per-lane knob
            // that could also amplify would mean two ways to do the same thing, and no way to tell
            // from the panel which of them is holding the level.
            m.params.push_back ({ "percLevel" + juce::String (l), "Amp" + juce::String (l),
                                  "AMP " + juce::String (l),
                                  ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.8f });
            m.params.back().legacyPersistKey = "Level" + juce::String (l);   // pre-2026-08 presets; still read
            // Placement per lane. PERC writes straight to the bus, so this is its own constant-power
            // pan — it never passes the STEREO stage the voices go through. Default centred: a
            // default that spreads the kit would rewrite every preset that omits the parameter.
            m.params.push_back ({ "percPan" + juce::String (l), "Pan" + juce::String (l),
                                  "PAN " + juce::String (l),
                                  ParamSpec::Kind::Float, juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f });
        }

        // The grid itself: 4 lanes x 48 steps of Bool, all showInBody = false. They claim no cell —
        // the PercGrid component paints them and writes them straight to the APVTS.
        // REGISTRATION order is append-only: the shipped 4x32 block stays exactly as it was,
        // steps 33..48 of every lane follow BEHIND it (16.2) — a naive kMaxSteps loop would have
        // spliced lane 1's new steps in front of lane 2's old ones and shifted every index.
        auto stepParam = [&m] (int l, int s)
        {
            ParamSpec step { "percStep" + juce::String (l) + "_" + juce::String (s),
                             "Step" + juce::String (l) + "_" + juce::String (s), "",
                             ParamSpec::Kind::Bool, {}, 0.0f };
            step.showInBody = false;
            m.params.push_back (step);
        };
        for (int l = 1; l <= PercSequencer::kLanes; ++l)
            for (int s = 1; s <= 32; ++s)
                stepParam (l, s);
        // 16.2's block: lanes × steps 33..48. FROZEN at 48 — extending ITS loop bound would splice
        // lane 1's new steps in front of lane 2's 33..48 and shift every shipped index (the same
        // trap 16.2 documented). New steps go in their own block below.
        for (int l = 1; l <= PercSequencer::kLanes; ++l)
            for (int s = 33; s <= 48; ++s)
                stepParam (l, s);
        // 16.3's block: lanes × steps 49..kMaxSteps, appended BEHIND everything shipped.
        for (int l = 1; l <= PercSequencer::kLanes; ++l)
            for (int s = 49; s <= PercSequencer::kMaxSteps; ++s)
                stepParam (l, s);
        return m;
    }
}
