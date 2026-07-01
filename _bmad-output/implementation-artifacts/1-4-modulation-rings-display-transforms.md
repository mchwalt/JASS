---
baseline_commit: b00f1efd6faaba0fdee70332717fa4fa349e6b5a
---

# Story 1.4: Live modulation rings & display-value transforms

Status: review

<!-- Note: Validation is optional. Run validate-create-story for quality check before dev-story. -->

## Story

As a JASS player,
I want knobs to show live modulation rings and the FREQ knob to show the actually-played frequency,
so that the new rack preserves the existing visual feedback exactly.

## Acceptance Criteria

1. **Modulation rings (AD-8).** Given a `Knob` declares a `modTarget` (`Frequency | Amplitude | FilterCutoff`), when the LFO targets that destination, a **single editor timer** reads the processor's LFO atomic + active target and animates the ring (`SynthySlider::setModAmount`) on **every knob the rack reports for that `modTarget`** — and only while that knob's module is enabled (a disabled OSC / bypassed filter shows no ring). Non-matching knobs are driven to `0`. Rings repaint only on meaningful change (already guarded in `SynthySlider::setModAmount`, NFR5).
2. **Display-value transform (AD-4, FR4).** Given a `Knob` with a fully-set `displayTransform` pair, the knob shows the derived value (`FREQ = base × played ratio`) and writes the **base** back to its APVTS param on edit.
3. **Guarded transform.** When the played ratio `≤ 0` (no note sounding) — or the knob's module is disabled — the transform is **identity** (the knob shows its base value) and **no write-back** occurs (no divide-by-zero, base param never corrupted).
4. **No regressions / RT + state rules.** The mechanism is UI-only: no new params, no `.synthy`/APVTS change, no allocation or locking on the audio thread; the UI reads processor `std::atomic`s only (NFR2/NFR3). The project builds clean with JUCE warning flags on.

## Tasks / Subtasks

- [x] **Task 1 — ModuleFrame: display-value transform wiring (AC: 2, 3)**
  - [x] In `buildBody()`, branch the `Knob` case: if the knob carries a **fully-set** transform (both `toDisplay` AND `fromDisplay` non-empty), build it **DECOUPLED** — do NOT create a `SliderAttachment` for it. For a knob with no transform (or a half-set pair), keep today's `SliderAttachment` path unchanged.
  - [x] For a transform knob: mirror the parameter's range onto the slider via `slider->setNormalisableRange(...)` derived from the param's `getNormalisableRange()` (range + skew, `float`→`double`) so the decoupled knob spans/feels identical to the param. Do NOT hardcode 20–10000/skew 0.3 — read it from the param generically.
  - [x] Set the slider's `onValueChange` to write the base back: `effRatio = (moduleEnabled && liveRatio > 0.0) ? liveRatio : 1.0; base = fromDisplay(slider->getValue(), effRatio);` then `p->setValueNotifyingHost(p->convertTo0to1((float) base));` — skip if `p` is null.
  - [x] Add `void ModuleFrame::setPlayedRatio(double ratio)` (or fold into `updateLiveFeed`, Task 3): store `liveRatio = ratio`; for each transform knob, if `! slider->isMouseButtonDown()`, set `slider->setValue(toDisplay(base, effRatio), juce::dontSendNotification)` where `base = *apvts.getRawParameterValue(paramId)` and `effRatio` as above. (Do not fight the user mid-drag — mirrors legacy `OscillatorPanel::setPlayedRatio`.)
- [x] **Task 2 — ModuleFrame: modulation-ring exposure + application (AC: 1)**
  - [x] In `buildBody()`, collect the frame's ring knobs: for each `Knob` with `modTarget != None`, record `{ SynthySlider*, ModTarget }`.
  - [x] Add ring application inside `updateLiveFeed` (Task 3): for each ring knob, `slider->setModAmount((lfoOn && moduleEnabled && knob.target == activeTarget) ? lfoValue : 0.0f);` — `moduleEnabled = (enableValue == nullptr) || enableValue->load() >= 0.5f`.
