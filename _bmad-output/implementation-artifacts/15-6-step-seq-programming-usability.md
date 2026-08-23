# Story 15.6: Step-sequencer programming, the practical way

Status: ready-for-dev

## Story

As a player writing a figure at the instrument,
I want the STEP SEQ to be programmed the way sequencer players expect — informed by how the
classic and current sequencers actually do it,
so that entering and editing a bassline is fast and predictable instead of a fight with the module.

## Why this story exists

The module works (15.1–15.5 shipped), but every usability decision so far was made from first
principles and single reference tracks. Two recent findings say the reference pool is too small:
the Los Niños MIDI transcription showed the original figure lives on a mechanism JASS lacks
entirely (the SQ-10 accent row), and the Kraftwerk "Roboter" attempt was parked for the same
reason. Before adding features one at a time, look at the field once, properly — then decide with
evidence what makes programming figures easier.

## Acceptance Criteria

1. **A market analysis exists** as `docs/notes/Sequencer_Market_Analysis.md` (English, like all
   docs) covering how common sequencers handle **pattern entry**, at minimum:
   - *Hardware, classic:* Korg SQ-10 (JASS's spiritual ancestor), Roland TB-303 pitch mode,
     TR-808/909 x0x entry.
   - *Hardware, current:* Elektron (Digitakt — trig + parameter locks), Arturia BeatStep Pro /
     KeyStep, Behringer TD-3, Korg volca bass/keys.
   - *Software:* DAW step input and piano roll (Waveform — it is on this desk — plus Ableton Live
     and FL Studio's step sequencer), and one modular-style sequencer (Intellijel Metropolix or
     Make Noise René) for the per-stage ideas.
   - For **each** device: entry gesture(s), how rests and ties/slides are written, accent or
     per-step modulation, changing pattern length, live recording vs. step entry, editing while
     the pattern runs, and the one thing that makes it *fast*.
2. **A gap analysis** maps the findings onto what JASS STEP SEQ already has — the 32-step grid
   with per-step on/off in the knob corner (15.1), step audition (15.3), write-by-playing with
   SPACE as rest and ESC as exit (15.4), the latch (15.5) — and names what is missing or awkward,
   each with the market evidence for it.
3. **A prioritized improvement proposal** is put to the maintainer: concrete items with effort
   estimates, quick wins separated from feature-sized work. Feature-sized items spin off as their
   own stories (the per-step accent row is already reserved as **story 15.2**; STEP SEQ ⇄ MIDI
   import/export is in `Feature_Ideas.md`) — the analysis must explicitly conclude whether those
   two move up the queue. **Hard gate: nothing is implemented before the maintainer has picked
   the scope.**
4. **The agreed quick wins are implemented** on a feature branch, with `CHANGELOG.md` entries
   carrying the reasoning and the help pages (`Resources/{EN,DE}/stepseq.md`) updated where
   behavior changes — terse, both languages in one pass.

## Tasks / Subtasks

- [ ] Task 1: Market analysis (AC1)
  - [ ] Web research per device class; manuals and demo videos beat marketing pages
  - [ ] Write `docs/notes/Sequencer_Market_Analysis.md` — one section per device, one
        comparison table at the end (entry / rests / ties / accent / length / live-edit)
  - [ ] Flag every mechanism that presupposes a feature JASS lacks (accent row, ties/slides,
        parameter locks) — these feed AC3's story-sized list
- [ ] Task 2: Gap analysis (AC2)
  - [ ] Re-read the dev records of 15-1, 15-4, 15-5 (decisions already made and why)
  - [ ] Walk the current module: `Source/Modules/StepSeqSpecs.h`, `Source/DSP/StepSequencer.h`,
        the write-mode rules in `Source/UI/PluginEditor.cpp`
  - [ ] Score each market mechanism: have it / have it differently / missing
- [ ] Task 3: Proposal and scope decision (AC3)
  - [ ] Prioritized list with effort stars, quick wins first
  - [ ] Maintainer review — in conversation, German; the documents stay English
  - [ ] Record the decision in this file under Dev Agent Record
- [ ] Task 4: Implement the agreed quick wins (AC4)
  - [ ] Feature branch, Conventional Commits, CHANGELOG reasoning
  - [ ] Help pages EN/DE in one pass
  - [ ] Build + run + maintainer's ear (no unit-test rig — say plainly what is only
        build-verified)

