---
name: 'JASS Editor — Unified Rack UI'
type: architecture-spine
purpose: build-substrate
altitude: feature
paradigm: 'Declarative, data-driven component composition (descriptor → ModuleFrame; rack owns layout)'
scope: 'The JASS standalone/VST3 editor UI: the module framework, the rack grid, and the invariants every module shares. UI-only.'
status: final
created: '2026-06-28'
updated: '2026-06-28'
binds: [PRD prd-JASS-2026-06-28 FR1–FR14, NFR1–NFR5]
sources: ['prd-JASS-2026-06-28/prd.md', 'project-context.md']
companions: []
---

# Architecture Spine — JASS Editor Unified Rack UI

## Design Paradigm

**Declarative, data-driven component composition.** A module is *data* (a `ModuleDescriptor`), not a bespoke component. One generic `ModuleFrame` renders any descriptor; one `Rack` owns all placement. This replaces the three legacy build mechanics (`OscillatorPanel`, reused `EffectPanel`, hand-laid inline modules) that caused the "cobbled-together" look.

Layer → location:

| Layer | Lives in |
| --- | --- |
| Framework (frame, rack/grid, descriptor + control types, LookAndFeel) | `Source/UI/rack/` |
| Reusable display components | `Source/UI/` (`WaveformDisplay`, `SpectrumDisplay`, `EnvelopeDisplay`) |
| Standard knob | `Source/UI/SynthySlider.h` |
| Descriptor assembly + global chrome + rack ownership | `Source/UI/PluginEditor.*` |

## Invariants & Rules

### AD-1 — One generic ModuleFrame + declarative descriptor `[ADOPTED]`
- **Binds:** all editor modules (FR1–FR3).
- **Prevents:** per-module bespoke layout code — the divergence that produced the inconsistent UI.
- **Rule:** every module is expressed as a `ModuleDescriptor` rendered by the single `ModuleFrame`. No module subclass carries its own layout. Adding/editing a module is a data change, not a new component.

### AD-2 — The Rack owns all placement on a fixed grid `[ADOPTED]`
- **Binds:** rack + every module (FR8–FR10, NFR1).
- **Prevents:** modules choosing incompatible sizes/positions; scattered `resized()` math.
- **Rule:** layout is a fixed grid of columns × rack-units (column width `Wc`, unit height `Hu`, uniform gutters). A module declares **only** its size class — **S = 1×1**, **M = 2×1**, **L = 2×2** (columns × units) — and occupies whole grid multiples. All `resized()` geometry lives solely in the rack layout engine; no module computes its own bounds. `Wc`/`Hu`/column-count are seed (see Deferred), pinned in the mockup. The unit height is **compact** = header + one knob row, so S/M span 1 unit and L spans 2 (a 1:2 height ratio).
- **The size-class set is a single data-driven table** `class → { cols, units, slotCapacity, knobSize }`; today `{ S, M, L }`. A new class is added by appending **one table row** — never by per-module custom dimensions; a module always picks an existing class. A **4th class is anticipated** (e.g. a wide-display `W` spanning >2 columns) and the table is designed to accept it, but it is not defined until a module first needs it (see Deferred).
- **Body-slot capacity is fixed per class** so two modules of the same class lay out identically: **S = 3 slots, M = 6 slots, L = 12 slots** (a Knob / Combo / Toggle / Action / FileAction = 1 slot; a `Display` declares the slot count it spans). The frame asserts `usedSlots(body) ≤ capacity(sizeClass)` at construction; a module that overflows its class must be promoted (e.g. Wavetable → L). Slot counts are seed-tunable in the mockup.

### AD-3 — Single uniform knob size (provisional)
- **Binds:** all knobs across all size classes (FR2).
- **Prevents:** inconsistent control sizing between modules.
- **Rule:** one knob diameter for every module regardless of size class. **Revisit:** if large modules read as empty/unimportant, switch to per-size-class knob sizes (S→Small, M→Medium, L→Medium/Large).

