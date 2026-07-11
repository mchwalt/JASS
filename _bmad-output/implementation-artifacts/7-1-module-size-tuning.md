# Story 7.1: Tighten module size classes to fit control counts

Status: ready-for-dev

<!-- Note: Validation is optional. Run validate-create-story for quality check before dev-story. -->

## Story

As a JASS player,
I want each module sized to its actual controls (no oversized / over-wide modules),
so that the rack reads tight and balanced rather than wasteful.

## Acceptance Criteria

1. **Audit-driven.** Each module's assigned size class is compared against its control count (combo = 2 slots, knob/button = 1 slot, display = N) using the audit table in Dev Notes; only modules that are visibly larger/wider than their content warrants are changed.
2. **Tighter where justified.** Apply the tightening changes the user approves in the running app (proposed first pass in Dev Notes). Modules already well-fitted stay as they are.
3. **Rotary minimum (AD-3).** After any shrink, rotaries keep a usable minimum diameter — no knob becomes too small. If shrinking would violate this, the module stays at its current class.
4. **Combo width.** Combos keep enough width to show their item text (a combo needs ~2 content slots). Do not cram a combo into a box too narrow to read (e.g. FILTER "Lowpass/Highpass", DISTORTION "Soft Clip").
5. **Rules held.** "XXS = single-control modules only"; MASTER BUS stays right-aligned, other zones left-aligned; the uniform header anatomy (title · info ⓘ · reset ↺ · enable) and the fixed 1520 px / 12-column grid are unchanged.
6. **OSC stays M.** OSC 1/2/3 remain **M** (6 controls incl. FB — never shrink to S).
7. **Scope.** The only code change is size-class assignments in `PluginEditor.cpp buildSampleRack` (and CROSS MOD's `mix.sizeClass`). Do NOT change the size-class table in `ModuleDescriptor.h` unless a genuinely new class is required (avoid).
8. **UI-only.** No parameter, APVTS, audio-thread, DSP or `.synthy` change (NFR2, NFR3).
9. **Verified in the running app.** Clean build; the tightened modules look balanced, every control is still usable and readable, nothing truncates or overlaps a grid boundary; the window still fits (height auto-fits per AD-12).

## Tasks / Subtasks

- [ ] **Task 1 — Confirm the audit table (AC: 1)** — read the current assignments in `buildSampleRack` against the table in Dev Notes; note any module whose look you'd change beyond the proposed first pass.
- [ ] **Task 2 — Apply the first-pass shrink candidates (AC: 2, 3, 4)** — in `PluginEditor.cpp buildSampleRack`, change only the `SizeClass` argument for the approved candidates:
  - [ ] **SUB** S → XS (WAVE combo + LEVEL = 2 controls).
  - [ ] **NOISE** S → XS (TYPE combo + AMP = 2 controls).
  - [ ] **FILTER** M → S (TYPE combo + CUTOFF + RESO = 3 controls).
  - [ ] **DISTORTION** M → S (TYPE combo + DRIVE + MIX = 3 controls).
  - [ ] (Leave everything else — see table.)
- [ ] **Task 3 — Build + verify each candidate in the app (AC: 3, 4, 9)** — for each shrunk module, confirm in the running app: the combo item text still reads, the knobs are still comfortably sized (AD-3), nothing overlaps. **Revert any candidate that looks cramped** (that's the correct outcome, not a failure).
- [ ] **Task 4 — Re-fit + regression pass (AC: 5, 9)** — confirm zones still align (MASTER BUS right, others left), the rack height auto-fits, and the RackLayout/customization + persistence still work (a tightened module keeps its id; default layout reproduces).
- [ ] **Task 5 — Decide the borderline cases (AC: 1)** — record the outcome for the modules flagged "borderline / grow?" in the table (CROSS MOD, WAVETABLE) — keep as-is unless the user wants them addressed here.

## Dev Notes

### How size class maps to the rendered module (the mechanics you're tuning)
- `sizeClassSpec(SizeClass)` (`Source/UI/rack/ModuleDescriptor.h`) returns `{cols, units, slotCapacity, knobSize}`: **XXS 1×1 (cap 2)**, **XS 2×1 (cap 4)**, **S 3×1 (cap 6)**, **M 4×1 (cap 8)**, **L 4×2 (cap 16)**, **XL 6×2 (cap 24)**. `cols` = the module's **grid-column footprint** (its on-rack width = `cols × columnWidth`); `units` = row height (L/XL span 2).
- **Body layout is content-driven, NOT class-driven** (AD-2, post-correct-course): `ModuleFrame::resized()` derives `nCols` from the sum of cell slots (combo = 2, knob/button = 1, display = N), independent of the class's `cols`. So shrinking a class makes the module **narrower on the grid** while the same content packs into that narrower pixel width → controls get tighter. The risk when shrinking is therefore **rotaries dropping below the AD-3 minimum** or **combos getting too narrow to read** — this is exactly what Task 3 checks by eye.
- `slotCapacity` is only a **debug guardrail** (`assertFitsClass` jassert), not the layout driver — but keep bodies at/under capacity so the assert doesn't trip in debug.
- **Knob draw width** is capped at ~62 px and centred; on a narrower module the cell width shrinks, so a knob can get smaller. AD-3 says one fixed, usable knob size — if a shrink makes knobs visibly tiny, don't do it.

### Current assignments + assessment (audit table)
| Module | Zone | Current | Controls (slots) | Verdict |
|---|---|---|---|---|
| STEREO | MASTER BUS | XS | WIDTH, TIME (2) | fits — keep |
| MASTER | MASTER BUS | XXS | VOL (1) | fits (single-control) — keep |
| OSC 1/2/3 | GEN | **M** | WAVE combo + FREQ/AMP/VOICES/DETUNE/FB (7) | **keep M** (user pref; 6 controls incl. FB) |
| CROSS MOD | GEN | S | MODE+SRC A+SRC B, 3 combos (6) | at S capacity, TIGHT — **borderline (grow to M?)**, not too big; keep unless user wants |
| SUB | GEN | S | WAVE combo + LEVEL (3) | **candidate S→XS** (2 controls) — verify combo width |
| NOISE | GEN | S | TYPE combo + AMP (3) | **candidate S→XS** (2 controls) — verify combo width |
| STRING-KARPLUS | GEN | M | PLUCK + FREQ/AMP/DAMP/STR (5) | fits M — keep |
| WAVETABLE | GEN | M | BANK combo + LOAD WAV + POS/FREQ/AMP/VOICES/DETUNE (8) | at M capacity, TIGHT — keep (not too big) |
| ENVELOPE-ADSR | MOD | L | ATK/DEC/SUS/REL + curve display (4) | needs L for the curve — keep |
| LFO | MOD | M | WAVE+TARGET combos + RATE/DEPTH (6) | fits M — keep |
| ARPEGGIATOR | MOD | M | MODE combo + RATE/OCT/GATE (5) | fits M — keep |
| FILTER | PROC | M | TYPE combo + CUTOFF/RESO (4) | **candidate M→S** — verify TYPE combo width |
| DISTORTION | PROC | M | TYPE combo + DRIVE/MIX (4) | **candidate M→S** — verify TYPE combo width |
| WAVEFOLD/BITCRUSH/CHORUS/DELAY/REVERB | PROC | S | 3 knobs (3) | fits S (3–4 controls) — keep |
| OSCILLOSCOPE | VIZ | XL | scope display | wide visualiser — keep |
| SPECTRUM | VIZ | XL | spectrum display | wide visualiser — keep |

**Proposed first pass (verify each, revert if cramped):** SUB S→XS, NOISE S→XS, FILTER M→S, DISTORTION M→S. Everything else stays.

### Precedent / history to respect
- **FILTER & DISTORTION were deliberately made M in Story 2.2** so their TYPE combo isn't cramped ("sizing is content-driven, not uniform S"). Shrinking them to S reverses that — so **verify the combo text specifically**; if "Lowpass/Highpass" or "Soft Clip/Hard Clip/Foldback" truncates, keep M.
- **Density pass `69e8fda`** already moved OSC M→S once; Self-FM (`6d6585b`) moved it back to M for the 6th control. **Do not shrink OSC.** (Memory: [[project_jass_rack_redesign]].)
- **XXS** is reserved for single-control modules (STEREO was tried on XXS and reverted to XS because the rotary got too small — AD-3). So SUB/NOISE go at most to **XS** (never XXS — they have 2 controls incl. a combo).

### Guardrails
- Change **only** the `SizeClass` argument in the `add(...)` calls (and `mix.sizeClass` for CROSS MOD if touched). No new `.cpp`, no `ModuleDescriptor.h` table change, no param/DSP/`.synthy` change (NFR2/NFR3).
- Module **ids are unchanged** (derived from title) → RackLayout persistence + customization keep matching.
- The header anatomy and the reserved info/reset/enable slots are unchanged (Epic 6). A narrower module still shows all header icons — check they don't crowd the title on XS (STEREO already lives at XS with 2 knobs, so XS headers are known-OK).
- Verification is by eye (no unit tests) — the user approves the final classes in the running app ([[feedback_ui_verification]]). Build incrementally via `build/JASS_Standalone.vcxproj` (MSBuild/PowerShell); no CMake reconfigure needed (no resource change).

### Testing standards
No unit-test framework → build + running app: for each changed module confirm (1) combo item text reads fully, (2) knobs are comfortably sized (AD-3), (3) no overlap / grid-boundary breach, (4) zones still aligned, height auto-fits. Toggle a few modules and open the customization panel to confirm layout/persistence unaffected.

### Project Structure Notes
- New Epic 7 (UI Polish) beyond Epics 1–6; file named per convention (sibling of `6-1-per-module-online-help.md`).
- No `sprint-status.yaml` (stories tracked via `epics.md` + story files); registered in `epics.md` (Epic 7 / FR24).

### References
- [Source: _bmad-output/planning-artifacts/epics.md#Epic 7 / Story 7.1] — FR24, epic definition.
- [Source: docs/Feature_Ideas.md#Backlog] — "Modul-Größen Feintuning" backlog entry (origin).
- [Source: Source/UI/rack/ModuleDescriptor.h] — `SizeClass` enum + `sizeClassSpec` table (XXS…XL) + `assertFitsClass`.
- [Source: Source/UI/rack/ModuleFrame.cpp#resized] — content-driven body layout (nCols from slots; knob width cap; combo centring).
- [Source: Source/UI/PluginEditor.cpp#buildSampleRack] — the `add(...)` calls whose `SizeClass` argument this story tunes (STEREO XS `:624`, MASTER XXS `:626`, OSC M `:634`, CROSS MOD S `:655`, SUB/NOISE S `:676`/`:678`, FILTER/DISTORTION M `:720`/`:728`, effects S, scope/spectrum XL).
- [Constraint: ARCHITECTURE-SPINE AD-2 (size-class table), AD-3 (one fixed usable knob size)].

## Dev Agent Record

### Agent Model Used

claude-opus-4-8[1m]

### Debug Log References

### Completion Notes List

### File List