- [x] **Task 3 — ModuleFrame + Rack: the live-feed fan-out (AD-8) (AC: 1, 2)**
  - [x] `ModuleFrame::updateLiveFeed(bool lfoOn, ModTarget activeTarget, float lfoValue, double playedRatio)` — applies rings (Task 2) + transforms (Task 1) in one pass. Cheap; called from the single editor timer.
  - [x] `Rack::updateLiveFeed(bool lfoOn, ModTarget activeTarget, float lfoValue, double playedRatio)` — fans out to every owned `ModuleFrame`. (This IS the AD-8 "rack owns the lookup" primitive, applied. Optionally also expose `std::vector<SynthySlider*> knobsWithModTarget(ModTarget)` if a raw lookup is wanted, but the fan-out method is sufficient and keeps enable-gating inside the frame.)
- [x] **Task 4 — PluginEditor: the single live-feed timer (AC: 1, 2, 3)**
  - [x] In the editor's existing `timerCallback()` (already `startTimerHz(30)`), after the legacy `osc*.setPlayedRatio` block, read: `lfoOn = *apvts.getRawParameterValue("lfoOn") > 0.5f;` `int rawTgt = (int) *apvts.getRawParameterValue("lfoTarget");` `float lfo = processor.getLfoDisplayValue();` `double ratio = processor.getCurrentNoteRatio();`
  - [x] Map the raw target to `ModTarget` with the **+1 offset** (raw 0/1/2 = Frequency/Amplitude/FilterCutoff; `ModTarget{None=0,Frequency=1,Amplitude=2,FilterCutoff=3}`): `ModTarget active = lfoOn ? static_cast<ModTarget>(rawTgt + 1) : ModTarget::None;`
  - [x] Call `sampleRack->updateLiveFeed(lfoOn, active, lfo, ratio);`
- [x] **Task 5 — Sample-rack verification wiring (throwaway; AC: 1, 2, 3)**
  - [x] So 1.4 is verifiable in-app before generators/filter are migrated (1.5 / 2.2), enrich a few **sample** descriptor knobs (thrown away when 1.5/2.2 build the real descriptors): give the sample **OSC FREQ** knobs `modTarget = Frequency` + the FREQ transform (`toDisplay = base*ratio`, `fromDisplay = shown/ratio`); **OSC AMP** `modTarget = Amplitude`; **FILTER CUTOFF** `modTarget = FilterCutoff`. Add a small `Knob` builder overload (e.g. `Kmod(id,label,modTarget)` and a `Kfreq(id,label)` for the transform) so the descriptor list stays readable.
- [x] **Task 6 — Build + in-app verification (AC: 1, 2, 3, 4)**
  - [x] Incremental Release build via `build/JASS_Standalone.vcxproj` (MSBuild, PowerShell). No new `.cpp` files ⇒ no CMake change (edits are to existing `ModuleFrame.*`, `Rack.*`, `PluginEditor.cpp`).
  - [x] Screenshot verify: enable the LFO, set TARGET = Frequency → the sample OSC FREQ knobs show a moving ring; switch TARGET = Filter Cutoff → the FILTER CUTOFF knob shows the ring, OSC rings drop to 0. Disable an OSC → its ring stops. Play a note (on-screen keyboard) → the OSC FREQ knob display tracks the played frequency; release → returns to base. Drag a FREQ knob while a note sounds → the base param updates correctly (no jump/corruption).

## Dev Notes

### The two mechanisms this story generalizes (both exist in the legacy editor — replicate exactly, FR13)

