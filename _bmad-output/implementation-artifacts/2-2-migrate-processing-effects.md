---
baseline_commit: 9a69649
---

# Story 2.2: Migrate the PROCESSING effect modules

Status: review

<!-- Note: Validation is optional. Run validate-create-story for quality check before dev-story. -->

## Story

As a JASS player,
I want Filter and all effect modules confirmed as authoritative, fully-bound rack modules,
so that the processing section is consistent, complete, and preserves every control + the filter-cutoff modulation ring.

## Acceptance Criteria

1. **Coverage + anatomy.** FILTER, DISTORTION, WAVEFOLD, BITCRUSH, CHORUS, DELAY and REVERB appear in the PROCESSING zone, each with the uniform header anatomy and its assigned size class. No control, binding or behaviour is lost versus the old UI (FR12/FR13). Each module's enable is its `<x>On` bool (`filterOn`/`distortionOn`/`wavefoldOn`/`bitcrushOn`/`chorusOn`/`delayOn`/`reverbOn`).
2. **Size classes (formalized deviation from "uniform S").** FILTER = **M** and DISTORTION = **M** (they each carry a TYPE `Combo` plus two knobs; M gives the combo room); WAVEFOLD / BITCRUSH / CHORUS / DELAY / REVERB = **S** (three knobs each). This intentionally revises the epic's original "uniform S" wording — reconciled the same way the correct-course made sizing content-driven. The epics AC for Story 2.2 is annotated to match.
3. **Combo item order is correct (no index mismatch).** The FILTER TYPE combo lists `{ "Lowpass", "Highpass" }` and the DISTORTION TYPE combo lists the `distortionType` param's choices in order. Because a `ComboBoxAttachment` maps by **index**, the combo item order must equal the param's `AudioParameterChoice` order — verified: FILTER `filterType = { "Lowpass", "Highpass" }` ✓, DISTORTION `distortionType = { "SoftClip", "HardClip", "Foldback" }` ✓. (Unlike the LFO-WAVE bug in 2.1, the rack descriptors here are already correct — this AC is a guard, not a fix.)
4. **Filter cutoff modulation ring preserved (FR13, headline preservation).** The FILTER CUTOFF knob carries `modTarget = ModTarget::FilterCutoff`; when the LFO targets Filter Cutoff **and** the Filter is enabled, the rack's single-timer live-feed (`ModuleFrame::updateLiveFeed` via the rack lookup) animates the ring on that knob — end-to-end, matching the legacy behaviour (`lfoTarget` raw index 2 → `ModTarget::FilterCutoff`).
5. **Signal chain + RT rules untouched (NFR2).** This is a UI-descriptor-only story. The per-voice chain order (wavefold → filter → ADSR → distortion → bitcrush → chorus → delay → reverb) and the final `[-1,1]` clamp in `SynthVoice::renderNextBlock` are not modified. No param IDs, APVTS layout or `.synthy` format change (NFR3).
6. **Optional display polish (DISTORTION labels).** The DISTORTION TYPE combo *may* show the friendlier `{ "Soft Clip", "Hard Clip", "Foldback" }` as **display text only** (restoring the legacy readability), provided the **item order/count is unchanged** so the index mapping — and therefore the bound value + `.synthy` interop — is byte-for-byte identical. Project-context explicitly allows UI display names to differ from the canonical param/interop string. If done, add a one-line "display-only, canonical string is `SoftClip`" comment. Skippable without failing the story.
7. **Build + verify.** The project builds clean with JUCE warning flags on; the rack renders and every processing control is bound and functional in the running app.

## Tasks / Subtasks