## Dev Notes

- **House rules that shape any UX proposal:**
  - No context menus / right-click — visible controls only; fixed readouts beat cursor-chasing
    tooltips (maintainer feedback, standing).
  - The step knob **stays a knob** — maintainer's explicit condition from the note-readout work;
    proposals may change what is displayed and how entry works, not the control type.
  - Combo choices must match enum order; dynamic/int-param combos need `indexIsValue`
    (recurring bug class).
- **Preset format is append-only.** Anything that changes stored step data means
  **FormatVersion 7** with legacy reading of the flat `Pitch1/Step1…` keys. The maintainer's
  declared direction (2026-08-23): each step becomes its own object keyed `"Note"` — MIDI number
  as the canonical value plus the spelled name (`"C#2"`) as generated, loader-ignored
  readability. The top-level key `"Note"` is deliberately kept free for this; preset annotations
  use `"Comment"` (see `DemoPresets/Los Ninos.jass`). Decide absolute-vs-root-relative storage
  consciously: today the file stores offsets against `LatchRoot` because the figure transposes
  with the played key.
- **Mechanisms to reuse, not rebuild:** `auditionStep` is the single preview path (15.4);
  write-cursor arming rules — only a mouse gesture on the control arms, ESC ends, preset load
  clears (15.4 + the #56 fixes); the latch is one atomic in the processor and `LatchRoot`
  reaches PresetIO through hooks, not as a parameter (15.5).
- **Warning from 15.5:** the sequencer's notes must stay out of `MidiKeyboardState` — the root
  search reads that state, and the pattern would re-root itself on its own output.
- **PERC is adjacent but out of scope:** its grid is a different widget with a two-dimensional
  cursor (15.4 follow-up note). If a market finding applies to PERC, note it in the analysis
  and leave it there.
- **Build:** MSBuild full path (CMake not on PATH); `/t:Rebuild` whenever a header with
  voice-embedded structs changes; resource `.md` changes need a CMake reconfigure + help-target
  rebuild.
- Feature branch; **no push, no merge** — the maintainer decides.

### Project Structure Notes

- Analysis document: `docs/notes/Sequencer_Market_Analysis.md` (new).
- Implementation (Task 4) touches: `Source/Modules/StepSeqSpecs.h`, `Source/DSP/StepSequencer.h`,
  `Source/UI/PluginEditor.cpp`, `Source/Audio/Parameters.h`, `Source/Audio/PresetIO.h`,
  `Resources/{EN,DE}/stepseq.md`, `CHANGELOG.md`.

### References

- [Source: _bmad-output/implementation-artifacts/15-1-step-sequencer.md] — layout arithmetic
  (W20, 19-cell wrap is load-bearing), the gate/rest design history, story 15.2 reservation
- [Source: _bmad-output/implementation-artifacts/15-4-step-recording.md] — SPACE-as-rest
  correction, off-EDGE-not-state lesson
- [Source: _bmad-output/implementation-artifacts/15-5-step-seq-latch.md] — latch design,
  MidiKeyboardState warning
- [Source: docs/notes/Feature_Ideas.md#Sequencer] — accent row (15.2), STEP SEQ ⇄ MIDI
- [Source: docs/JASS_Preset_Format.md] — FormatVersion contract, `legacyPersistKey`

## Dev Agent Record

### Agent Model Used

### Debug Log References

### Completion Notes List

### File List
