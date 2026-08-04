---
baseline_commit: f7534a3
---

# Story 2.3: Add the graphical display modules (Oscilloscope + Spectrum)

Status: done

<!-- Note: Validation is optional. Run validate-create-story for quality check before dev-story. -->

## Story

As a JASS player,
I want the oscilloscope and spectrum as real rack display modules (with a scope time-base/zoom and proper axes),
so that the visualizations are first-class, consistent parts of the rack — completing Epic 2.

## Acceptance Criteria

1. **Real displays in the rack.** OSCILLOSCOPE and SPECTRUM render **live** as size-**XL** modules placed side by side (the display band), each reusing its existing component (`WaveformDisplay` / `SpectrumDisplay`) as a `Display` body element — **not** the throwaway `SampleDisplayPlaceholder`. Owned via `sampleOwned` (same lifetime pattern as the ADSR `EnvelopeDisplay`); separate instances from the legacy ones (a `juce::Component` has one parent; legacy stays behind the opaque rack until Story 3.3). They repaint only on their existing 30 Hz poll (NFR5).
2. **Scope time-base/zoom + ms scale.** The Oscilloscope has a selectable time-base (**1 / 2 / 5 / 10 / 25 ms**) as a combo and a left-side/bottom **millisecond scale** (axis ticks + labels). _These already exist in `WaveformDisplay` (`zoomBox` + ms ticks + amplitude scale)._ The displayed sample window must equal **`ms × sampleRate/1000`** using the **actual** sample rate (see AC4), not a hardcoded 44.1 kHz.
3. **Spectrum scales.** The Spectrum shows a **frequency axis** (log) and a **level/dB axis**. _These already exist in `SpectrumDisplay` (log-freq ticks + dB ticks)._ The bin→frequency mapping must use the **actual** sample rate (see AC4).
4. **Correct sample rate (the real fix).** Both displays use the engine's **actual** sample rate, not a hardcoded/uninitialized 44.1 kHz. `WaveformDisplay` currently hardcodes `44100.0` in its ms→samples formula; `SpectrumDisplay` has an orphaned `setSampleRate()` that is never called (bin width defaults to 44.1 kHz). Fix so both track the real rate (and rate changes) — the scope's ms window and the spectrum's Hz labels are correct at 48 k / 96 k etc.
5. **No duplicate title.** The rack module header already shows "OSCILLOSCOPE"/"SPECTRUM"; the display component must **not** also draw its own internal title inside the body (avoid the doubled label / wasted vertical space). Legacy standalone use is unaffected.
6. **No engine/param/format change (NFR2/NFR3).** No new APVTS params, no `.synthy` change, no per-sample audio-thread work added. Adding a display-support sample-rate value set in `prepareToPlay` is not an audio-behaviour change. Build clean with JUCE warning flags on.

## Tasks / Subtasks

- [x] **Task 1 — Sample rate to the displays via `WaveformCapture` (AC: 4)** — `WaveformCapture` got `std::atomic<double> sampleRate` + `setSampleRate`/`getSampleRate`; `prepareToPlay` calls `waveformCapture.setSampleRate(sampleRate)`; `WaveformDisplay` uses `captureRef.getSampleRate()` instead of hardcoded 44100; `SpectrumDisplay` refreshes `sampleRate` from `captureRef.getSampleRate()` each frame in `timerCallback`.
- [x] **Task 2 — Suppress the internal title in rack use (AC: 5)** — `showTitle` + `setShowTitle` added to both; internal `drawText(title)` guarded by `if (showTitle)`; default true (legacy unchanged).
- [x] **Task 3 — Wire real displays into the rack (AC: 1, 2, 3, 5)** — placeholders replaced with real `WaveformDisplay`/`SpectrumDisplay` (typed ptr `new … → setShowTitle(false) → sampleOwned.add(…)` → `Display{ptr,12}`) at XL, no enableParam. The now-unused `display()` lambda + `SampleDisplayPlaceholder` (Story-1.3 prototype scaffolding, not legacy audio code) were removed to keep warnings clean.
- [x] **Task 4 — Build + in-app verification (AC: all)** — Release build clean (after fixing: `sampleOwned.add()` returns `Component*`, so keep the typed `WaveformDisplay*`/`SpectrumDisplay*` before adding). `JASS.exe` launched for live verification.

## Dev Notes

### What this story actually is

Per the recon (2026-07-05), `WaveformDisplay` and `SpectrumDisplay` are **~95% complete**: the scope already has the zoom combo (`{1,2,5,10,25} ms`), the amplitude (Y) scale, and the ms (X) ticks; the spectrum already draws a log-frequency axis and a dB axis. The 2026-07-02 requirement (scope zoom + scales, spectrum scales) is therefore **already met by the components** — this story does **not** build those from scratch. The real work is:

1. **Migrate placeholders → real components** in the rack (like the ADSR `EnvelopeDisplay` migration in Story 2.1).
2. **Fix the sample-rate wiring** — the one genuine defect: `WaveformDisplay` hardcodes `44100.0` (wrong ms window off-44.1k) and `SpectrumDisplay`'s `setSampleRate()` is never called (wrong Hz labels off-44.1k). Route the real rate through `WaveformCapture` (single shared source, tracks rate changes, no editor plumbing).
3. **Suppress the components' internal titles** so they don't double the rack header title.

This completes **Epic 2** (modulation + processing + displays all in the uniform rack).

### Key facts (from recon)

- **WaveformDisplay** (`Source/UI/WaveformDisplay.h`, header-only): ctor `WaveformDisplay(WaveformCapture&, juce::Colour = 0xff40c0ff)`; `zoomBox` items `{"1 ms","2 ms","5 ms","10 ms","25 ms"}` default 10 ms → `updateTimeRange()` sets `timeRangeMs`; 30 Hz timer calls `captureRef.updateSnapshot()` + `repaint()`; `paint()` draws Y amplitude scale (±1) + X ms ticks + waveform; **hardcoded 44100 at the ms→samples line**; internal title "OSCILLOSCOPE".
- **SpectrumDisplay** (`Source/UI/SpectrumDisplay.h`, header-only): ctor `SpectrumDisplay(WaveformCapture&, juce::Colour = 0xffa78bfa)`; `fftOrder = 10` (1024); 30 Hz timer → `updateSnapshot()` + `computeFFT()` + `repaint()`; `paint()` draws dB axis (`0,-12,-24,-36,-48`, `mindB=-48`) + log-freq axis (`freqToX` log2, `minFreq=30`, `maxFreq=16000`); bin width `sampleRate/fftSize`; `sampleRate` member defaults 44.1k, `setSampleRate()` exists but **orphaned**; internal title "SPECTRUM".
- **WaveformCapture** (`Source/DSP/WaveformCapture.h`, header-only): triple-buffered ring + zero-crossing trigger; `writeSample()` on audio thread (RT-safe); `updateSnapshot()` on GUI thread is **safe to call from multiple display instances** (idempotent per frame; recompute cost negligible). Single instance in the processor (`PluginProcessor.h`), `getWaveformCapture()` accessor; written in `processBlock`.
- **Rack placeholders:** `buildSampleRack` (`PluginEditor.cpp` ~`:1149-1152`) adds both at XL via the `display(label, slots)` lambda (→ `SampleDisplayPlaceholder` in `sampleOwned`). Replace with real components (same `sampleOwned` ownership) exactly as the ADSR `Display{ sampleOwned.add(new EnvelopeDisplay(...)), 4 }` migration did.
- **Legacy displays** (`PluginEditor.h` `waveformDisplay`/`spectrumDisplay` unique_ptrs, built in the editor ctor with `p.getWaveformCapture()`, laid out in a legacy footer band) stay in place behind the opaque rack (deleted in Story 3.3). The rack gets **new** instances.

### Sharing the capture across instances

Confirmed safe: `updateSnapshot()` reads `writePos` once and rebuilds a per-frame snapshot; multiple displays polling it just recompute the (cheap) trigger + copy independently. So the rack's new scope/spectrum + the still-present legacy ones all reading the one capture is fine (NFR5 — each repaints only on its 30 Hz change).

### Guardrails (project-context + ADs)

- **Display = BodyElement (AD-5):** scope/spectrum are `Display{component, slots}` at XL, placed/dimmed by the frame like any body element. No dedicated VISUALIZATION zone (decided 2026-07-01) — they read as a display band by form + placement in PROCESSING.
- **No params/format/audio-behaviour change (NFR2/NFR3):** the only processor touch is `waveformCapture.setSampleRate(sampleRate)` in `prepareToPlay` (an atomic store; no per-sample work, no param, no `.synthy`).
- **Ownership (AD-5):** `Display.component` is non-owning; own the instances in `sampleOwned` (parity with the placeholders + the ADSR display). No lifetime change vs the shipped pattern.
- **Don't delete legacy display code** — Story 3.3. Leave the legacy `waveformDisplay`/`spectrumDisplay` + their footer layout behind the opaque rack.
- **Displays have no enabler** — the universal-enabler decision (Story 2.4) is about control modules; passive visualizers stay always-on (empty `enableParam`, no `enabledWhen`). The `ModuleFrame` still shows a static-on toggle for them (from `e759783`) — confirm in review whether that reads oddly on a scope; if so, a follow-up can special-case "no enabler for Display-only modules". Not addressed here.

### Project Structure Notes

- Reused components stay in `Source/UI/` (`WaveformDisplay.h`, `SpectrumDisplay.h`); `WaveformCapture.h` in `Source/DSP/`. Auto-derived module `id`s: `oscilloscope` / `spectrum`.

### Previous-story intelligence