- [x] **Task 1 — Verify + formalize the PROCESSING descriptors (AC: 1, 2, 3, 5)**
  - [x] Confirmed the 7 descriptors in `buildSampleRack` (`PluginEditor.cpp:1106-1125`): FILTER (M, `filterOn`, TYPE+CUTOFF+RESO), DISTORTION (M, `distortionOn`, TYPE+DRIVE+MIX), WAVEFOLD/BITCRUSH/CHORUS/DELAY/REVERB (S, three knobs each). All bound via frame-owned attachments (AD-6). No discrepancy — no descriptor change needed.
  - [x] FILTER TYPE `{Lowpass,Highpass}` and DISTORTION TYPE order both match their param choices (AC3) — index mapping correct.
  - [x] Stable `id`s auto-derived by `add()` (`filter`/`distortion`/`wavefold`/`bitcrush`/`chorus`/`delay`/`reverb`). Left as-is.
  - [x] Per-voice signal chain + clamp untouched (AC5) — no audio/DSP edits.
  - [x] Formalized the size deviation (FILTER/DIST=M, rest=S) in `epics.md` Story 2.2 (reconciliation note).
- [x] **Task 2 — Confirm the Filter cutoff modulation ring end-to-end (AC: 4)**
  - [x] Confirmed FILTER CUTOFF = `Kmod(P::filterCutoff, "CUTOFF", ModTarget::FilterCutoff)` (`:1111`); rack live-feed animates the ring when LFO targets Filter Cutoff and `filterOn` is true (1.4 mechanism, `ModuleFrame::updateLiveFeed`). No gap — no code change.
- [x] **Task 3 — DISTORTION combo display labels (AC: 6)**
  - [x] Changed the DISTORTION TYPE combo display text to `{ "Soft Clip", "Hard Clip", "Foldback" }` (order/count unchanged → index mapping + `.synthy` interop identical). Added a comment: display-only, canonical param string stays `SoftClip`/`HardClip`.
- [x] **Task 4 — Build + in-app verification (AC: all)**
  - [x] Incremental Release build via `build/JASS_Standalone.vcxproj` (MSBuild, VS2022). Only `PluginEditor.cpp` recompiled → `JASS.exe`. No new files ⇒ no CMake change. Build clean (no warnings/errors).
  - [x] App relaunched for live verification (user confirms per [[feedback-ui-verification]]).

## Dev Notes

### What this story actually is

Like 1.5 and 2.1, the throwaway sample rack (`buildSampleRack`) **already** renders all 7 PROCESSING modules as descriptors bound to the real params — and a full audit (2026-07-05) found them **complete and correct**: every legacy control is present, both TYPE combos are in the right index order, and the FILTER CUTOFF mod ring is tagged + wired end-to-end. So 2.2 is honestly a **verification + formalization** story, not a rebuild:

- **Formalize the size-class deviation** (FILTER/DISTORTION = M, not "uniform S") — the one spec-vs-code gap, resolved by revising the wording (AC2 + an epics annotation), consistent with the correct-course making sizing content-driven.
- **Verify** coverage, bindings, combo order, and the filter-cutoff ring in the running app.
- **Optionally** restore the friendlier DISTORTION display labels (display-only, interop-safe).

Expect **little or no C++ change** (Task 3 is the only optional edit). That is the correct outcome — the prototype did the migration; this story ratifies it and proves it in-app. The legacy effect panels (`EffectPanel` reuse, inline Filter/Distortion) stay behind the opaque rack until **Story 3.3**.

### Audit results (2026-07-05) — per module

| Module | Size | Enable | Body (paramIds) | Notes |
| --- | --- | --- | --- | --- |
| FILTER | M | `filterOn` | `filterType`{Lowpass,Highpass} + `filterCutoff`(modTarget=FilterCutoff) + `filterReso` | combo order ✓; ring wired ✓ |
| DISTORTION | M | `distortionOn` | `distortionType`{SoftClip,HardClip,Foldback} + `distortionDrive` + `distortionMix` | combo order ✓ |
| WAVEFOLD | S | `wavefoldOn` | `wavefoldDrive` + `wavefoldSymmetry` + `wavefoldMix` | complete |
| BITCRUSH | S | `bitcrushOn` | `bitcrushBits`(Int1-16) + `bitcrushRate`(Int1-50) + `bitcrushMix` | complete |
| CHORUS | S | `chorusOn` | `chorusRate` + `chorusDepth` + `chorusMix` | complete |
| DELAY | S | `delayOn` | `delayTime` + `delayFeedback`(label "FB", legacy "FDBK") + `delayMix` | label-only diff, no impact |
| REVERB | S | `reverbOn` | `reverbRoom` + `reverbDamp` + `reverbMix` | complete |

