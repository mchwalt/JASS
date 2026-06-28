---
baseline_commit: 246377d74471bfb5f48f9a77b17c1c2f21d766d9
---

# Story 1.3: Rack grid layout engine & zone headers

Status: done (engine built + visually verified in the running app)

<!-- Note: Validation is optional. Run validate-create-story for quality check before dev-story. -->

## Story

As a JASS developer,
I want a `Rack` component that places `ModuleFrame`s on a fixed columns×units grid with zone headers and the shared look,
so that modules align consistently and ALL layout geometry lives in exactly one place (NFR1) — the third and final pillar of the rack framework.

## Acceptance Criteria

1. **Given** a set of S/M/L modules handed to the `Rack`, **When** the editor lays out, **Then** each module occupies whole grid multiples (**S = 1×1, M = 2×1, L = 2×2** cols×units) on a fixed **8-column** grid with **uniform gutters**, and **no module overlaps a grid boundary or another module** (placement is the single layout site — `Rack::resized()`; no per-module `resized()` is added).
2. **Given** the three module groups, **When** the rack lays out, **Then** **GENERATORS / MODULATION / PROCESSING** zone headers span the full rack width, start their group on a fresh grid row, and visually separate the groups (text + rule, in the group's type-tag hue).
3. **Given** the rack, **When** it is shown, **Then** a **single `SynthyLookAndFeel`** is set **once by the rack** and applies to every module frame and control beneath it; no `ModuleFrame` or control installs its own LookAndFeel (AD-7).
4. **Given** the full first-pass module census (≈10×S, 6×M, 4×L + 3 zone headers — the mockup population), **When** the rack lays out at the fixed target window (**~1920×1200**), **Then** the entire rack fits **without scrolling** and the chosen grid constants (`Wc`, `Hu`, gutter, zone-header height) are frozen as named code constants in `Rack`.
5. **Given** the build, **When** compiled, **Then** `Source/UI/rack/Rack.{h,cpp}` (and the extracted `Source/UI/rack/SynthyLookAndFeel.{h,cpp}`) are in `CMakeLists.txt` `target_sources`, the project builds clean (Release, JUCE warning flags) with no new warnings, and a representative sample population renders correctly in the running app (zones + placement + a dimmed module + uniform look), confirming the fit, before completion.
6. **Given** the real-time / state constraints, **Then** the rack is message-thread/UI-only: it touches the audio engine only through the frames' APVTS attachments (it adds none of its own), allocates only on the message thread, and changes no parameter ID / APVTS layout / `.synthy` format (NFR2/NFR3).
7. **Given** the deferred ModuleFrame body-grid items from Story 1.2's review (due now per `deferred-work.md`), **Then** the body slot-grid in `ModuleFrame::resized()` no longer lets a spanning cell overflow below the body, and row count is derived from placed cells (not `bodySlots(desc.body)`), so a skipped null-`Display` does not inflate the grid.

## Tasks / Subtasks

- [x] **Task 1: Extract `SynthyLookAndFeel` into the rack framework** (AC: 3, 5)
  - [x] Moved `class SynthyLookAndFeel` (+ `drawRotarySlider`) out of `PluginEditor.{h,cpp}` into new `Source/UI/rack/SynthyLookAndFeel.{h,cpp}` (top-level `Synthy*` name kept). Implementation byte-identical (pure move).
  - [x] `#include "rack/SynthyLookAndFeel.h"` from `PluginEditor.h`; editor's `SynthyLookAndFeel lnf;` still compiles (legacy panels keep using it until Story 3.3).
  - [x] Added `Source/UI/rack/SynthyLookAndFeel.cpp` to `target_sources`.
- [x] **Task 2: `Rack` skeleton + CMake** (AC: 1, 5, 6)
  - [x] Created `Source/UI/rack/Rack.{h,cpp}`; `class Rack : public juce::Component` in `namespace rack`.
  - [x] Ctor `Rack(juce::AudioProcessorValueTreeState&)` stores apvts; owns `SynthyLookAndFeel lnf;` and `setLookAndFeel(&lnf)` on itself; dtor clears it.
  - [x] `enum class Zone { Generators, Modulation, Processing };`
  - [x] `void addModule(Zone, ModuleDescriptor)` — builds+owns a `ModuleFrame` (`OwnedArray<ModuleFrame>`), records `Zone`+footprint, `addAndMakeVisible`.
  - [x] Added `Source/UI/rack/Rack.cpp` to `target_sources`.
- [x] **Task 3: Grid constants + the placement engine** (AC: 1, 4)
  - [x] Froze `kCols=8`, `kGutter=10`, `kHu=84`, `kZoneHeaderH=28`, `kPad=8` on `Rack`; `Wc` derived from width.
  - [x] `layout(width, apply)` places each zone's frames into the 8-wide occupancy grid (footprint from `sizeClassSpec`), L blocks 2 rows, no overlap; the single placement site.
  - [x] **Placement strategy:** row-major **first-fit** isolated in `layout()` (the one packing path) — swappable to dense-fill later without touching `resized()`/`preferredHeight()`. (First-fit already backfills gaps, so the dense-fill upgrade may be unnecessary.)
  - [x] Grid-cell → pixel-rect conversion in `layout()` sets each frame's bounds.
  - [x] `int preferredHeight(int width) const` returns the measured stacked height (`layout(..., apply=false)`).
- [x] **Task 4: Zone headers** (AC: 2)
  - [x] Zone headers (text + rule) drawn full-width in `Rack::paint()`; bands computed in `layout()`; each zone starts a fresh row.
  - [x] Promoted `typeColour(ModuleType)` to `ModuleDescriptor.h` (single source); `ModuleFrame` and `Rack` both use it (no duplicate).
- [~] **Task 5: Wire the Rack into the editor with a representative sample population & verify fit** (AC: 4, 5)
  - [x] `SynthyEditor::buildSampleRack()` builds the full mockup census (3×M OSC, S Sub/Noise/MixMode, M Karplus, L Wavetable; L ADSR(+Display)/M LFO/M Arp; 7×S processors + L Scope/Spectrum Displays), all bound to real `Parameters::ID`. Rack added LAST (opaque, covers legacy body).
  - [x] Window set to fixed **1920×1200** (`kDesignW/H`); rack placed in the body band in `resized()` (below header, above keyboard); legacy panels untouched (not deleted).
  - [ ] **Visual eyeball pending** (needs a run): 8-col grid, whole-multiple footprints, uniform gutters, three zone headers, dimmed disabled body, uniform knob look, fits 1920×1200 without scrolling. (Build confirms the fit math: population ≈ 882px ≤ ~1008px body band.)
  - [x] Rack kept wired; sample population marked `TEMP (Story 1.3) … replaced by real descriptors in Story 1.5`.
- [x] **Task 6: Resolve the Story 1.2 deferred body-grid items** (AC: 7)
  - [x] `ModuleFrame::resized()` row count now from placed `cells` (not `bodySlots`).
  - [x] Placement clamped to `nRows-1` so a spanning cell can't fall below the body.
  - [x] Marked the body-grid item resolved in `deferred-work.md`.
- [~] **Task 7: Verify** (AC: 5, 6)
  - [x] Clean Release build (JUCE warning flags), **0 warnings / 0 errors**; `PluginEditor.cpp` compiles after the LookAndFeel move.
  - [ ] **Visual confirmation pending** a run (AC4/AC5 render gate).

## Dev Notes

### What this story IS (and is NOT)
- **IS:** the `Rack` grid engine — owns `ModuleFrame`s, places them on the fixed 8-col × unit grid (the single layout site), draws the three zone headers, sets the one shared `SynthyLookAndFeel`, and proves the whole population fits 1920×1200. Plus the physical extraction of `SynthyLookAndFeel` into `rack/`, and closing the two deferred ModuleFrame body-grid items.
- **IS NOT:** modulation-ring/display-transform wiring (Story 1.4 — the rack's `modTarget` lookup is added THEN, not here), real module descriptors (Story 1.5+), the global header chrome / Master+Stereo / keyboard restyle (Epic 3), or deleting the legacy `OscillatorPanel`/`EffectPanel`/inline code (Story 3.3). Use a throwaway sample population to test.
- **NFR1 nuance:** `Rack::resized()` owns the *frame outer rectangles* and zone-header bands; `ModuleFrame::resized()` (Story 1.2) owns each frame's *internal* header+body. Together these two framework sites are the ONLY layout geometry. No per-*module* class has a `resized()`.

### Must reuse — do NOT reinvent (with sources)
- **`ModuleFrame`** — the rack builds one per descriptor via `rack::ModuleFrame(apvts, std::move(desc))`; the frame owns its attachments, header, body, dim, reset. The rack only sets its bounds. [Source: Source/UI/rack/ModuleFrame.h/.cpp]
- **The descriptor + size table** — `rack::ModuleDescriptor`, `SizeClass{S,M,L}`, `sizeClassSpec()` giving `{cols, units, slotCapacity, knobDiameter}` (S=1×1/3, M=2×1/6, L=2×2/12). Drive the footprint from `sizeClassSpec(sc).cols/units` — never hardcode per-module sizes. [Source: Source/UI/rack/ModuleDescriptor.h:97-115]
- **`SynthyLookAndFeel`** — currently defined in `PluginEditor.h:8-15` + `PluginEditor.cpp`; MOVE it verbatim into `rack/SynthyLookAndFeel.{h,cpp}` (Task 1). The editor already sets it globally today (`setLookAndFeel(&lnf)` in its ctor; cleared in dtor) — mirror that ownership pattern in the rack. [Source: Source/UI/PluginEditor.h:8-15, :121]
- **Type-tag hues** — already in `ModuleFrame.cpp`'s anonymous-namespace `typeColour(ModuleType)`; promote to a shared `rack::typeColour` so the rack zone headers and the frame use ONE definition (no copy). [Source: Source/UI/rack/ModuleFrame.cpp:7-15]
- **Display components** — `WaveformDisplay`, `SpectrumDisplay`, `EnvelopeDisplay` are reused as `Display{component}` body elements (their lifetime is owned by the editor per AD-5; the sample L-module in Task 5 can wrap one of them or a throwaway `juce::Component`). [Source: Source/UI/WaveformDisplay.h, SpectrumDisplay.h, PluginEditor.h:84-114]

### Grid constants (seed — tune so AC4 holds, then freeze as `Rack` constants)
The mockup uses CSS `1fr` columns, so exact pixels are intentionally NOT pinned there ("tune exact Wc/Hu in code" — mockup footer). Seed from the mockup's proportions and the existing frame header height:
- `kCols = 8` (fixed — the whole layout rests on 8 columns; mockup `grid-template-columns:repeat(8,1fr)`).
- `kGutter ≈ 10` (mockup gap 9px).
- `kHu ≈ 84` (mockup `grid-auto-rows:84px`; L spans `2*kHu + kGutter`). Compact unit = header (22px, `ModuleFrame::kHeaderH`) + one knob row.
- `kZoneHeaderH ≈ 28`.
- `Wc` derived: `(gridWidth - (kCols-1)*kGutter) / kCols`. At ~1872px grid width → `Wc ≈ 225`.
- **Fit math to satisfy AC4:** census ≈ S×10 + M×6 + L×4 dense-packed into 8 cols ≈ ~10 unit-rows + 3 zone headers. `10*kHu + 9*kGutter + 3*kZoneHeaderH ≈ 840 + 90 + 84 = 1014`px of grid, leaving headroom under 1200 for top chrome + keyboard. If it overflows, reduce `kHu` first. Knob diameter is already handled inside the frame via `sizeClassSpec().knobDiameter` — do not set it in the rack.

### Hard constraints (project-context.md — binding)
- **UI/message-thread only.** The rack allocates frames/labels on the message thread; it adds NO APVTS attachments of its own (the frames own all bindings — AD-6). [project-context.md RT rules; NFR2]
- **No parameter-ID / APVTS-layout / `.synthy` changes.** [NFR3]
- **Naming dualism stays** — new code under `Source/UI/rack/` in `namespace rack`; the moved `SynthyLookAndFeel` keeps its `Synthy*` top-level name. [project-context.md]
- **After editing `CMakeLists.txt`** (3 new `.cpp` entries) CMake re-runs on build via ZERO_CHECK; a clean Release build picks them up. Build via MSBuild from PowerShell (`cmake.exe` not on PATH; full path in project memory). MSBuild: `C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe "build\JASS_Standalone.vcxproj" /p:Configuration=Release /p:Platform=x64`. [project-context.md Build]
- **Keep JUCE warning flags clean.** [project-context.md]

### Previous-story intelligence (Stories 1.1 + 1.2, incl. 1.2 review)
- **`ModuleFrame` API to consume:** `ModuleFrame(juce::AudioProcessorValueTreeState&, ModuleDescriptor)` (descriptor moved in); it is `JUCE_DECLARE_NON_COPYABLE` — store frames in an `OwnedArray<ModuleFrame>`, never by value/copy. [Source: Source/UI/rack/ModuleFrame.h]
- **Reset is now body-derived + always shown** (1.2 review decision): every module header has a ↺ that resets all body params except the enable flag. Header geometry is already uniform (enable+reset slots reserved unconditionally). The rack does nothing extra here — just inherit it. [Source: Source/UI/rack/ModuleFrame.cpp doReset()/resized()]
- **Attachments crash on a bad paramId** (1.2 deferred, still open): JUCE's `*Attachment` deref `getParameter(id)` with no guard. The Task-5 sample descriptors MUST use real `Parameters::ID` values, or the editor will assert/crash on open. [Source: deferred-work.md; Source/Audio/Parameters.h]
- **Two body-grid items are due NOW** (Task 6): spanning-cell overflow below body, and `nRows` derived from `bodySlots` (counts skipped null-Displays). The suspected "double column-advance" was already verified NOT a bug — do not "fix" it. [Source: deferred-work.md "code review of story 1-2-module-frame"]
- **Verification reality:** no unit-test framework. Verification = clean Release build + temporary in-app smoke instance (here: the sample population), consistent with how 1.1/1.2 were verified.

### Project Structure Notes
- New files: `Source/UI/rack/Rack.{h,cpp}` and `Source/UI/rack/SynthyLookAndFeel.{h,cpp}` (the latter moved from `PluginEditor`). All three new `.cpp`s go into `target_sources`. No other structural change.
- `PluginEditor.{h,cpp}` are MODIFIED: remove the inline `SynthyLookAndFeel` definition, include the new header, construct + show the `Rack` with the sample population. Do not touch the legacy panel members beyond what the LookAndFeel move requires (their deletion is Story 3.3).

### References
- [Source: _bmad-output/planning-artifacts/epics.md#Story-1.3]
- [Source: ARCHITECTURE-SPINE.md AD-2 (rack owns placement / grid / size-class table), AD-7 (one shared LookAndFeel), AD-1, AD-5; Structural Seed; Deferred (exact grid constants)]
- [Source: rack-mockup.html (8-col grid, S/M/L footprints, zone headers, dimmed body, type-tag hues; footer "tune exact Wc/Hu in code")]
- [Source: Source/UI/rack/ModuleDescriptor.h] [Source: Source/UI/rack/ModuleFrame.h/.cpp]
- [Source: Source/UI/PluginEditor.h/.cpp (SynthyLookAndFeel, legacy zone headers genHeaderBounds/modHeaderBounds/procHeaderBounds, window sizing)]
- [Source: _bmad-output/implementation-artifacts/deferred-work.md] [Source: _bmad-output/project-context.md]

## Dev Agent Record

### Agent Model Used

claude-opus-4-8[1m] (Claude Opus 4.8, 1M context)

### Debug Log References

- Release build (MSBuild, `build/JASS_Standalone.vcxproj`, x64): CMake auto-reconfigured on the `CMakeLists.txt` change, compiled `SynthyLookAndFeel.cpp`, `Rack.cpp`, `ModuleFrame.cpp`, recompiled `PluginEditor.cpp`/`PluginProcessor.cpp`, linked `JASS.exe` — **0 warnings, 0 errors**.

### Completion Notes List

- **Rack engine (`Source/UI/rack/Rack.{h,cpp}`):** owns a `ModuleFrame` per descriptor, places them on a fixed 8-col × `kHu` grid via `layout(width, apply)` — the single frame-placement site (NFR1). Row-major first-fit packing (isolated in `layout()`, swappable to dense-fill later); L blocks 2 cols × 2 rows, no overlap. Zone-header bands computed in `layout()`, drawn in `paint()`. `preferredHeight()` measures via `layout(apply=false)`.
- **Shared look (AD-7):** `SynthyLookAndFeel` extracted verbatim to `Source/UI/rack/SynthyLookAndFeel.{h,cpp}`; the `Rack` owns one instance and `setLookAndFeel`s itself so all frames/controls inherit it. Editor's legacy `lnf` stays for the legacy panels (removed in Story 3.3).
- **Single palette:** `typeColour(ModuleType)` promoted to `ModuleDescriptor.h`; `ModuleFrame` + `Rack` share it (removed the frame's duplicate).
- **Grid constants frozen (1920×1200 target):** `kCols=8, kGutter=10, kHu=84, kZoneHeaderH=28, kPad=8` → `Wc ≈ 226` at the body-band width. Window set to 1920×1200; existing auto-fit-down preserved.
- **1.2 deferred body-grid fixes (AC7):** `ModuleFrame::resized()` row count now from placed cells (not `bodySlots`), and placement clamped to `nRows-1` so spanning cells can't overflow below the body.
- **Sample population (TEMP):** `SynthyEditor::buildSampleRack()` builds the full mockup census bound to real `Parameters::ID`; the opaque rack covers the legacy body while chrome + keyboard stay live. Replaced by real descriptors in Story 1.5.
- **Verification honesty:** verified by clean Release build + the fit math (population ≈ 882px ≤ ~1008px body band). The in-app visual render (AC4/AC5: zones, placement, dimmed body, uniform look, no-scroll fit) is **not yet eyeballed** — pending a run. No project test framework exists.

### File List

- **NEW:** `Source/UI/rack/Rack.h`
- **NEW:** `Source/UI/rack/Rack.cpp`
- **NEW:** `Source/UI/rack/SynthyLookAndFeel.h`
- **NEW:** `Source/UI/rack/SynthyLookAndFeel.cpp`
- **MODIFIED:** `Source/UI/rack/ModuleDescriptor.h` (shared `typeColour`)
- **MODIFIED:** `Source/UI/rack/ModuleFrame.cpp` (use shared `typeColour`; body-grid fixes)
- **MODIFIED:** `Source/UI/PluginEditor.h` (remove inline LookAndFeel, include rack headers, sample-rack members)
- **MODIFIED:** `Source/UI/PluginEditor.cpp` (remove LookAndFeel def; `buildSampleRack()`; place rack; window 1920×1200)
- **MODIFIED:** `CMakeLists.txt` (added `Rack.cpp`, `SynthyLookAndFeel.cpp`)
- **MODIFIED:** `_bmad-output/implementation-artifacts/deferred-work.md` (body-grid item resolved)

### Change Log

- 2026-06-28 — Story 1.3 drafted (create-story): Rack grid engine + zone headers + shared LookAndFeel + 1920×1200 fit, plus SynthyLookAndFeel extraction and the two deferred ModuleFrame body-grid fixes. Status → ready-for-dev.
- 2026-06-28 — Story 1.3 implemented (dev-story): Rack engine, LookAndFeel extraction, shared palette, body-grid fixes, and a temporary sample-rack population wired into the editor at 1920×1200. Clean Release build (0/0). Visual render confirmation pending a run → Status stays in-progress.
- 2026-06-28 — Story 1.3 **done**: visually verified in the running app (zones, placement, dimmed disabled body, uniform look, fit). Window design width is 1520 (not 1920 — the auto-fit-down still applies). Status → done.
- 2026-06-28 — **Prototype work built ON TOP of 1.3** (throwaway sample, NOT formal stories; to be formalised via correct-course — see `deferred-work.md` "Prototype decisions pending formalization"):
  - Stereo + Master became rack modules in a new **MASTER BUS** zone (top row, right-aligned); legacy header Stereo/Master controls removed; header flattened, "Current State" grouped with Save/Load, title centred.
  - **LFO / NOISE / FILTER / DISTORTION** got their own enable toggle; "Off" removed from their comboboxes (engine gates via new `*On` bools, `.synthy` format unchanged — "Off" stays the on-disk marker).
  - **Grid refined 8 → 12 columns**; size classes redefined as 12-col spans (XS=2×1, S=3×1, M=4×1, L=4×2, XL=6×2) — decoupled from knob size; `ModuleFrame` body now fills the module width from its content (knobs centred). DISTORTION/FILTER carry TYPE combo + DRIVE/MIX (resp. CUTOFF/RESO).
  - Open for next session: column-width/row-packing optimisation (trailing empty cells in partial rows, MASTER a touch airy).
