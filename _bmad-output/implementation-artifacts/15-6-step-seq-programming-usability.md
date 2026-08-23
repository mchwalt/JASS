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

- [x] Task 1: Market analysis (AC1) — done 2026-08-23
  - [x] Web research per device class; manuals and demo videos beat marketing pages
  - [x] Write `docs/notes/Sequencer_Market_Analysis.md` — one section per device, one
        comparison table at the end (entry / rests / ties / accent / length / live-edit)
  - [x] Flag every mechanism that presupposes a feature JASS lacks (accent row, ties/slides,
        parameter locks) — these feed AC3's story-sized list
- [x] Task 2: Gap analysis (AC2) — done 2026-08-23, see Dev Agent Record
  - [x] Re-read the dev records of 15-1, 15-4, 15-5 (decisions already made and why)
  - [x] Walk the current module: `Source/Modules/StepSeqSpecs.h`, `Source/DSP/StepSequencer.h`,
        the write-mode rules in `Source/UI/PluginEditor.cpp`
  - [x] Score each market mechanism: have it / have it differently / missing
- [ ] Task 3: Proposal and scope decision (AC3)
  - [x] Prioritized list with effort stars, quick wins first (see Dev Agent Record)
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

Claude Fable 5 (four parallel research subagents for Task 1; synthesis and gap
analysis in the main session).

### Task 2 — Gap analysis (2026-08-23)

Mapping the market findings onto the module as shipped (15.1–15.5 + the #56 fixes):

**JASS already has, and the market confirms as right:**
- *Value-per-step surface* (knob per step + corner toggle, audition on touch) — the
  SQ-10 archetype, with quantized display names on top (better than the original).
- *Typewriter recording* (15.4) with the universal grammar's core: play = write +
  advance, SPACE = rest, ESC = exit. Matches KeyStep/Live/Digitakt conventions.
- *Editing against the running loop*, audible — the property that divides beloved
  from infamous workflows. JASS is on the right side.
- *Polymeter against the drums* (independent LENGTH vs PERC) — several devices sell
  this as a feature (Digitakt per-track, BSP polyrhythm mode).
- *Latch/transpose with the played key* — the 303's pattern-transpose idea, live.

**Missing or divergent, ordered by market weight:**
1. **Accent** — no per-step dynamics at all; rests currently fake the SQ-10 accent
   row (Los Niños, Roboter both blocked on this). Every surveyed acid/drum device
   has it. = reserved story 15.2.
2. **Per-step gate / tie / slide** — JASS has one global GATE; the market's elegant
   model is BSP's per-step gate 1–99 % → TIE → SLIDE continuum.
3. **No step-back in write mode** — 303/TD-3 have BACK, Live has Left-arrow-deletes-
   last; JASS's cursor only moves forward.
4. **No tie gesture in write mode** — the grammar's third verb (advance while
   holding); needs per-step gate storage first.
5. **No skipped steps** (third step state: removed from time) — volca ACTIVE STEP /
   Metropolix SKIP; JASS's off = silent but time-consuming only.
6. **No ratchets / probability / micro-shift** — cheap once a per-step container
   exists, not before.
7. **No MIDI import/export** — the maintainer independently asked for this
   (2026-08-23); conventions documented in the analysis.
8. **No live overdub recording** — the fourth archetype; questionable value on a
   synth panel whose keyboard already plays the transposed figure live.
9. **No fill/Euclid generator** — T-1's entry accelerator; nice-to-have.

### Task 3 — Prioritized proposal (awaiting maintainer decision)

| # | Item | Effort | Depends on | Note |
|---|---|---|---|---|
| A | **Step-back in write mode** (Backspace = cursor back one step, second press clears that step) | ★ | nothing | the one true quick win; completes the typewriter grammar |
| B | **Format v7 step objects + accent row (= story 15.2)** — 909 gesture: second click on the corner toggle cycles off → on → accented (ring dim/bright); one global ACCENT depth knob driving level + filter bump | ★★★ | format v7 | unblocks Los Niños authenticity and the parked Roboter preset |
| C | **Per-step gate → TIE → SLIDE continuum** (BSP model) + tie gesture in write mode (advance while holding) | ★★★ | B (container) | brings 303 slides nearly free |
| D | **STEP SEQ ⇄ MIDI import/export** — import: cluster velocities → accent classes, duration rules → gate/tie/slide; export: inverse | ★★★ | B (accent), C helps | maintainer already wants this; conventions in the analysis doc |
| E | Euclid fill (steps/pulses/rotate pre-populates the grid) | ★★ | nothing | optional sugar |

Explicitly **not** proposed: parameter locks / MOD lanes (JASS's mod matrix already
covers global modulation; a maximal 15.2 can revisit), live overdub recording,
skipped steps (LENGTH covers most uses).

Suggested split: **A** stays in this story (Task 4). **B, C, D** become their own
stories (B = the reserved 15.2; C, D new), sequenced B → C → D because both C and D
build on the v7 per-step container. E only on demand.

### Completion Notes List

- Task 1 delivered as `docs/notes/Sequencer_Market_Analysis.md` (research: four
  parallel agents over official manuals; SMF round-trip section feeds story D).
- Tasks 2 + 3 drafted above; hard gate holds — no implementation until the
  maintainer picks the scope.

### File List

- `docs/notes/Sequencer_Market_Analysis.md` (new)
- `_bmad-output/implementation-artifacts/15-6-step-seq-programming-usability.md` (this file)
