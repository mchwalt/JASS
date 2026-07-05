---
baseline_commit: 8c6cf54
---

# Story 3.4: Validate VST3 parity

Status: ready-for-dev

<!-- Note: Validation is optional. Run validate-create-story for quality check before dev-story. -->

## Story

As a JASS developer,
I want the VST3 editor verified in a host,
so that the rack UI is confirmed to work as a plugin, not only standalone.

## Acceptance Criteria

1. **Given** a Release build of the JASS VST3, **when** it is loaded in REAPER v7.65, **then** the rack editor renders and operates identically to the standalone (modules, controls, displays, rings).
2. **And** any divergence from the standalone is recorded as a follow-up (NFR4; not a launch blocker).

## Tasks / Subtasks

- [ ] **Task 1 — Build the VST3 (dev)** — Release build of `build/JASS_VST3.vcxproj` (MSBuild). Output: `build/JASS_artefacts/Release/VST3/JASS.vst3`.
- [ ] **Task 2 — Make it loadable by REAPER (dev)** — copy `JASS.vst3` to a VST3 folder REAPER scans (system `C:\Program Files\Common Files\VST3\`, or add the build path to REAPER Prefs → Plug-ins → VST → VST plug-in paths), then rescan.
- [ ] **Task 3 — In-host parity check (USER, in REAPER v7.65)** — insert JASS on a track, open the editor, and compare against the standalone:
  - Rack renders: all four zones (MASTER BUS / GENERATORS / MODULATION / PROCESSING), every module, correct sizes/layout.
  - Controls work: knobs, combos, enable toggles, reset ↺; values move params (audible).
  - Displays live: ADSR curve reshapes, oscilloscope + spectrum animate on a played note; scope zoom combo works.
  - Modulation rings animate (LFO → OSC FREQ/AMP, FILTER CUTOFF).
  - Interactions: PLUCK button + spacebar; preset SAVE/LOAD/RANDOM/RESET + "Current State"; keyboard plays; z/x octave.
  - New enables: MASTER mute, ADSR bypass, MIX MODE additive; Mix-Mode dims when an OSC is off.
- [ ] **Task 4 — Record divergences (dev + user)** — note any difference vs standalone (rendering, sizing, DPI, host-threading quirks, param automation) in `deferred-work.md` as NFR4 follow-ups. Not a launch blocker.

## Dev Notes

### What this story is

The rack editor is the same `SynthyEditor` for both formats (JUCE builds Standalone + VST3 from one `juce_add_plugin`, `FORMATS Standalone VST3`), so parity is *expected* — but VST3 runs the editor inside a host (different window ownership, DPI/scaling, message-thread timing, parameter automation via the host). This story is the **validation** that nothing rack-specific misbehaves as a plugin. It is primarily a **user-run host check**; the dev part is building + placing the VST3 and recording findings.

### Build + placement

- Build target: `build/JASS_VST3.vcxproj` (Release, x64). Output `build/JASS_artefacts/Release/VST3/JASS.vst3` (a bundle folder). `COPY_PLUGIN_AFTER_BUILD` is **not** set in CMake, so it is not auto-installed.
- REAPER (v7.65) must scan the folder: either copy the `JASS.vst3` bundle into `C:\Program Files\Common Files\VST3\` (may need admin) or add `…\build\JASS_artefacts\Release\VST3` under REAPER → Options → Preferences → Plug-ins → VST → "VST plug-in paths", then "Re-scan".
- The `.synthy` preset format + `%AppData%\Synthy` state are shared with the standalone, so presets/state carry over.

### Guardrails

- **No code change expected.** This is validation. If the VST3 reveals a real editor bug (e.g. a rack path that assumes standalone-only behaviour), fix it minimally and note it; otherwise record divergences as NFR4 follow-ups (not blockers).
- Same audio engine + params + `.synthy` in both formats (NFR2/NFR3) — unchanged.
- Known context to watch: fixed target window (~1520 wide) — check the editor fits / scales acceptably in the host; the auto-fit-down from Story 1.3 should apply.

### References

- [Source: _bmad-output/planning-artifacts/epics.md#Story 3.4] (VST3 parity; NFR4; not a launch blocker)
- [Source: ARCHITECTURE-SPINE.md#Deferred] (VST3 editor parity — validate after standalone)
- Code: one `SynthyEditor` for both formats; `CMakeLists.txt:13` (`FORMATS Standalone VST3`).

## Dev Agent Record

### Agent Model Used

claude-opus-4-8[1m] (Opus 4.8, 1M context)

### Debug Log References

### Completion Notes List

### File List
