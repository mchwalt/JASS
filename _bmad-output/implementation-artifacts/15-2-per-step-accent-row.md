# Story 15.2: Per-step accent row, on format-v7 step objects

Status: ready-for-dev

## Story

As a player recreating classic sequenced basslines,
I want every step to carry an accent — and the preset to store its steps as readable note
objects,
so that figures like Los Niños and Der Mussolini play with the dynamics the originals have,
and a preset file reads like music instead of like a knob dump.

## Why this story, and why now

Reserved since 15.1 ("the SQ-10 had three rows and the second one classically went to the
MS-20's cutoff"). The market analysis (story 15.6, `docs/notes/Sequencer_Market_Analysis.md`)
made it concrete: every classic acid/drum machine has per-step accent — the TB-303/TD-3 flag
with one global depth knob, the TR-909's second-press-on-the-same-button, the SQ-10's second
knob row. The Los Niños MIDI transcription proved the original plays **all 24 steps in exactly
two classes** (v98/long vs v80/short); our preset fakes that with rests. The Kraftwerk
"Roboter" preset is parked on this feature. And the maintainer's preset-format design
(2026-08-23) — each step as its own object with the MIDI note — is the enabling container:
once a step is an object, accent is one added field, and so is everything after it
(gate/tie/slide = follow-up story C, MIDI import/export = story D).

## Acceptance Criteria

1. **Preset FormatVersion 7 — step objects.** The StepSeq block stores each step as its own
   object per the maintainer's design: the **MIDI note number as the canonical value**, the
   spelled name (`"Bb1"`) as writer-generated readability the loader ignores, plus on/off and
   accent. The top-level annotation key `"Comment"` keeps working (Los Ninos carries one).
   **The exact shape is decided with the maintainer before any code** (see Task 1): array
   `"Steps": [...]` vs `"Step1"…"Step32"` keys, and absolute note vs root-relative offset
   (recommendation: absolute, stored alongside the latch root — a file then reads as real
   pitches; the engine keeps transposing with the played key either way).
2. **v6 and older files keep loading bit-identically**: flat `Pitch1/Step1…` keys read as
   before, no accents (all steps plain), sound unchanged. Append-only rules obeyed —
   missing ⇒ default, and the accent default is *plain*.
3. **Accent per step in the UI, no new rack height** — the 909 gesture: the step's corner
   toggle cycles **off → on → accented** (third state), shown dim vs bright on the switch/ring.
   The knob stays a knob; no context menus.
4. **One global ACCENT depth knob** on STEP SEQ: what an accented step *does* — level boost
   and a filter-cutoff bump at note-on (the 303/SQ-10 recipe; exact amounts tuned by the
   maintainer's ear). Depth 0 ⇒ accents change nothing (safe default for every old preset).
5. **Los Niños becomes authentic**: all 24 steps ON in the transcription's two classes
   (v98/long class = accented), rests removed; the preset's `"Comment"` updated to say the
   approximation is gone. DAF Beat gets accents only if the maintainer wants them after
   hearing it.
6. **Docs complete**: help pages EN/DE (terse, one pass), CHANGELOG reasoning,
   `docs/JASS_Preset_Format.md` documents v7 and the step-object shape.

## Tasks / Subtasks

- [x] Task 1: Design decisions — **maintainer decided 2026-08-24:**
  - [x] Step-object shape: **`"Steps"` array** of 32 objects
  - [x] **Absolute MIDI note** in the file (spelled name generated alongside); the engine keeps
        root-relative offsets internally — conversion against the stored latch root, falling
        back to C3 (48, the keyboard's default C) when no latch was saved
  - [x] Accent carrier: **velocity** — accented steps emit a higher note-on velocity; the
        ACCENT knob scales how much velocity moves amp + cutoff
  - [ ] Write-by-playing capturing accent from played velocity: deferred, revisit after the
        core ships
- [ ] Task 2: Format v7 (`Source/Audio/PresetIO.h`, `docs/JASS_Preset_Format.md`)
  - [ ] Writer emits step objects; reader reads v7 objects AND v6 flat keys (legacy path)
  - [ ] `kFormatVersion` → 7; startup migration re-saves user presets (existing mechanism);
        shipped demo presets re-saved deliberately in this story
  - [ ] The 32 accent states live as parameters (`seqAcc1..32`, bool) so UI attachment,
        automation and persistence follow the house pattern — but RANDOM must NOT re-roll
        them wildly (check `randomize()`'s treatment of seqStep/seqPitch and mirror it)
- [ ] Task 3: Engine (`Source/DSP/StepSequencer.h`, `Source/Audio/SynthVoice.*`)
  - [ ] Accented step emits its note with the accent velocity class; plain steps keep today's
        velocity (100 — the audition already matches it, 15.3)
  - [ ] ACCENT depth maps velocity to level boost + cutoff bump at note-on; depth 0 = today's
        behavior exactly (old presets!)
- [ ] Task 4: UI (`Source/UI/PluginEditor.cpp`, `Source/Modules/StepSeqSpecs.h`)
  - [ ] Corner toggle → three states (extend `Knob::toggleParamId` or pair it with the accent
        param; the 15.1 mechanism is documented in the spec header)
  - [ ] ACCENT knob into the module row (fits W20? check the 15-1 layout arithmetic —
        19-cell wrap is load-bearing)
  - [ ] Audition of an accented step sounds the accent (one preview path — `auditionStep`)
- [ ] Task 5: Presets + docs
  - [ ] Los Niños: 24 steps, two classes, Comment updated (AC5); maintainer's ear tunes depth
  - [ ] Help EN/DE, CHANGELOG, `JASS_Preset_Format.md`
  - [ ] Build + run + maintainer's ear; say plainly what is only build-verified

## Dev Notes

- **Append-only is law**: never change a global param default for a wish scenario
  (missing ⇒ default hits old presets retroactively). Accent default = plain, depth default =
  a value that keeps old presets sounding identical (0, or a small tasteful default ONLY if
  old presets cannot carry accents anyway — they cannot, so 0 is still the safe choice; the
  knob's value ships IN the updated demo presets instead).
- **The preset is a post-coupling snapshot and loads verbatim** (PR #60): the loader silences
  parameter couplings and kills voices on both edges of a load — new step params simply ride
  the same `applyVar` path. LatchRoot is NOT a parameter (hook path, 15.5) — if the step
  objects store absolute notes, their offset conversion needs the same care.
- **Voice-embedded structs**: `StepSequencer` is embedded by value — **any header change ⇒
  `/t:Rebuild`** or 0xC0000005 at startup.
- **15.6 reference rule**: preview, write and note names resolve over `seqPitchReference()`
  (latch root, else keyboard C). The step objects' spelled names in the FILE should use the
  stored latch root for the same reason — box, file and figure must agree.
- **The original 15.2 idea** (a full per-step value row feeding a MOD-MATRIX *source*; needs a
  `ModSource` enum append + SRC list order match, see 15-1's follow-up note) is deliberately
  NOT this story — the accent flag is the proven 909/TD-3 version and covers the preset class.
  If the maximal row is ever wanted, it becomes 15.2b in `Feature_Ideas.md`.
- **Story D (MIDI ⇄ STEP SEQ) compatibility**: keep accent ⇔ velocity classes clean — the
  SMF conventions section of the market analysis is the contract (cluster velocities on
  import; export two classes).
- Combo rules if any new combo appears: choice order == enum order; dynamic/int combos need
  `indexIsValue`.
- No context menus, fixed readouts over cursor tooltips, both help languages in one pass.
- Resource `.md` edits ⇒ help-target rebuild; the standalone build does NOT build the
  DemoPresets/Help libs — build them explicitly when demo presets change.
- Feature branch `story/15-2-per-step-accent-row` (exists, off develop @ee8964d);
  **no push, no merge — the maintainer decides.**

### Project Structure Notes

- `Source/Audio/Parameters.h` (32 accent params + ACCENT depth), `Source/Audio/PresetIO.h`
  (v7 writer/reader + legacy), `Source/DSP/StepSequencer.h` (velocity classes),
  `Source/Audio/SynthVoice.*` (velocity → amp/cutoff mapping), `Source/Modules/StepSeqSpecs.h`
  (ACCENT knob, toggle wiring), `Source/UI/PluginEditor.cpp` (3-state corner toggle, audition),
  `DemoPresets/Los Ninos.jass`, `Resources/{EN,DE}/stepseq.md`, `docs/JASS_Preset_Format.md`,
  `CHANGELOG.md`.

### References

- [Source: docs/notes/Sequencer_Market_Analysis.md] — accent models (909 gesture, TD-3
  flag+depth, SQ-10 rows), SMF velocity-class conventions
- [Source: _bmad-output/implementation-artifacts/15-6-step-seq-programming-usability.md] —
  gap analysis, proposal B, maintainer's scope decision
- [Source: _bmad-output/implementation-artifacts/15-1-step-sequencer.md] — 15.2 reservation,
  corner-toggle mechanism (`Knob::toggleParamId`), layout arithmetic
- [Source: docs/JASS_Preset_Format.md] — FormatVersion contract, `legacyPersistKey`
- [Source: DemoPresets/Los Ninos.jass] — the `"Comment"` documenting today's approximation;
  transcription data: all 24 steps, v98/long vs v80/short

## Dev Agent Record

### Agent Model Used

Claude Fable 5.

### Completion Notes List (2026-08-24, build-verified — maintainer's ear still pending)

- **Format v7 shipped** exactly as decided: `toVar` post-processes the spec-written StepSeq
  object — flat `Pitch/Step/Accent` keys removed, `"Steps"` array of
  `{On, Note (absolute, canonical), Name (generated, ignored on load), Accent (omitted when
  plain)}` added; reference = `LatchRoot`, fallback C3 (48). `applyVar` decodes the array after
  the spec pass (which still reads flat v6 keys). `kFormatVersion` = 7; the existing startup
  migration + LOAD-dialog path handle old files with backups, unchanged.
- **Accent chain:** per-step `seqAcc1..32` (Bool, spec-driven, `showInBody=false`) + `seqAccent`
  depth knob (0..1, default 0.5, fills row 2's empty trailing cell — footprint unchanged).
  `StepSequencer` emits accented steps at velocity 127 vs plain 100. `SynthVoice` maps velocity
  around the plain reference (100/127): cutoff ±1 octave at full depth (baked into `baseCutoff`,
  pushed through the strips' existing cutoff apply via `tActive`), gain ±4 dB (one multiply at
  the output write). Depth 0 = bit-exact pre-15.2 behaviour. Side effect by design: a MIDI
  keyboard plays touch-sensitively as far as ACCENT is up.
- **UI:** new `StepSwitch` (3-state, hand-painted checkbox: empty/tick/filled+tick) in
  `ModuleFrame`, driven by two `ParameterAttachment`s so preset/host changes repaint without
  firing the cycle (the #56 lesson); `ModuleDescriptor::Knob::accentParamId` opts a knob in.
  Audition previews accented steps hot (127) and re-triggers on accent change.
- **Still open (Task 5):** Los Niños update to the authentic 24-step two-class figure — needs
  the per-step velocity classes extracted from `D:\downloads\los_ninos.mid` plus the
  maintainer's ear for the ACCENT depth; demo presets not yet re-saved as v7 files in the repo.
  DAF Beat accents = maintainer's call after hearing.
- RANDOM now also rolls accents/depth (they are ordinary params) — treated as a feature of the
  button, same as random figures.

### File List

- `Source/Audio/Parameters.h` (seqAcc1..32, seqAccent, warm loop)
- `Source/Modules/StepSeqSpecs.h` (accent params, ACCENT knob, layout comment)
- `Source/DSP/StepSequencer.h` (accent array, hot velocity)
- `Source/Audio/SynthVoice.{h,cpp}` (accentDepth, cutoff/gain mapping)
- `Source/PluginProcessor.cpp` (accent copy, depth push per voice)
- `Source/UI/rack/ModuleDescriptor.h` (accentParamId), `Source/UI/rack/ModuleFrame.cpp` (StepSwitch)
- `Source/UI/PluginEditor.{h,cpp}` (wiring, accented audition)
- `Source/Audio/PresetIO.h` (FormatVersion 7, Steps array writer/reader)
- `Resources/{EN,DE}/stepseq.md`, `docs/JASS_Preset_Format.md`, `CHANGELOG.md`
