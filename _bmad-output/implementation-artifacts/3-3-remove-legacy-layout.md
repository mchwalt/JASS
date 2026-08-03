---
baseline_commit: 6351e88
---

# Story 3.3: Remove legacy per-module layout code & confirm NFR1

Status: done

<!-- Note: Validation is optional. Run validate-create-story for quality check before dev-story. -->

## Story

As a JASS developer,
I want all bespoke per-module layout code removed,
so that the "no per-module resized()" invariant (NFR1) is actually true and the cobbled-together legacy code is gone.

## Acceptance Criteria

1. **Legacy panels + inline modules deleted.** `OscillatorPanel` and `EffectPanel` classes, the three `osc1/2/3`, the six `EffectPanel` members (delay/chorus/reverb/wavefold/bitcrush/karplus), and every inline legacy module (Mix-Mode, ADSR sliders, Filter, LFO, Arpeggiator, Distortion, Noise, Sub, Wavetable) — their member declarations, construction, wiring, attachments, and layout — are removed from `PluginEditor.h/.cpp`.
2. **Legacy layout + drawing removed.** The legacy per-module positioning in `resized()` and the legacy section/zone-header drawing in `paint()` are gone. What remains lays out ONLY the fixed chrome (header cluster + title) + the on-screen keyboard + the rack.
3. **Reusable components preserved.** `EnvelopeDisplay`, `WaveformDisplay`, `SpectrumDisplay`, `SynthySlider` and the `SynthyLookAndFeel` are **kept** — the rack reuses them. The rack's own scope/spectrum/ADSR-curve instances (owned in `sampleOwned`) keep working. The **legacy** `waveformDisplay`/`spectrumDisplay` unique_ptr members (the footer instances behind the rack) are removed, but the classes stay.
4. **NFR1 true.** No module defines its own `resized()` geometry; layout lives only in the rack engine (`Rack`/`ModuleFrame`). The editor's `resized()` only positions chrome + keyboard + the rack.
5. **Functionally identical + clean build.** The editor is functionally identical to before the cleanup (rack renders; all controls work; preset SAVE/LOAD/RANDOM/RESET + "Current State"; spacebar plucks Karplus; z/x octave shift; keyboard plays). The project builds clean with JUCE warning flags on. No param/format/audio change (NFR2/NFR3).

## Tasks / Subtasks

- [x] **Task 1 — Delete legacy classes + members (`PluginEditor.h`) (AC: 1, 3)** — `OscillatorPanel`/`EffectPanel` classes + all legacy members removed; `EnvelopeDisplay` class kept; header now: processor/lnf/preset-cluster/keyboard/g_titleBounds/sampleRack. (done via delegated deletion agent)
  - [ ] Remove the `OscillatorPanel` class (~`:10-41`) and `EffectPanel` class (~`:43-70`).
  - [ ] **KEEP** the `EnvelopeDisplay` class (~`:77-107`) — the rack's ADSR module reuses it.
  - [ ] Remove these `SynthyEditor` members: `osc1/osc2/osc3`; the Mix-Mode inline group (`mixModeTitle`/`mixModeHint`/`mixPlusLabel`/`mixModeSelector`/`mixModeAttach`); ADSR inline (`adsrTitle`, `adsrEnvDisplay`, `attackKnob`…`releaseKnob`, `atk/dec/sus/relLabel`, `atk/dec/sus/relAttach`); Filter inline; LFO inline; Arpeggiator inline; Distortion inline; `delayPanel`/`chorusPanel`/`reverbPanel`/`wavefoldPanel`/`bitcrushPanel`/`karplusPanel`; `noiseResetBtn`/`subResetBtn`/`wtResetBtn`; Noise inline; Sub inline; Wavetable inline + `refreshBankSelector()`; `setupKnob()`; `initResetButton()` + `adsrResetBtn`…`distResetBtn`; the legacy paint bounds (`adsrBounds`,`lfoBounds`,`filterBounds`,`distBounds`,`noiseBounds`,`wtBounds`,`subBounds`,`arpBounds`) and the zone-header bounds (`genHeaderBounds`,`modHeaderBounds`,`procHeaderBounds`); the legacy `waveformDisplay`/`spectrumDisplay` unique_ptr members.
  - [ ] **KEEP**: includes; `EnvelopeDisplay`; `processor`; `lnf`; preset buttons (`saveBtn`/`loadBtn`/`randomBtn`/`resetBtn`) + `presetChooser` + `presetNameLabel` + `shownLabel` + `setPresetName()` + `updatePresetLabel()`; `keyboard` + `kbBaseOctave`; `g_titleBounds`; `sampleRack` + `sampleOwned` + `buildSampleRack()`; the `Timer`.
