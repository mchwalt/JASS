# Story 15.7 (C): Per-step gate → TIE → SLIDE, on one continuum

Status: built 2026-08-27, build-verified — maintainer's ear pending

## Story

As a player writing sequenced lines,
I want each step to carry its own length — up to "held into the next step" (tie) and
"gliding into the next note" (slide),
so that figures phrase like the originals: Los Niños' two length classes, and the 303's
acid lines where notes melt into each other.

## Why this story, and why now

The market analysis (15.6) named ties/slides/per-step gate as gap #2 behind accent, and the
BeatStep Pro model as the elegant one: **the per-step gate value runs 1–99 % and then two
special top values, TIE and SLIDE — staccato→legato→glide as one continuum on one knob.**
The 15.2 accent shipped the enabling container (format-v7 step objects): gate is now one
added field. And the Los Niños measurement gives this story its ear-test: the original's two
classes differ in LENGTH as much as level (104 vs 43 of 120 ticks — 87 % vs 36 %), which the
accent alone cannot express. History note: per-step gate knobs were tried in 15.1 and thrown
out ("they were all sitting at 1") — what changed is (a) a measured preset that needs them,
(b) the row-toggle UI that makes them cost no rack space, (c) TIE/SLIDE living on the same
knob, which a global GATE can never carry.

## Design (maintainer, 2026-08-26)

- **Model: the BSP continuum.** One value per step: **gate 5–100 %, then TIE, then SLIDE**
  as the two values past the top. Int parameter `seqSGate1..32`, range 5..102
  (5–100 = percent, 101 = TIE, 102 = SLIDE), **default 100**. Readout "36%" / "TIE" / "SLIDE".