- **Filter-cutoff ring:** legacy path was `cutoffKnob.setModAmount((lfoActive && target==2 && filterOn) ? lfo : 0)` (`PluginEditor.cpp` ~`:849`). The rack does the equivalent generically via `ModuleFrame::updateLiveFeed` (~`ModuleFrame.cpp:375`) when `activeTarget == ModTarget::FilterCutoff`. `lfoTarget` raw index 2 (Filter Cutoff) maps to `ModTarget::FilterCutoff` with the +1 offset (`ModTarget = rawLfoTarget + 1`, None = 0). End-to-end, gated by `filterOn`.
- **Signal chain (do NOT touch):** `SynthVoice::renderNextBlock` — wavefold (before filter) → amplitude LFO → filter → ADSR → distortion → bitcrush → chorus → delay → reverb → `std::clamp(x, -1, 1)` (final safety, ~`SynthVoice.cpp:159`). Effects run per-voice, mono, widened at the master `StereoWidth` stage. UI-only story: untouched.

### The DISTORTION label nuance (AC6) — why it's safe

Project-context: "Enum strings = C# enum member names, no display spaces (`SoftClip`, not `Soft Clip`)… UI display names may differ, but the persisted/interop string must be the canonical one." The `ComboBoxAttachment` binds the combo's **selected index** to the param choice index; the displayed item **text is cosmetic**. So showing "Soft Clip" (index 0) while the param/`.synthy` still stores `SoftClip` (index 0) is legitimate and changes nothing on disk. Constraint: keep **3 items in the same order** — never reorder or add/remove, or the index mapping (and interop) breaks. (This is the inverse of the 2.1 LFO-WAVE bug, where the *order* was wrong; here order is right and only the label prettiness is in question.)

**Note (not this story):** the *legacy* DISTORTION combo shows "Soft Clip"/"Hard Clip" too — harmless display text, and the legacy panel is deleted in Story 3.3. Don't touch legacy here.

### Files to touch

- **`Source/UI/PluginEditor.cpp`** — only if Task 3 (optional DISTORTION display labels) is done; otherwise no code change.
- **`_bmad-output/planning-artifacts/epics.md`** — annotate Story 2.2's AC to formalize FILTER/DISTORTION = M (reconciliation note, like Story 1.1's), so the planning artifact matches the built code.

### Guardrails (project-context + ADs)

- **No new params / no `.synthy` change (NFR3):** every processing param already exists; add/rename nothing. Task 3, if done, is display-text only.
- **Frame owns attachments (AD-6):** all effect knobs/combos bind via the frame from `paramId`; no `*Attachment` members in the editor.
- **RT-safety (NFR2):** UI-only; the audio-thread signal chain and clamp are not touched.
- **Don't touch layout/anatomy or the grid** — sizes are set (M/M/S×5); don't reopen `resized()`/`kHu`.
- **Don't delete legacy processing code** — that's Story 3.3. Leave `EffectPanel`/inline Filter/Distortion behind the opaque rack.
- **Naming dualism:** keep `Synthy*` / `rack::` / `buildSampleRack`.

### Project Structure Notes

- Descriptor assembly stays in `PluginEditor` (AD-1 layer map). Auto-derived `id`s (`filter`/`distortion`/…) are the future layout-persistence keys (Spine "Deferred"); unused today.

### Previous-story intelligence (1.5 / 2.1 + review)

- **2.1 pattern reused:** sample descriptors → authoritative + verify; legacy stays behind the opaque rack until 3.3.
- **2.1's key lesson — combo index vs param order — was re-checked here** and both PROCESSING combos passed (no fix needed). The OSC-WAVE follow-up from 2.1's `deferred-work.md` is unrelated to this zone.
- **1.4 mod-ring mechanism reused verbatim** for the filter-cutoff ring; no new ring wiring.
- The Sonnet-5 review of 2.1 approved the `sampleOwned`/`add()` pattern; nothing here changes those mechanics.