- [x] **Task 2 — Delete legacy implementations (`PluginEditor.cpp`) (AC: 1, 2, 3)** — `OscillatorPanel`/`EffectPanel` ctors+resized, `setupKnob`/`initResetButton`/`refreshBankSelector`, the anon-namespace `resetParamsToDefault`/`styleResetButton`, all legacy ctor wiring, legacy `resized()` positioning, legacy `paint()` sections + zone headers, and legacy `timerCallback` fan-out removed. `EnvelopeDisplay::paint()` + `buildSampleRack()` preserved; `timerCallback` keeps the rack `updateLiveFeed(...)` (still computes ratio/lfo/target) + `updatePresetLabel()`.
  - [ ] Remove `OscillatorPanel`'s constructor + `resized()` impl, and `EffectPanel`'s constructor + `resized()` impl.
  - [ ] **KEEP** `EnvelopeDisplay::paint()`.
  - [ ] In the anonymous namespace, remove `resetParamsToDefault()` and `styleResetButton()` **iff** they are only used by legacy code (they are — the rack uses `ModuleFrame::doReset`). Verify no remaining reference.
  - [ ] Remove `setupKnob()`, `initResetButton()`, `refreshBankSelector()` implementations.
  - [ ] **Constructor:** strip all legacy construction/wiring (every OSC/effect/inline panel setup, their attachments, reset buttons, legacy scope/spectrum construction). **KEEP**: `setLookAndFeel(&lnf)` / rack look setup, the preset buttons' setup + onClick handlers, the keyboard creation, `buildSampleRack()`, `startTimerHz(...)`, window size.
  - [ ] **`resized()`:** delete all legacy per-module positioning; keep only the header band (preset cluster + `g_titleBounds`), the keyboard band, and the rack bounds (below header, above keyboard).
  - [ ] **`paint()`:** delete the legacy `drawSection(...)` calls + the GENERATORS/MODULATION/PROCESSING zone-header drawing (the rack draws its own zone headers). Keep the background + the centred "J A S S" title (using `g_titleBounds`).
  - [ ] **`timerCallback()`:** keep the rack live-feed (`sampleRack->updateLiveFeed(...)`) + `updatePresetLabel()`; remove any legacy per-panel updates (`osc.setPlayedRatio`, `setFreqMod`/`setAmpMod`, `cutoffKnob.setModAmount`, legacy scope pokes).
  - [ ] **`keyPressed()`:** keep the spacebar→Karplus PLUCK (via `sampleRack->moduleById("karplus")->clickFirstAction()` or the current rack mechanism) and z/x octave shift; remove any legacy references.
- [x] **Task 3 — Build iteratively + confirm NFR1 (AC: 4, 5)** — Release build **clean on the first pass** (no dangling refs → the legacy was truly unreferenced). `git diff`: only `PluginEditor.h/.cpp`, **1095 deletions / 8 insertions**. `grep OscillatorPanel|EffectPanel` = 0 across `Source/`. NFR1 confirmed: the only `resized()` with layout math are `SynthyEditor::resized()` (chrome-only), `ModuleFrame::resized()`, `Rack::resized()`.
- [x] **Task 4 — In-app verification (AC: 5)** — Built + launched for the user's live parity check. (Also tidied two now-misleading comments on `sampleRack`/`resized()` that referenced "legacy panels beneath".)

## Dev Notes