- **UI: the knobs switch meaning.** A **ROW: PITCH ⇄ GATE toggle** on the module flips what
  the 32 step knobs edit (the BSP "KNOBS cycles the row's meaning" gesture). No new rack
  height. The corner switches (on/accent) stay visible in both rows — they belong to the
  step, not to the knob's current meaning. The toggle is **view state, not a parameter**
  (loading a preset must not flip the user's view).
- **Semantics** (303/BSP, the flag sits on the EARLIER step):
  - Plain step: effective note length = step interval × global GATE × (stepGate/100).
    Default 100 ⇒ bit-exact today's behaviour (append-only rule satisfied).
  - **TIE on step N:** N's note is held through the boundary; **step N+1 does not
    retrigger** — if N+1 is ON its pitch is taken over without a new attack (same pitch =
    classic lengthened note; different pitch = pitch steps without attack), if N+1 is OFF
    the note ends at the boundary. TIE with nothing sounding stays silent.
  - **SLIDE on step N:** like TIE, but the pitch **glides** into N+1's note (303
    portamento; fixed ~60 ms to start, the maintainer's ear tunes it).
  - Accent on a step that was tied/slid INTO is ignored (there is no attack to accent).
- **Format v8:** the step object gains `"Gate"` — an int 5..100, or the string `"TIE"` /
  `"SLIDE"`; **omitted when 100** (the common case stays terse). Missing ⇒ 100.
  `kFormatVersion` → 8; v7 and older keep loading bit-identically.

## Acceptance Criteria

1. Loading any pre-15.7 preset sounds bit-identical (all step gates default 100).
2. ROW toggle flips the 32 knobs between PITCH and GATE editing; the readouts follow
   ("A#1" ⇄ "36%"/"TIE"/"SLIDE"); corner switches and playhead work in both views;
   audition previews a gate edit audibly (hear the new length/tie).
3. A tied step does not retrigger; a slid step glides. GATE (global) still scales all
   plain steps.
4. **Los Niños carries its measured length classes**: accented steps Gate 87, plain 36,
   the three high fills 41 (from the MIDI durations 104/43/49 of 120 ticks); preset saved
   as v8, Comment updated. DAF Beat untouched (measured: uniform legato — correct as is).
5. Docs complete: help EN/DE (one pass), CHANGELOG reasoning, `JASS_Preset_Format.md` v8.

## Tasks / Subtasks

- [x] Task 1: Parameters + spec (`Parameters.h`, `StepSeqSpecs.h`)
  - [x] `seqSGate1..32` Int 5..102 default 100, `showInBody=false` (the pitch knob's cell
        hosts them via the row toggle); textFromValue percent/TIE/SLIDE; RANDOM treatment:
        mirror 15.2's accents (they roll — a random figure includes random phrasing)
- [x] Task 2: Engine (`StepSequencer.h`, `SynthVoice.*`, `PluginProcessor.cpp`)
  - [x] Per-step gate scales the countdown; TIE suppresses next retrigger (pitch takeover),
        SLIDE additionally glides — reuse the poly-glide plumbing (`glideInfo` per-voice
        ratios) rather than a second pitch-ramp mechanism
  - [x] Header changes ⇒ `/t:Rebuild` (voice-embedded structs, 0xC0000005 rule)
- [x] Task 3: UI (`ModuleDescriptor.h`, `ModuleFrame.cpp`, `PluginEditor.cpp`)
  - [x] Row toggle (view state); knob re-attachment or paired-slider visibility per cell;
        keep corner switch + ring/playhead anchoring intact; audition on gate edits
- [x] Task 4: Format v8 (`PresetIO.h`, `docs/JASS_Preset_Format.md`)
  - [x] Writer emits `"Gate"` (omitted at 100); reader v8 + v7 + v6 legacy; migration
        re-save path untouched (existing mechanism)
- [ ] Task 5: Presets + docs + verify
  - [x] Los Niños gates 87/36/41 (AC4), Comment; help EN/DE; CHANGELOG
  - [ ] Build + run + maintainer's ear; say plainly what is only build-verified

## Dev Agent Record (2026-08-27, build-verified — maintainer's ear pending)

- **Engine:** `StepSequencer` gained `sgate[32]` (5..100 %, 101=TIE, 102=SLIDE; constants on the
  class), a `tiePending/tieIsSlide` chain state and a fixed `Legato{note, semitones, slide}[8]`
  event list per block. A TIE/SLIDE boundary emits NO MIDI: the processor applies the takeover
  via the new `SynthVoice::slideTo(semitones, seconds)` (multiplies `transposeRatio`, re-ramps
  `glideRatio` — the poly-glide smoother; envelope untouched). The chain keeps its MIDI identity
  (`soundingNote` unchanged, `soundingPitch` tracks the audible note), so the final note-off
  matches its note-on. Slide time = `kSlideSeconds` 0.06 (ear-tunable constant). Block-granular
  slide start (≤ one buffer early) — documented, inaudible against ≥ 60 ms steps.
- **Per-step gate math:** countdown = interval × globalGATE × sg/100 — default 100 is bit-exact
  pre-15.7 (AC1). A tied-INTO step applies its own gate to the held note (BSP hand-over).
- **UI:** `Knob::altParamId/altTextFromValue/altValueFromText` + `ModuleDescriptor::altRowTitle`
  — a header latch ("GATE") flips paired sliders per cell (second `SynthySlider` on identical
  bounds, visibility swap only; corner switch kept above via `toFront`). Alt knob dims with the
  step (condKnobs dimOnly), auditions through the STEP'S pitch, gets preset-baseline
  double-click, right-click accepts "TIE"/"SLIDE"/number. View state — never in presets.
- **Format v8:** writer adds `"Gate"` (omitted at 100; strings for TIE/SLIDE), reader tolerant
  (number or string, missing ⇒ 100); `kFormatVersion` = 8; docs updated. The numbered flat
  "Gate1..32" spec keys are stripped like Pitch/Step/Accent — the GLOBAL "Gate" key stays.
- **Los Niños:** gates 87 (accents) / 36 (plain) / 41 (the three fills) on steps 1..24 from the
  MIDI durations 104/43/49 of 120 ticks; Comment rewritten; v8; copied to %AppData% (seeding
  never overwrites — the 15.2 lesson). DAF Beat untouched (measured uniform legato).
- **Verify (maintainer):** old preset loads unchanged · GATE latch flips row and back · gate
  knob edit audibly previews · TIE holds/takes over without attack · SLIDE glides · Los Niños
  phrasing now short/long as the record · save+reload round-trips gates.

## Dev Notes

- Append-only is law: default 100 = today's sound; never change global GATE's meaning.
- The preset is a post-coupling snapshot (PR #60); new params ride `applyVar` unchanged.
- 15.1 layout arithmetic (19-cell wrap) is load-bearing — the row toggle must not claim a
  grid cell in the step rows; header placement (next to Reset ↺) is the candidate.
- Keep accent ⇔ velocity and gate ⇔ duration clean for story D (MIDI ⇄ STEP SEQ): SMF
  conventions say duration ≤ step = gated, spanning = tie, overlapping next note-on = slide.
- Audition path is `auditionStep` (one preview path, 15.3); the #56 lesson: only real
  gestures sound, loader replays stay silent.

### References

- [Source: docs/notes/Sequencer_Market_Analysis.md] — BSP gate→TIE→SLIDE, 303 semantics,
  SMF duration conventions
- [Source: _bmad-output/implementation-artifacts/15-2-per-step-accent-row.md] — step-object
  container, corner-switch mechanics, velocity mapping
- [Source: memory project_jass_session_2026_08_26] — Los Niños duration classes 104/43/49
