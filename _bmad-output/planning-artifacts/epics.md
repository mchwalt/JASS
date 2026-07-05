---
stepsCompleted: [1, 2, 3, 4]
inputDocuments:
  - _bmad-output/planning-artifacts/prds/prd-JASS-2026-06-28/prd.md
  - _bmad-output/planning-artifacts/architecture/architecture-JASS-2026-06-28/ARCHITECTURE-SPINE.md
  - _bmad-output/planning-artifacts/architecture/architecture-JASS-2026-06-28/rack-mockup.html
  - _bmad-output/project-context.md
---

# JASS Unified Rack UI - Epic Breakdown

## Overview

This document decomposes the JASS Standalone/VST3 editor UI redesign (PRD + Architecture Spine) into implementable stories. The work is UI-only: re-cast every module into a uniform 19" rack built from one generic `ModuleFrame` + declarative descriptors, placed by a `Rack` on a fixed data-driven grid. No audio features, DSP, parameters, or preset format change.

## Requirements Inventory

### Functional Requirements

FR1: A single module framework defines the uniform anatomy every module renders — a header row (title, enable/bypass toggle, reset ↺) plus a body of controls/displays laid out by the framework, not by the module.
FR2: The framework renders one consistent control style for knobs, combo boxes, toggles, and labels across all modules (one knob diameter per size class, consistent label placement and spacing).
FR3: Each module is defined declaratively (size class, title, type tag, optional enable param, reset param set, ordered body list). The control vocabulary covers: Knob (with optional display transform), Combo (static or dynamically-populated items), Toggle, Action/trigger button (e.g. PLUCK), FileAction button (e.g. LOAD WAV), Label, Display.
FR4: The framework preserves all current per-module affordances generically: reset ↺, enable toggle, live modulation rings, right-click value entry, shift-fine / wheel-step, and a per-knob display-value transform (FREQ shows played frequency = base × ratio, writing base back on edit).
FR5: Every module shows a title, an enable/bypass control (where applicable), a reset ↺, and its controls. Modules with no on/off (Master, ADSR, Mix-Mode) omit the toggle but keep identical header geometry.
FR6: Each module carries a consistent type/color tag (Generator / Modulator / Processor) within a minimal palette.
FR7: Enabled/bypassed state is unambiguous and consistent: disabled ⇒ body dimmed, header lit; the enable toggle is the single source of truth; always-on modules stay lit.
FR8: Size classes live in a single data-driven table; 3 are defined and in use (S/M/L), each module assigned one. The set is extensible by one table entry (a 4th class anticipated, not implemented now) — never by per-module custom sizing.
FR9: The rack arranges modules on a fixed grid of rack-units × columns, aligned with consistent gutters; no module overlaps a grid boundary.
FR10: Zone headers (GENERATORS / MODULATION / PROCESSING) separate the three groups and span the rack width.
FR11: Graphical displays — oscilloscope, spectrum, ADSR-envelope curve — are themselves modules (size L) within the rack.
FR12: All existing modules are re-expressed on the framework: OSC 1–3, Sub, Noise, Karplus, Wavetable, ADSR, LFO, Arpeggiator, Filter, Distortion, Wavefolder, Bitcrusher, Chorus, Delay, Reverb, plus Mix-Mode and the display modules (Scope, Spectrum). (Master + Stereo move to the header chrome — see FR14.)
FR13: No existing control, parameter binding, or behavior is dropped in migration; the played-frequency FREQ display and the Mix-Mode coupling between OSCs are preserved.
FR14: The global header (preset SAVE / LOAD / RANDOM / RESET, preset-name / "Current State" indicator, the master-bus Master volume and Stereo width/time) and the on-screen keyboard remain as fixed chrome outside the module grid.

### NonFunctional Requirements

NFR1: Maintainability — no module defines its own `resized()` geometry; layout is data-driven via the framework. Primary engineering win and a success gate.
NFR2: Audio-thread safety — UI-only; must not violate the project's real-time rules. UI↔audio handoff stays via the existing `std::atomic` channels and APVTS attachments.
NFR3: No state/format impact — parameter IDs, APVTS layout, and the `.synthy` preset format are untouched.
NFR4: VST3 parity — the same editor should render correctly as the VST3 plugin editor; to be validated (currently untested). Not a launch blocker.
NFR5: Performance — repaint cost stays at or below today's; modulation rings, scope, and spectrum repaint only on meaningful change.