- **ADSR migration (2.1)** is the exact template: `Display{ sampleOwned.add(new X(...)), slots }`, separate instance from legacy, non-owning pointer.
- **2.1/2.4 Sonnet reviews** validated the `sampleOwned` lifetime (declared after `sampleRack`, destroyed first-safe) — same here.
- **Scope for zoom/scales was already built** (likely during the original C#-feature-restore work); this story wires + corrects rather than re-implements. Don't duplicate the axis drawing.

### Verification

Build + launch; user confirms live ([[feedback-ui-verification]]). Manual checks in Task 4. No unit-test framework.

### References

- [Source: _bmad-output/planning-artifacts/epics.md#Story 2.3] (Scope+Spectrum as XL displays; 2026-07-02 zoom+scales requirement; no VISUALIZATION zone)
- [Source: ARCHITECTURE-SPINE.md#AD-5] (graphic displays are BodyElements; uniform dim)
- [Source: _bmad-output/project-context.md] (RT rules; no param/format change; prepareToPlay is the place to propagate sample rate)
- Code: `Source/UI/WaveformDisplay.h` (zoomBox, ms ticks, hardcoded 44100), `Source/UI/SpectrumDisplay.h` (dB + log-freq axes, orphaned setSampleRate), `Source/DSP/WaveformCapture.h` (ring + updateSnapshot, add sampleRate), `Source/PluginProcessor.cpp` prepareToPlay (`~:198`) + `getWaveformCapture()`, `Source/UI/PluginEditor.cpp` buildSampleRack (`~:1149-1152`), ADSR-migration reference (the `EnvelopeDisplay` Display line).

## Dev Agent Record

### Agent Model Used

claude-opus-4-8[1m] (Opus 4.8, 1M context)

### Debug Log References

- Build error C2039 `setShowTitle not a member of juce::Component`: `sampleOwned.add(new …)` returns `juce::Component*` (the OwnedArray element type). Fixed by keeping the typed `WaveformDisplay*`/`SpectrumDisplay*` (call `setShowTitle` before `sampleOwned.add`). Rebuild clean.
- Final Release build (MSBuild, VS2022) clean; `PluginProcessor.cpp`/`PluginEditor.cpp` recompiled → `JASS.exe`. Launched for live verification per [[feedback-ui-verification]].

### Completion Notes List

- **Verification-heavy story:** the scope zoom combo (1/2/5/10/25 ms), amplitude+ms scales, and the spectrum's log-freq+dB scales were **already implemented** in the components — the 2026-07-02 requirement was met; this story wired them into the rack and fixed the sample rate.
- **Sample-rate fix (the real defect):** `WaveformDisplay` hardcoded 44100 (wrong ms window off-44.1k); `SpectrumDisplay::setSampleRate` was orphaned (wrong Hz labels off-44.1k). Both now pull the real rate from `WaveformCapture` (a `std::atomic<double>` set in `prepareToPlay`), which auto-tracks rate changes with zero editor-side plumbing (both displays already hold the capture ref). RT-safe (atomic store in prepare; reads on the GUI thread).
- **Real displays in the rack** via `sampleOwned` (own instances, separate from the still-present legacy ones behind the opaque rack — one-parent rule; legacy deleted in 3.3). Sharing one `WaveformCapture` across instances is safe (`updateSnapshot` idempotent per frame).
- **No doubled title:** `setShowTitle(false)` on the rack instances (module header carries the title); legacy standalone default stays true.
- **Cleanup:** removed the now-unused `display()` lambda + `SampleDisplayPlaceholder` (Story-1.3 prototype scaffolding) so no unused-local warning; no audio/param/format change (NFR2/NFR3).
- **Completes Epic 2** (MODULATION + PROCESSING + display modules all in the uniform rack).
- **Review notes:** (a) displays currently still show the universal static-on enabler toggle (from `e759783`) — confirm whether that reads oddly on a passive scope; a follow-up could special-case "no enabler for Display-only modules". (b) Confirm the scope/spectrum look right at a non-44.1 k device rate.

### File List

- `Source/DSP/WaveformCapture.h` (UPDATE) — `sampleRate` atomic + get/set.
- `Source/PluginProcessor.cpp` (UPDATE) — `waveformCapture.setSampleRate(sampleRate)` in `prepareToPlay`.
- `Source/UI/WaveformDisplay.h` (UPDATE) — use `captureRef.getSampleRate()`; `showTitle`/`setShowTitle`.
- `Source/UI/SpectrumDisplay.h` (UPDATE) — refresh `sampleRate` from capture; `showTitle`/`setShowTitle`.
- `Source/UI/PluginEditor.cpp` (UPDATE) — real Scope/Spectrum in the rack; removed unused placeholder lambda + struct.

## Change Log

- 2026-07-05 — Story 2.3: Oscilloscope + Spectrum are real XL rack Display modules (zoom + scales already existed). Fixed sample-rate wiring (WaveformCapture atomic set in prepareToPlay → both displays), suppressed the doubled internal title, removed the placeholder scaffolding. Build clean. Status → review. **Epic 2 complete.**