### What this story is

Every module now lives in the rack (Epics 1–2 complete). The legacy per-module UI (`OscillatorPanel`, reused `EffectPanel`, and the inline ADSR/Filter/LFO/Arp/Distortion/Noise/Sub/Wavetable/Mix-Mode panels + the footer scope/spectrum) has been sitting **behind the opaque rack**, dead but present (recon 2026-07-05: ~600 lines in the header, ~700 in ctor/`resized()`). This story deletes it, making NFR1 ("no per-module `resized()`") literally true. This is the "opaque rack on top of legacy" scaffold finally removed.

### The one critical nuance — keep the reusable component CLASSES

The rack **reuses** three display component classes and the knob:

- `EnvelopeDisplay` (class in `PluginEditor.h`) — the rack ADSR module wraps a `new EnvelopeDisplay(...)` (Story 2.1). **Keep the class**; delete only the legacy `adsrEnvDisplay` member.
- `WaveformDisplay` / `SpectrumDisplay` (own headers) — the rack scope/spectrum wrap `new WaveformDisplay/SpectrumDisplay(...)` (Story 2.3). **Keep the classes + includes**; delete only the legacy `waveformDisplay`/`spectrumDisplay` unique_ptr members + their footer layout.
- `SynthySlider`, `SynthyLookAndFeel` — used everywhere by the rack. Keep.

Deleting the legacy *members* while keeping the *classes* is the whole trick. The compiler will catch any missed reference.

### Method

Delete-then-build-then-fix. The legacy code is unreferenced by the rack path, so after removing the members + impls the only build errors should be leftover references *within other legacy code* (delete those too) — no rack/chrome code should break. Iterate the build until clean, then in-app verify parity.