### AD-4 — Fixed module descriptor + control vocabulary `[ADOPTED]`
- **Binds:** all modules (FR3, FR4).
- **Prevents:** ad-hoc control wiring; controls that exist in code but have no declarative form.
- **Rule:** `ModuleDescriptor = { sizeClass, title, typeTag(Generator|Modulator|Processor), enableParam?, resetParams[], body[] }`. `body` is an ordered list of `BodyElement`, each one of: `Knob{paramId,label,displayTransform?,modTarget?}`, `Combo{paramId,label,items:static|dynamicProvider}`, `Toggle{paramId,label}`, `Action{label,onClick,refreshes?}`, `FileAction{label,onChoose,refreshes?}`, `Caption` (static-text element with a `text` field; named to avoid the `juce::Label` clash), `Display{component,slots}`. The frame renders a uniform header (title + optional enable + reset ↺) and flows `body` into grid slots. A missing `enableParam` means the module is always-on (Master, ADSR, Mix-Mode) — header geometry stays identical, always lit.
- **`displayTransform` is a pair** `{ toDisplay(base, ratio), fromDisplay(shown, ratio) }` with a mandatory guard: when `ratio ≤ 0` (no note sounding) the transform is identity and write-back is suppressed — never divide a stale/zero ratio into the base param (protects FR4 FREQ display + NFR3 state integrity). The `ratio` arrives on the same read-only live-feed channel as modulation (AD-8).
- **Dynamic combos refresh declaratively:** an `Action`/`FileAction` may list `refreshes: [comboParamId]`; the frame re-polls those dynamic providers after the action fires (replaces the manual `refreshBankSelector()` for the Wavetable bank). No combo is refreshed by an external reference.
- **Reset writes only APVTS:** `resetParams[]` writes each param's default into APVTS; everything else (Displays, dynamic combos) reacts through its normal channel — no special reset path.

### AD-5 — Graphic displays are BodyElements `[ADOPTED]`
- **Binds:** scope, spectrum, ADSR-curve modules (FR11).
- **Prevents:** a separate custom-body path = a display special-case = the drift source.
- **Rule:** a display is a `Display{component, slots}` body element occupying grid slots through the *same* layout mechanism as controls. Scope/Spectrum = a module whose body is one `Display` spanning all slots; ADSR = knobs + one `Display`. Existing display components are reused as the wrapped `component`. When a module is disabled, the **whole body region dims uniformly, including any Display** (the display keeps its own poll/repaint; dimming is a frame-level overlay, not a per-component path).

### AD-6 — The frame owns parameter binding `[ADOPTED]`
- **Binds:** every parameter-bound control (FR4, NFR2/NFR3).
- **Prevents:** scattered, hand-wired attachments drifting from the descriptor.
- **Rule:** `ModuleFrame` creates and owns the APVTS attachment for each bound `BodyElement` from its `paramId`. The editor never declares per-control `*Attachment` members. Parameter IDs and the APVTS layout are untouched (inherited).

### AD-7 — One shared LookAndFeel; SynthySlider is the only knob `[ADOPTED]`
- **Binds:** all controls (FR2).
- **Prevents:** divergent control styling.
- **Rule:** a single `SynthyLookAndFeel` is set by the rack and applies to all modules. `SynthySlider` is the standard knob everywhere (rotary, right-click value entry, shift-fine drag, wheel-step, modulation ring). No module installs its own LookAndFeel.

### AD-8 — Modulation rings are declarative
- **Binds:** knobs that are LFO targets (FR4).
- **Prevents:** the editor holding fixed references to specific knobs (hand-wiring returning).
- **Rule:** a `Knob` may carry an optional `modTarget` (`Frequency|Amplitude|FilterCutoff`). The rack offers lookup "all knobs with `modTarget == X`". A single editor timer reads the processor's LFO atomic + active target and sets the matching knobs' rings. No knob ring is wired by direct reference.

### AD-9 — Cross-module coupling only through shared APVTS `[ADOPTED]`
- **Binds:** Mix-Mode and the three OSCs (FR13); any future inter-module dependency.
- **Prevents:** one module holding a reference to another (Mix-Mode reaching into OSC1/2/3) — a coupling the descriptor can't see.
- **Rule:** Mix-Mode is its own module (size **S**: a `Combo` + a `Label` caption). Its effect on how OSC 1/2/3 combine flows **only** through the shared `mixMode` APVTS param read by the audio engine — OSC descriptors carry no knowledge of it, and no module references another module. All cross-module state rides shared params.