### Additional Requirements

_From the Architecture Spine (AD-1…AD-9) and project-context.md — implementation-affecting:_

- **New framework code** under `Source/UI/rack/`: `ModuleDescriptor.h` (descriptor + BodyElement variant types), `ModuleFrame.h/.cpp` (uniform header + body flow), `Rack.h/.cpp` (grid layout engine, zone headers, modTarget lookup), `SynthyLookAndFeel.*` (moved out of `PluginEditor`). New `.cpp` files MUST be added to `target_sources` in `CMakeLists.txt`. (AD-1, Structural Seed)
- **Rack owns placement** on a fixed grid; the size-class table holds `{cols, units, slotCapacity, knobSize}`; body-slot capacity **S=3 / M=6 / L=12** enforced by a build-time assertion; compact unit height = header + one knob row, L spans 2 units. (AD-2)
- **displayTransform is a guarded pair** `{toDisplay(base,ratio), fromDisplay(shown,ratio)}` — ratio ≤ 0 ⇒ identity + no write-back. (AD-4)
- **Dynamic combos refresh declaratively** via `Action`/`FileAction.refreshes[]`; reset writes only APVTS defaults. (AD-4)
- **Display dimming** is a frame-level overlay covering the whole body uniformly. (AD-5)
- **The frame owns APVTS attachments** created from the descriptor; the editor declares no per-control `*Attachment` members. (AD-6)
- **One shared `SynthyLookAndFeel`** set by the rack; `SynthySlider` is the only knob. (AD-7)
- **Modulation rings are declarative** via `Knob.modTarget` + rack lookup + a single editor timer reading the processor's LFO atomic. (AD-8)
- **Cross-module coupling only through shared APVTS**; Mix-Mode is its own S module. (AD-9)
- **`PluginEditor`** builds the descriptors and owns the Rack + fixed chrome; `WaveformDisplay` / `SpectrumDisplay` / `EnvelopeDisplay` are reused as `Display` components.
- **Inherited constraints (binding):** APVTS is the single source of truth; parameter IDs/order in `Parameters.h` never change; no allocation/locking on the audio thread; UI↔audio via `std::atomic`; the "Synthy" naming (`.synthy`, `%AppData%\Synthy`, class names) is kept.

### UX Design Requirements

_No separate bmad-ux contract exists; for this UI redesign the Architecture Spine carries the UX role and `rack-mockup.html` is the visual reference. The visual/interaction requirements (size-class footprints S/M/L, type-tag colors, dimmed-disabled state, modulation rings, compact unit, Master/Stereo in the header chrome, Scope+Spectrum as adjacent L displays) are already captured in FR2/FR6/FR7/FR8/FR11/FR14 above and validated visually in the mockup._

### FR Coverage Map

FR1: Epic 1 — single module framework / uniform anatomy
FR2: Epic 1 — one consistent control style across modules
FR3: Epic 1 — declarative descriptor + control vocabulary
FR4: Epic 1 — generic affordances (reset, rings, value entry, display transform)
FR5: Epic 1 — header (title/enable/reset), identical geometry
FR6: Epic 1 — type/color tag
FR7: Epic 1 — dimmed-disabled / enable-as-truth
FR8: Epic 1 — data-driven size-class table (S/M/L, extensible)
FR9: Epic 1 — fixed grid placement
FR10: Epic 1 — zone headers
FR11: Epic 1 (Display element mechanism) / Epic 2 (Scope, Spectrum, ADSR-curve modules)
FR12: Epic 1 (generators) / Epic 2 (modulation, processing, displays) / Epic 3 (Master+Stereo to chrome)
FR13: Epic 1 (generators preserved) / Epic 2 (mod+processing preserved, incl. filter mod ring)
FR14: Epic 3 — header chrome + keyboard
NFR1: Epic 1 (established) / Epic 3 (final confirmation, legacy layout removed)
NFR2: All epics — audio-thread safety (constraint)
NFR3: All epics — no param-ID / APVTS / .synthy impact (constraint)
NFR4: Epic 3 — VST3 parity validation
NFR5: Epic 1 (repaint discipline) / All epics