**A) Modulation rings.** `SynthySlider` already carries the ring: `setModAmount(float -1..+1)` with a built-in change threshold (`Source/UI/SynthySlider.h:33-37`). The legacy editor drives it from ONE 30 Hz timer (`Source/UI/PluginEditor.cpp:821-849`):
- reads `processor.getLfoDisplayValue()` (`std::atomic<float> lfoDisplayValue`, `Source/PluginProcessor.h:59,84`),
- reads `lfoOn` + `lfoTarget` from APVTS (raw target int: **0 Frequency / 1 Amplitude / 2 FilterCutoff**),
- gates each knob by its module's on-flag (`osc1On`, `filterOn`, …): `setModAmount((active && moduleOn) ? lfo : 0)`.
This story moves that fan-out behind `Rack::updateLiveFeed` → `ModuleFrame::updateLiveFeed`, where the frame already knows its enable atomic (`enableValue`, `Source/UI/rack/ModuleFrame.h:55`; loaded at `ModuleFrame.cpp:28`).

**B) FREQ display transform.** The legacy `OscillatorPanel` FREQ knob is **DECOUPLED from the param — NO SliderAttachment** (`Source/UI/PluginEditor.cpp:86-106,137-147`):
- range/skew mirror the param (`setRange(20,10000,1); setSkewFactor(0.3)`),
- `setPlayedRatio(ratio)` sets the DISPLAY value `= base × er` (`er = enabled ? ratio : 1.0`), and **bails while the knob is dragged** (`isMouseButtonDown()`),
- `onValueChange` writes the base back: `base = shown / jmax(0.0001, er)` → `setValueNotifyingHost(convertTo0to1(base))`.
The rack expresses this generically via the `Knob.toDisplay/fromDisplay` guarded pair (already defined, `Source/UI/rack/ModuleDescriptor.h:39-50`). The `ratio ≤ 0 ⇒ identity + no write-back` guard (AD-4, `ModuleDescriptor.h:44-46`) is enforced HERE (the closures stay pure; the frame supplies `effRatio`). For FREQ: `toDisplay = [](b,r){ return b*r; }`, `fromDisplay = [](s,r){ return s/r; }`.

### Files to touch (all UPDATE — no NEW files, no CMake change)

- **`Source/UI/rack/ModuleFrame.h` / `.cpp`** — the core of this story.
  - `buildBody()` (`ModuleFrame.cpp:62-149`) currently ALWAYS adds a `SliderAttachment` for every `Knob` (`:77-78`). Branch it: transform-knob ⇒ decoupled (no attachment) + `onValueChange` write-back + mirrored `NormalisableRange`; plain knob ⇒ unchanged.
  - Add member stores: `std::vector<{SynthySlider*, ModTarget}> ringKnobs;` and `std::vector<{SynthySlider*, juce::String paramId, toDisplay, fromDisplay}> xformKnobs;` plus `double liveRatio = 1.0;`.
  - Add `void updateLiveFeed(bool lfoOn, ModTarget activeTarget, float lfoValue, double playedRatio);` (public).
  - Keep the existing enable-dim `Timer` (`ModuleFrame.cpp:298-303`) as-is — it polls only the frame's OWN enable atomic (cheap, local). The LFO/ratio live-feed is a SEPARATE, editor-driven push (AD-8 wants ONE timer reading the processor atomics; per-frame timers must NOT each read the LFO atomic).
- **`Source/UI/rack/Rack.h` / `.cpp`** — add `void updateLiveFeed(bool, ModTarget, float, double);` iterating `frames` (`Rack.h:69`). The header comment already flags "Modulation-ring lookup (AD-8) is added in Story 1.4" (`Rack.h:14`). Update the stale comment about an "8-column grid" if you touch it (`Rack.h:9` — it is 12 now).
- **`Source/UI/PluginEditor.cpp`** — extend `SynthyEditor::timerCallback()` (`:821-854`) to also call `sampleRack->updateLiveFeed(...)` (Task 4); enrich sample descriptors (Task 5, `:1010-1049`). The editor already owns the 30 Hz timer and the `sampleRack` (`:974`).

### Critical guardrails (from project-context.md + AD-4/AD-8)