### Dependency direction

```mermaid
graph TD
    PE[PluginEditor: builds descriptors, owns chrome] --> RACK[Rack: grid layout + module lookup]
    RACK --> MF[ModuleFrame]
    MF --> DESC[ModuleDescriptor / BodyElement types]
    MF --> CTRL[Controls: SynthySlider, Combo, Toggle, buttons]
    MF --> DISP[Display components]
    CTRL --> LNF[SynthyLookAndFeel]
    CTRL --> APVTS[APVTS attachments]
    PE --> PROC[Processor atomics: LFO value, played ratio]
    PROC -.read-only.-> CTRL
```

## Consistency Conventions

| Concern | Convention |
| --- | --- |
| Naming | Framework types `ModuleFrame`, `Rack`, `ModuleDescriptor`, `BodyElement`. One descriptor-builder per module (e.g. `makeOsc1Descriptor`). Existing `Synthy*` class-name prefix kept (inherited). |
| Data & formats | Parameter IDs come only from `Parameters::ID` (inherited, canonical); controls reference `paramId`, never literals. Size class is the only sizing input a module gives. |
| State & cross-cutting | APVTS is the single source of truth (inherited). UI→audio handoff via `std::atomic` only (inherited). Attachments owned by the frame. No allocation/locking on the audio thread (inherited). |

## Stack

| Name | Version |
| --- | --- |
| C++ | 20 (existing) |
| JUCE | vendored submodule (existing) |

_No new dependencies; this is a UI refactor on the current stack._

## Structural Seed

```text
Source/UI/
  rack/
    ModuleDescriptor.h    # descriptor + BodyElement variant types
    ModuleFrame.h/.cpp    # renders one descriptor: uniform header + body flow
    Rack.h/.cpp           # grid layout engine + zone headers + modTarget lookup
    SynthyLookAndFeel.*   # the single shared look (moved out of PluginEditor)
  SynthySlider.h          # standard knob (existing)
  WaveformDisplay.h       # reused as Display{component} (existing)
  SpectrumDisplay.h       # reused (existing)
  PluginEditor.h/.cpp     # builds descriptors, owns Rack + fixed chrome (preset header, keyboard)
```

Fixed chrome stays in `PluginEditor`, outside the rack grid (FR14): preset SAVE/LOAD/RANDOM/RESET, preset-name indicator, on-screen keyboard, **and the global Master volume + Stereo (width/time)** — these are master-bus controls, not rack modules, and live in the top header. `EnvelopeDisplay` moves into `rack/` or stays a display component — either is fine; it is consumed as a `Display`.

## Capability → Architecture Map

| PRD area | Lives in | Governed by |
| --- | --- | --- |
| Shared module framework (FR1–FR4) | `rack/ModuleFrame`, `rack/ModuleDescriptor` | AD-1, AD-4, AD-6 |
| Module anatomy & identity (FR5–FR7) | `ModuleFrame` header + typeTag + dimmed-body rule | AD-1, AD-4 |
| Standard sizes & rack layout (FR8–FR11) | `rack/Rack` | AD-2, AD-5 |
| Migration & header (FR12–FR14) | per-module descriptors in `PluginEditor`; chrome in `PluginEditor` | AD-1, AD-4 |
| Maintainability / RT / state (NFR1–NFR3) | rack engine; inherited constraints | AD-2, AD-6, conventions |

## Deferred

- **Exact grid constants** (`Wc`, `Hu`, column count, gutter, knob diameter) — pinned visually in the HTML rack mockup, then frozen as code constants.
- **Per-module size-class assignment** (which of the ~20 modules is S/M/L) — first pass in the mockup; finalized with epics/stories.
- **Per-size-class knob sizes** — only if AD-3's uniform-size attempt fails (its revisit condition).
- **Fourth size class (wide-display `W`)** — the class table (AD-2) is built to accept it; define the row when a module first needs >2-column display width. Not implemented now.
- **VST3 editor parity** — same editor should render in VST3; validate after standalone (NFR4), not a blocker.
- **Resizable / DPI-scalable rack** — out of scope; fixed target window for now.