## Epic List

### Epic 1: Rack Foundation & Generator Modules
Stand up the module framework (`ModuleFrame`, `Rack`, descriptor types, shared `SynthyLookAndFeel`) and the fixed grid, then bring the entire GENERATORS zone into the new uniform rack. After this epic, JASS opens with a real, consistent rack of all sound sources — built entirely from declarative descriptors with zero per-module layout code (NFR1 established).
**Modules:** OSC 1–3, Sub, Noise, Karplus, Wavetable, Mix-Mode.
**FRs covered:** FR1, FR2, FR3, FR4, FR5, FR6, FR7, FR8, FR9, FR10, FR11 (Display mechanism), FR12 (generators), FR13 (generators).

### Epic 2: Modulation & Processing Modules
Migrate the MODULATION zone (ADSR + envelope display, LFO, Arpeggiator) and the PROCESSING zone (Filter, Distortion, Wavefolder, Bitcrusher, Chorus, Delay, Reverb), plus the graphical display modules (Oscilloscope, Spectrum). After this epic the full synth signal path lives in the uniform rack.
**Modules:** ADSR, LFO, Arpeggiator, Filter, Distortion, Wavefolder, Bitcrusher, Chorus, Delay, Reverb, Oscilloscope, Spectrum.
**FRs covered:** FR11 (display modules realized), FR12 (modulation + processing + displays), FR13 (preservation incl. filter-cutoff mod ring).

### Epic 3: Header Chrome & Final Integration
Build the fixed header chrome (preset SAVE / LOAD / RANDOM / RESET, centred title, preset-name / "Current State" indicator) and migrate Master + Stereo as rack modules in the MASTER BUS zone; restyle the on-screen keyboard to match the rack; remove all legacy per-module layout code (incl. the old header Master/Stereo controls) from the editor; validate VST3 parity.
**FRs covered:** FR14, NFR4, final NFR1 confirmation.

## Epic 1: Rack Foundation & Generator Modules

Stand up the module framework and grid, then migrate the GENERATORS zone. After this epic JASS opens with a real, uniform rack of all sound sources, built entirely from declarative descriptors with no per-module layout code.

### Story 1.1: Module descriptor & control-vocabulary types

As a JASS developer,
I want a single declarative `ModuleDescriptor` data model with the full control vocabulary and a data-driven size-class table,
So that every module can later be expressed as data instead of bespoke layout code.

**Acceptance Criteria:**

**Given** the new `Source/UI/rack/ModuleDescriptor.h`
**When** the project builds
**Then** `ModuleDescriptor { sizeClass, title, typeTag, enableParam?, resetParams[], body[] }` and the `BodyElement` variants exist — `Knob{paramId,label,displayTransform?,modTarget?}`, `Combo{paramId,label,items:static|dynamicProvider}`, `Toggle`, `Action{label,onClick,refreshes?}`, `FileAction{label,onChoose,refreshes?}`, `Label`, `Display{component,slots}`
**And** a size-class table maps each class to `{cols, units, slotCapacity, knobSize}` on the 12-column grid with XS=2×1, S=3×1, M=4×1, L=4×2, XL=6×2 defined, and adding a class is a single table entry
**And** a build-time assertion helper rejects a descriptor whose body wildly exceeds its class slot capacity (a generous debug guard; layout itself derives column count from content, not from slotCapacity).

_(Reconciled 2026-07-01 via correct-course: implemented with the XS–XL 12-column model, not the original S/M/L 1×1/2×1/2×2. Story remains done.)_

### Story 1.2: ModuleFrame renders a descriptor

As a JASS developer,
I want one `ModuleFrame` component that renders any descriptor into a uniform header + body and owns its parameter bindings,
So that all modules share identical anatomy and zero layout code.

**Acceptance Criteria:**

**Given** a sample descriptor passed to `ModuleFrame`
**When** the frame is shown
**Then** it draws a uniform header (title, optional enable LED toggle, reset ↺) and flows the body elements into grid slots, with no layout math in the module itself
**And** the frame creates and owns the APVTS attachment for every parameter-bound element (moving a knob updates its param; no `*Attachment` members live in the editor)
**And** when the module is disabled the whole body region dims uniformly while the header stays lit, with the enable toggle as the single source of truth
**And** pressing reset ↺ writes only the descriptor's `resetParams` defaults into APVTS.