- **A transform knob MUST NOT get a `SliderAttachment`** — the attachment would drive the slider from the normalized param and fight the displayed `base×ratio`. This is the #1 way to get this wrong.
- **`ModTarget` has a +1 offset vs the raw `lfoTarget` param** (`ModTarget::None = 0`; raw target 0 = Frequency). Map with `static_cast<ModTarget>(rawTgt + 1)` only when `lfoOn`.
- **Divide-by-zero guard:** never divide by `ratio` when `ratio ≤ 0`; use identity (`effRatio = 1.0`) and suppress write-back (AC3). Legacy uses `jmax(0.0001, er)` — the `>0` branch is equivalent and clearer.
- **Audio-thread safety (NFR2):** all reads are `processor.getLfoDisplayValue()` / `getCurrentNoteRatio()` (`std::atomic<float>`, `PluginProcessor.h:59,54,84,97`) on the UI timer. No writes to the audio thread; write-back goes through APVTS `setValueNotifyingHost` (message thread), exactly as legacy.
- **No new params / no `.synthy` change (NFR3):** this is pure UI wiring. `modTarget` + transforms are descriptor data, not parameters.
- **Reset interaction:** `doReset()` (`ModuleFrame.cpp:151-172`) writes param defaults via `setValueNotifyingHost`. For a transform knob that is decoupled, the display refresh happens on the next live-feed tick (30 Hz) — acceptable (legacy explicitly re-called `setPlayedRatio` after reset, `PluginEditor.cpp:77`; you may call the frame's transform-refresh at the end of `doReset()` for immediacy, optional).

### Project Structure Notes

- Framework code stays under `Source/UI/rack/` (AD-1 layer map). No files move; no `target_sources` change (only existing `.cpp` edited).
- `SynthySlider` is the only knob (AD-7); do not subclass it — the ring API already exists on it.
- Naming: keep `Synthy*`/`rack::` conventions. New members `camelCase`, constants `k`-prefixed if any.

### Previous-story intelligence (1.2 / 1.3 + deferred review)

- The frame **owns all APVTS attachments** (AD-6) — this story adds a case that deliberately owns NO attachment for a knob (the decoupled transform knob). Keep the plain-knob path byte-for-byte.
- Deferred 1.2 review carry-over (still open, `deferred-work.md`): _unguarded attachment construction against a bad paramId_ — the decoupled write-back already null-checks `getParameter(id)`; keep that guard. `Display` null-safety pattern (`ModuleFrame.cpp:142`) is the model for defensive checks.
- 1.3 established the `ModuleFrame::resized()` content-derived grid; **do not touch layout** in this story — rings/transforms are paint/value concerns, not geometry.

### Verification (no unit-test framework in this project)

Verification = **Release build + in-app screenshot** (PowerShell `CopyFromScreen` into the scratchpad), per project-context "Build & Workflow". See Task 6 for the exact checks. Optionally also load the VST3 in REAPER, but standalone is the quick smoke test (VST3 parity is Story 3.4).

### References

- [Source: _bmad-output/planning-artifacts/epics.md#Story 1.4]
- [Source: _bmad-output/planning-artifacts/architecture/architecture-JASS-2026-06-28/ARCHITECTURE-SPINE.md#AD-4] (guarded displayTransform pair) and #AD-8 (declarative rings; single editor timer; rack owns the lookup)
- [Source: _bmad-output/planning-artifacts/prds/prd-JASS-2026-06-28/prd.md#FR4] (per-knob display transform; FREQ = base × played ratio)
- [Source: _bmad-output/project-context.md] (RT-audio rules, atomic handoff, APVTS single source of truth, build workflow)
- Legacy patterns to replicate: `Source/UI/PluginEditor.cpp:60-147` (OscillatorPanel FREQ transform), `:821-849` (single-timer ring routing). Atomics: `Source/PluginProcessor.h:54,59,84,97`. Ring API: `Source/UI/SynthySlider.h:33-37`. Descriptor fields: `Source/UI/rack/ModuleDescriptor.h:35-50`.

## Dev Agent Record

### Agent Model Used

claude-opus-4-8[1m] (Opus 4.8, 1M context)

### Debug Log References

- Incremental Release build (`build/JASS_Standalone.vcxproj`, MSBuild) — clean, no errors/warnings; `PluginEditor.cpp`, `ModuleFrame.cpp`, `Rack.cpp`, `PluginProcessor.cpp` recompiled → `JASS.exe`.
- In-app verification (standalone launch + screen capture): rack renders fully; decoupled FREQ knobs construct and show their base value (no crash/misrender). With the LFO on (TARGET=Amplitude) and OSC enabled (auto-play drone), the AMP modulation ring animates — **confirmed live by the user** ("ring animiert funktioniert").

### Completion Notes List

- **Rings (AC1):** knobs with `modTarget != None` are collected per frame; the single editor `timerCallback` reads `lfoOn`/`lfoTarget`/`lfoDisplayValue`, maps the raw target to `ModTarget` with the **+1 offset** (`None=0`), and calls `Rack::updateLiveFeed` → each `ModuleFrame` sets `SynthySlider::setModAmount` on matching knobs, gated by its own enable. Non-matching/disabled → 0. Verified live.
- **Transform (AC2/AC3):** a `Knob` with a fully-set `toDisplay`/`fromDisplay` pair is built **decoupled** (no `SliderAttachment`); its range/skew mirror the param (`getNormalisableRange`), `onValueChange` writes `fromDisplay(shown, effRatio)` back via `setValueNotifyingHost(convertTo0to1(...))`, and `updateLiveFeed` refreshes the shown value to `toDisplay(base, effRatio)` (skipped while dragging). Guard: `effRatio = (moduleEnabled && ratio>0) ? ratio : 1.0` → identity + no write-back when no note sounds or the module is off. Static render confirmed; played-note tracking uses the same mechanism as the (proven) legacy `OscillatorPanel` — the drone sounds C4 (ratio 1.0 = base) so the value is unchanged at rest; a non-C4 note on the on-screen keyboard is the manual spot-check.
- **No new params / no `.synthy`/APVTS change (AC4/NFR3):** pure UI wiring; `modTarget`+transforms are descriptor data. UI reads only `std::atomic` accessors on the message-thread timer; write-back goes through APVTS. Plain-knob path unchanged (still uses `SliderAttachment`).
- **Sample-rack wiring (Task 5)** is throwaway verification wiring (OSC FREQ/AMP + FILTER CUTOFF); Stories 1.5/2.2 fold `modTarget`/transform into the real generator/filter descriptors.
- The per-frame enable-dim `Timer` was left as-is (local enable poll); the LFO/ratio live feed is the separate, single editor-driven push (AD-8).

### File List

- `Source/UI/rack/ModuleFrame.h` (UPDATE) — `updateLiveFeed` API, `moduleEnabled()` helper, ring/xform knob stores + `liveRatio`.
- `Source/UI/rack/ModuleFrame.cpp` (UPDATE) — `toDoubleRange` helper; decoupled transform-knob build path + ring collection in `buildBody`; `updateLiveFeed` implementation.
- `Source/UI/rack/Rack.h` (UPDATE) — `updateLiveFeed` declaration; refreshed header comment (12-col; AD-8 fan-out).
- `Source/UI/rack/Rack.cpp` (UPDATE) — `updateLiveFeed` fan-out to frames.
- `Source/UI/PluginEditor.cpp` (UPDATE) — timer live-feed call to the rack (with +1 ModTarget mapping); `Kmod`/`Kfreq` sample builders; OSC FREQ/AMP + FILTER CUTOFF sample knobs tagged.

## Change Log

- 2026-07-01 — Story 1.4 implemented: live modulation rings + display-value transforms wired generically into the rack (ModuleFrame/Rack + single editor timer). Build clean; rings verified live. Status → review.