### Verification

Build + launch; the user confirms behaviour live ([[feedback-ui-verification]] — don't reflexively re-read a screenshot). Manual checks in Task 4. No unit-test framework in this project.

### References

- [Source: _bmad-output/planning-artifacts/epics.md#Story 2.2] (PROCESSING migration; filter-cutoff ring)
- [Source: ARCHITECTURE-SPINE.md#AD-2/AD-4/AD-6/AD-8] (size-class table content-driven; control vocabulary; frame owns binding; declarative mod rings)
- [Source: _bmad-output/project-context.md] (APVTS single source, RT/signal-chain rules, enum-string canonicalization, no param/format change)
- Code: rack PROCESSING descriptors `PluginEditor.cpp` (FILTER/DIST at M with TYPE combos; WAVEFOLD…REVERB at S). Legacy filter/distortion panels `PluginEditor.cpp:428-485`; filter-cutoff ring legacy path `~:849`; rack live-feed `ModuleFrame.cpp:~375`. Signal chain + clamp `SynthVoice.cpp:~87-159`.
- Params: `Parameters.h` — FILTER `:35-38` (`filterType` choices `:184`), DISTORTION `:41-44` (`distortionType` choices `:190`), WAVEFOLD `:47-50`, BITCRUSH `:53-56`, DELAY `:77-80`, CHORUS `:83-86`, REVERB `:96-99`.

## Dev Agent Record

### Agent Model Used

claude-opus-4-8[1m] (Opus 4.8, 1M context)

### Debug Log References

- Incremental Release build (MSBuild, VS2022) — clean; only `PluginEditor.cpp` recompiled → `JASS.exe`. No warnings/errors.
- App relaunched (`JASS.exe`) for live in-app verification per [[feedback-ui-verification]].

### Completion Notes List

- **Verification story — the PROCESSING descriptors were already authoritative + correct.** A full audit (2026-07-05) confirmed all 7 modules complete, bound, with correct combo index order and the filter-cutoff mod ring wired end-to-end. So (unlike 2.1's LFO-WAVE fix) no correctness fix was required.
- **Formalized the size deviation (AC2):** FILTER + DISTORTION = M (TYPE combo needs room), rest = S. Annotated `epics.md` Story 2.2 so the planning artifact matches the built code (reconciliation, consistent with the correct-course AD-2 content-driven sizing).
- **DISTORTION display polish (AC6):** combo now shows "Soft Clip"/"Hard Clip"/"Foldback" — **display text only**; order/count unchanged so the `ComboBoxAttachment` index mapping and the canonical `.synthy` strings (`SoftClip`/`HardClip`) are byte-for-byte identical (project-context sanctions UI display ≠ canonical string). Restores the legacy readability.
- **No engine/param/format change (AC5):** signal chain + `[-1,1]` clamp in `SynthVoice` untouched; no param IDs / APVTS / `.synthy` change. Legacy processing panels NOT deleted (Story 3.3).
- **Filter-cutoff ring (AC4):** confirmed tagged + wired; `lfoTarget` raw index 2 → `ModTarget::FilterCutoff`, gated by `filterOn`.

### File List

- `Source/UI/PluginEditor.cpp` (UPDATE) — DISTORTION TYPE combo display labels → "Soft Clip"/"Hard Clip"/"Foldback" (display-only, index/order preserved).
- `_bmad-output/planning-artifacts/epics.md` (UPDATE) — Story 2.2 AC annotated: FILTER/DISTORTION = M, rest = S (formalized deviation from "uniform S").

## Change Log

- 2026-07-05 — Story 2.2: PROCESSING zone verified as authoritative (all 7 effect descriptors complete + correct; filter-cutoff ring confirmed). DISTORTION combo display labels restored ("Soft Clip"/"Hard Clip", display-only). Size deviation (FILTER/DIST=M) formalized in epics. Build clean. Status → review.