### Story 1.3: Rack grid layout engine & zone headers

As a JASS developer,
I want a `Rack` that places module frames on a fixed columns×units grid with zone headers and the shared look,
So that modules align consistently and the layout lives in exactly one place (NFR1).

**Acceptance Criteria:**

**Given** a set of S/M/L modules handed to the `Rack`
**When** the editor lays out
**Then** each module occupies whole grid multiples on the 12-column grid (XS=2×1, S=3×1, M=4×1, L=4×2, XL=6×2) with uniform gutters and no module overlaps a grid boundary
**And** the MASTER BUS / GENERATORS / MODULATION / PROCESSING zone headers span the rack width and separate the groups
**And** a single `SynthyLookAndFeel` is set once by the rack and applies to all modules
**And** the full rack fits the fixed target window (~1920×1200) without scrolling.

### Story 1.4: Live modulation rings & display-value transforms

As a JASS player,
I want knobs to show live modulation rings and the FREQ knob to show the actually-played frequency,
So that the new rack preserves the existing visual feedback exactly.

**Acceptance Criteria:**

**Given** a `Knob` declares a `modTarget` (Frequency | Amplitude | FilterCutoff)
**When** the LFO targets that destination
**Then** a single editor timer reads the processor's LFO atomic and animates the ring on every knob the rack reports for that `modTarget` (repainting only on meaningful change)
**And** a knob with a `displayTransform` shows the derived value (FREQ = base × played ratio) and writes the base back on edit
**And** when the played ratio ≤ 0 (no note sounding) the transform is identity and no write-back occurs (no divide-by-zero, base param never corrupted).

### Story 1.5: Migrate the GENERATORS zone

As a JASS player,
I want all sound-source modules rebuilt as descriptors in the rack,
So that the generator section looks and behaves as one consistent unit.

**Acceptance Criteria:**

**Given** the framework from Stories 1.1–1.4
**When** JASS opens
**Then** OSC 1–3, Sub, Noise, Karplus, Wavetable and Mix-Mode appear in the GENERATORS zone, each with identical header anatomy and their assigned size class
**And** every control is bound and functional, with no control, binding or behavior lost versus the old UI
**And** Karplus **PLUCK** (Action) triggers a pluck, and Wavetable **LOAD WAV** (FileAction) loads a file and refreshes the dynamic bank combo declaratively
**And** Mix-Mode is its own module whose value changes how OSC 1/2/3 combine purely through the shared `mixMode` APVTS param (no module references another)
**And** the audio engine, parameter IDs and `.synthy` format are untouched (NFR2, NFR3).

## Epic 2: Modulation & Processing Modules

Migrate the modulation and processing zones plus the graphical displays, so the full synth signal path lives in the uniform rack.

### Story 2.1: Migrate the MODULATION zone

As a JASS player,
I want the ADSR, LFO and Arpeggiator rebuilt as rack modules,
So that the modulation section matches the generators.

**Acceptance Criteria:**

**Given** the rack framework
**When** JASS opens
**Then** ADSR (size L: A/D/S/R knobs + the envelope-curve `Display`), LFO (M) and Arpeggiator (M) appear in the MODULATION zone with uniform anatomy
**And** the ADSR envelope-curve display renders inside the module as a `Display` body element and updates as A/D/S/R change
**And** all controls are bound and no behavior is lost versus the old UI.

### Story 2.2: Migrate the PROCESSING effect modules

As a JASS player,
I want Filter and all effect modules rebuilt as rack modules,
So that the processing section is consistent and complete.

**Acceptance Criteria:**

**Given** the rack framework
**When** JASS opens
**Then** Filter, Distortion, Wavefolder, Bitcrusher, Chorus, Delay and Reverb appear in the PROCESSING zone
**And** the Filter cutoff knob shows its modulation ring when the LFO targets FilterCutoff (via `modTarget`)
**And** every control is bound, the per-voice signal-chain order is unchanged, and the final [-1,1] clamp / RT rules are respected (NFR2).

