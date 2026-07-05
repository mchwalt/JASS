---
baseline_commit: 45f90dc
---

# Story 3.1: Global header chrome + MASTER BUS modules

Status: review

<!-- Note: Validation is optional. Run validate-create-story for quality check before dev-story. -->

## Story

As a JASS player,
I want preset controls in a fixed top header and Master + Stereo as rack modules in a MASTER BUS zone,
so that global preset actions sit apart from the rack while Master/Stereo follow the same "one mold" as every other module.

## Acceptance Criteria

1. The header shows SAVE / LOAD / RANDOM / RESET, a centred title, and the preset-name / "Current State" indicator, outside the module grid — the legacy header Master/Stereo controls are gone.
2. Master and Stereo are **rack module descriptors in the MASTER BUS zone** (top row), built on the framework like every other module.
3. All preset actions work as before and the "Current State" indicator still reflects unsaved changes.
4. Master and Stereo are bound to their existing params (no audio behaviour change).

## Tasks / Subtasks

- [x] **Task 1 — Verify the header chrome is complete + correct (AC: 1, 3)** — SAVE/LOAD/RANDOM/RESET buttons (`saveBtn`/`loadBtn`/`randomBtn`/`resetBtn`) exist and are wired; centred "J A S S" title + subtitle drawn in `paint()`; `presetNameLabel` shows "Preset: <name>" or "Current State", driven by `processor.isPresetModified()` polled at 30 Hz. Header is a fixed 64 px band outside the rack grid.
- [x] **Task 2 — Verify MASTER + STEREO are MASTER BUS rack modules; no legacy header controls remain (AC: 2, 4)** — `buildSampleRack` adds `STEREO` (XS, `stereoOn`, WIDTH/TIME) and `MASTER` (XXS, `masterOn`, VOL) in `Rack::Zone::MasterBus`. No editor members for `masterVol`/`stereoWidth`/`stereoTime` remain (they exist only as params). Legacy header Master/Stereo controls already removed (correct-course 2026-07-01).
- [x] **Task 3 — Verify preset actions still work (AC: 3)** — SAVE → `PresetIO::saveToFile` + `markPresetClean` + name; LOAD → `PresetIO::loadFromFile` + `markPresetClean` + name; RANDOM → `processor.randomize()`; RESET → `processor.resetToDefault()`. "Current State" flips to a preset name after save/load/reset and back to "Current State" on edit.
- [x] **Task 4 — Build + in-app verification (AC: all)** — verified in the running app (post-`45f90dc` build): header renders with title + preset cluster + indicator; MASTER/STEREO sit in the MASTER BUS zone; SAVE/LOAD/RANDOM/RESET function; indicator tracks unsaved changes.

## Dev Notes

### What this story actually is

**Already satisfied — no code change.** The AC for 3.1 was implemented during the 2026-07-01 correct-course prototype (formalised in `sprint-change-proposal-2026-07-01.md`): the header was flattened, Master/Stereo were moved out of the header into MASTER BUS rack modules, the title was centred, and "Current State" moved into the preset cluster. The recon (2026-07-05) confirmed every AC clause is present and functional in the current code. This story is a **verification + traceability** record closing FR14's header/MASTER-BUS clause; the remaining FR14 keyboard clause is Story 3.2 and the legacy-control deletion is Story 3.3.

### Current state (recon 2026-07-05)

- **Header chrome** (`PluginEditor.cpp` `resized()` ~`:1154-1171`, `paint()` ~`:909-919`, ctor ~`:551-598`): 64 px band. `saveBtn`/`loadBtn`/`randomBtn`/`resetBtn` (2×2 cluster, left ~340 px), centred "J A S S" + "Just Another Simple Synthesizer" title, `presetNameLabel` right of the cluster.
- **"Current State"** (`PluginEditor.cpp` `updatePresetLabel()` ~`:850-860`, `timerCallback` 30 Hz): `processor.isPresetModified()` vs `cleanSnapshot` value-compare (`PluginProcessor.cpp` `markPresetClean`/`isPresetModified` ~`:170-186`).
- **MASTER BUS modules** (`buildSampleRack` ~`:1022-1025`): STEREO (XS, `stereoOn`), MASTER (XXS, `masterOn`). Header members for Master/Stereo removed (comment at `PluginEditor.h` ~`:222-223`).
- **Preset plumbing** (`PresetIO.h` `saveToFile`/`loadFromFile`/`toVar`/`applyVar`; `PluginProcessor.cpp` `randomize` ~`:91-149`, `resetToDefault` ~`:151-166`).

### Guardrails

- Verification-only story; **no code edits**. If in-app verification surfaced a defect, it would be fixed here — none did.
- Legacy per-module layout (OscillatorPanel/EffectPanel/inline panels) still sits behind the opaque rack — **its removal is Story 3.3**, not here.
- Keyboard restyle is **Story 3.2**.

### References

- [Source: _bmad-output/planning-artifacts/epics.md#Story 3.1] and [#FR14]
- [Source: _bmad-output/planning-artifacts/architecture/architecture-JASS-2026-06-28/sprint-change-proposal-2026-07-01.md] (Master/Stereo → MASTER BUS; header flattened; "Current State" in cluster)
- Code: `PluginEditor.cpp` header/paint/ctor (see recon lines above); `buildSampleRack` MASTER BUS; `PluginProcessor.cpp` preset actions + modified-flag; `PresetIO.h` save/load.

## Dev Agent Record

### Agent Model Used

claude-opus-4-8[1m] (Opus 4.8, 1M context)

### Debug Log References

- No build (verification-only story; zero code changes). Verified against the running `45f90dc` build.

### Completion Notes List

- **Zero code changes** — every AC was already implemented by the 2026-07-01 correct-course prototype and confirmed present/functional by the 2026-07-05 recon and in-app check. Story closed as verification/traceability for FR14's header + MASTER-BUS clause.
- Master/Stereo confirmed as MASTER BUS rack modules (STEREO XS `stereoOn`, MASTER XXS `masterOn`); no legacy header Master/Stereo controls remain.
- Preset SAVE/LOAD/RANDOM/RESET + "Current State" (via `isPresetModified`/`cleanSnapshot`) confirmed working.
- Follow-ups tracked elsewhere: keyboard restyle = Story 3.2; legacy layout deletion = Story 3.3.

### File List

- _(none — verification-only story)_

## Change Log

- 2026-07-05 — Story 3.1: verified complete (no code change). Header chrome (SAVE/LOAD/RANDOM/RESET, centred title, "Current State"), MASTER/STEREO as MASTER BUS rack modules, and preset actions were all already implemented by the 2026-07-01 correct-course. Status → review.