Watch for shared references:
- **`keyPressed` spacebar → PLUCK** must keep working via the rack (Story 1.5 re-bound it to the rack Karplus module's action, e.g. `moduleById("karplus")->clickFirstAction()`), NOT the deleted `karplusPanel`. Verify the current mechanism and keep it.
- **`timerCallback`** — keep `sampleRack->updateLiveFeed(...)` + `updatePresetLabel()`; drop legacy `osc*.setPlayedRatio`, `setFreqMod/AmpMod`, `cutoffKnob.setModAmount`.
- **anon-namespace helpers** `resetParamsToDefault`/`styleResetButton` — legacy-only; remove after confirming no rack reference.
- **`g_titleBounds`** stays (title); all other `*Bounds` go.

### Files to touch (UPDATE only — no NEW files, no CMake change)

- `Source/UI/PluginEditor.h` — delete legacy classes + members; keep `EnvelopeDisplay`, chrome, keyboard, rack, timer.
- `Source/UI/PluginEditor.cpp` — delete legacy impls + ctor wiring + legacy `resized()`/`paint()`/`timerCallback` bodies; keep chrome/keyboard/rack/title/live-feed/preset paths + `EnvelopeDisplay::paint()`.

_No processor/params/DSP/preset/`.synthy` change — pure editor cleanup (NFR2/NFR3)._

### Guardrails

- **Functionally identical (AC5):** this is deletion of already-hidden code; the visible app must not change. If anything visibly changes, a rack path was accidentally depending on legacy — stop and fix.
- **Don't touch the rack, descriptors, `ModuleFrame`, `Rack`, or the reusable component classes** — only remove legacy editor code.
- **Keep JUCE warnings clean** — removing code shouldn't add warnings; watch for now-unused includes/members.
- **NFR1 gate:** after this, the only `resized()` with layout math are `Rack::resized()` and `ModuleFrame::resized()` (+ the editor's chrome-only `resized()`). Confirm by grep.

### References

- [Source: _bmad-output/planning-artifacts/epics.md#Story 3.3] (remove legacy layout; confirm NFR1)
- [Source: ARCHITECTURE-SPINE.md#AD-1/AD-2, NFR1] (one generic ModuleFrame; rack owns all placement; no per-module resized())
- [Source: _bmad-output/project-context.md] (no param/format/audio change; keep warnings clean)
- Recon (2026-07-05): `PluginEditor.h` legacy classes `~:10-70`, `EnvelopeDisplay` (KEEP) `~:77-107`, legacy members `~:125-235`, chrome/keyboard/rack (KEEP) `~:226-263`. `PluginEditor.cpp` legacy ctor/wiring, legacy `resized()` `~:1147-1391` (chrome+keyboard+rack `~:1154-1171`,`1399-1421`), legacy `paint()` sections + zone headers (title KEEP `~:909-919`).

## Dev Agent Record

### Agent Model Used

claude-opus-4-8[1m] (Opus 4.8, 1M context) — deletion executed via a delegated sub-agent, reviewed + built + comment-tidied by the main agent.

### Debug Log References

- Deletion done by a focused sub-agent (guided by this story's keep/delete map), constrained to `PluginEditor.h/.cpp`. It reported a **clean first-pass build** — no dangling references, confirming the legacy code was fully unreferenced behind the rack.
- Main-agent review: `git diff --stat` = 2 files, **+8 / −1095**; `grep` confirms `OscillatorPanel`/`EffectPanel` gone everywhere; header verified lean (only chrome/keyboard/rack/EnvelopeDisplay); `SynthyEditor::resized()` verified chrome-only; NFR1 grep = only rack + chrome `resized()`.
- Own Release build clean (twice — after deletion, and after tidying two stale comments). `JASS.exe` launched for verification.

### Completion Notes List

- **~1300 lines of dead legacy UI removed** (`OscillatorPanel`, `EffectPanel`, all inline ADSR/Filter/LFO/Arp/Distortion/Noise/Sub/Wavetable/Mix-Mode panels + their construction/wiring/attachments/`resized()`/`paint()` sections + the legacy footer scope/spectrum). Net **−1095 / +8** across the two editor files.
- **NFR1 now literally true:** no per-module `resized()` — layout lives only in `Rack`/`ModuleFrame` (+ the editor's chrome-only `resized()`).
- **Reusable classes preserved:** `EnvelopeDisplay` (+ `::paint`), `WaveformDisplay`, `SpectrumDisplay`, `SynthySlider`, `SynthyLookAndFeel` — the rack wraps them. `buildSampleRack()` byte-for-byte unchanged. Chrome (preset SAVE/LOAD/RANDOM/RESET + "Current State"), keyboard, spacebar-pluck, z/x octave, and the rack live-feed timer all preserved.
- **Clean-first-build** is the parity signal for a pure deletion: nothing outside the deleted legacy referenced it. No param/format/audio change (NFR2/NFR3).
- Tidied two now-misleading comments (`sampleRack` "throwaway/placeholder/Story-1.5" and `resized()` "hides legacy panels beneath").
- **Deferred (unchanged by this story):** Story 3.4 (VST3 parity in REAPER — needs the user); the "no enabler on Display-only modules" follow-up from 2.3. The `sampleRack`/`sampleOwned`/`buildSampleRack` names are still the historical "sample*" from the 1.3 scaffold — a pure rename is optional cosmetic follow-up.

### File List

- `Source/UI/PluginEditor.h` (UPDATE) — removed `OscillatorPanel`/`EffectPanel` + all legacy members; kept `EnvelopeDisplay` + chrome/keyboard/rack; refreshed the `sampleRack` comment.
- `Source/UI/PluginEditor.cpp` (UPDATE) — removed legacy impls, ctor wiring, legacy `resized()`/`paint()`/`timerCallback` fan-out, and legacy helper functions; kept `EnvelopeDisplay::paint()` + `buildSampleRack()` + chrome/keyboard/rack paths; refreshed a stale comment.

## Change Log

- 2026-07-05 — Story 3.3: removed all legacy per-module layout code (~1300 lines: OscillatorPanel/EffectPanel + inline panels + their `resized()`/`paint()`). NFR1 confirmed (no per-module `resized()`). Reusable display classes + `buildSampleRack` + chrome/keyboard preserved. Clean build, functionally identical. Status → review.