_(Reconciled 2026-07-05 via Story 2.2: sizing is content-driven, not "uniform S". FILTER and DISTORTION are **M** (each carries a TYPE combo + two knobs); WAVEFOLD/BITCRUSH/CHORUS/DELAY/REVERB are **S** (three knobs). Consistent with the correct-course AD-2 revision that decoupled layout from a fixed per-class slot count.)_

### Story 2.3: Add the graphical display modules

As a JASS player,
I want the oscilloscope and spectrum as rack display modules,
So that the visualizations are first-class, consistent parts of the rack.

**Acceptance Criteria:**

**Given** the `Display` body-element mechanism
**When** JASS is playing
**Then** the Oscilloscope and Spectrum render live as size-XL modules placed side by side (forming a display band)
**And** each reuses its existing display component (`WaveformDisplay` / `SpectrumDisplay`) and repaints only on meaningful change (NFR5).

_Decision (2026-07-01): the display modules get **no dedicated VISUALIZATION zone header** — a header for just two passive visualisers would cost a full row of vertical space in the fixed window for no navigation benefit. They read as a display band by form + placement alone. Zones stay MASTER BUS / GENERATORS / MODULATION / PROCESSING._

_Requirement (2026-07-02, restore C# features): (a) the Oscilloscope module gets a **selectable time-base / zoom** (1 / 2 / 5 / 10 / 25 ms) as a Combo + a **left-side ms scale** (axis ticks + labels in `WaveformDisplay::paint`); displayed samples = `ms × sampleRate/1000`. (b) the **Spectrum** module gets its own **scale** too (frequency axis, and level/dB axis, drawn in `SpectrumDisplay::paint`). Both are pure UI/drawing, no audio change._

## Epic 3: Header Chrome & Final Integration

Build the fixed chrome, remove the legacy layout code, and validate the plugin build — finishing the redesign.

### Story 3.1: Global header chrome + MASTER BUS modules

As a JASS player,
I want preset controls in a fixed top header and the Master + Stereo controls as rack modules in a MASTER BUS zone,
So that global preset actions sit apart from the rack while Master/Stereo follow the same "one mold" as every other module.

**Acceptance Criteria:**

**Given** the redesigned editor
**When** JASS opens
**Then** the header shows SAVE / LOAD / RANDOM / RESET, a centred title, and the preset-name / "Current State" indicator (in the Save/Load cluster), outside the module grid — the legacy header Master/Stereo controls are gone
**And** Master and Stereo are **rack module descriptors in the MASTER BUS zone** (top row, right-aligned), built on the framework like every other module
**And** all preset actions work as before and the "Current State" indicator still reflects unsaved changes
**And** Master and Stereo are bound to their existing params (no audio behavior change).

### Story 3.2: Restyle the on-screen keyboard

As a JASS player,
I want the on-screen keyboard restyled to match the rack and kept as fixed chrome,
So that the whole window reads as one instrument.

**Acceptance Criteria:**

**Given** the redesigned editor
**When** JASS opens
**Then** the keyboard sits as fixed chrome (below the grid), visually matching the rack
**And** it still plays notes and the computer-keyboard octave shift (z / x) works as before.

### Story 3.3: Remove legacy layout code & confirm NFR1

As a JASS developer,
I want all bespoke per-module layout code removed,
So that the "no per-module resized()" invariant is actually true and the cobbled-together code is gone.

**Acceptance Criteria:**

**Given** every module now runs on the framework
**When** the legacy `OscillatorPanel`, reused `EffectPanel`, inline per-module layout / scattered `*Attachment` members, and the old header Master/Stereo controls (now MASTER BUS modules) are deleted
**Then** the project builds cleanly with JUCE warning flags on
**And** no module defines its own `resized()` layout geometry (layout lives only in the rack engine)
**And** the editor is functionally identical to before the cleanup.

### Story 3.4: Validate VST3 parity

As a JASS developer,
I want the VST3 editor verified in a host,
So that the rack UI is confirmed to work as a plugin, not only standalone.

**Acceptance Criteria:**

**Given** a Release build of the JASS VST3
**When** it is loaded in REAPER v7.65
**Then** the rack editor renders and operates identically to the standalone (modules, controls, displays, rings)
**And** any divergence from the standalone is recorded as a follow-up (NFR4; not a launch blocker).
