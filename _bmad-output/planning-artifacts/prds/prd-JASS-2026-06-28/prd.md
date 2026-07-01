---
title: "JASS Standalone UI — Unified 19\" Rack Layout"
status: final
created: 2026-06-28
updated: 2026-06-28
---

# JASS Standalone UI — Unified 19" Rack Layout (PRD)

## 1. Overview

JASS is a JUCE-based software synthesizer (Standalone + VST3). Its feature set is rich and stable, but the editor UI has grown organically: modules are built three different ways — a dedicated `OscillatorPanel`, a reused `EffectPanel`, and many modules laid out by hand inline in the editor. The result, in the user's words, looks **"cobbled together" and unprofessional** rather than **"from one mold."**

This PRD covers a **UI-only redesign**: re-cast every sound source, modulator, filter, and effect as a **uniform module in a 19″-rack-style layout**, where every module shares the same anatomy, controls, and displays, and comes in a small set of **standard size classes**. No audio features, DSP, or parameters change.

## 2. Goals & Non-Goals

**Goals**
- The UI reads as a single, consistent, professional instrument — every module visibly "from one mold."
- Every module is built from **one shared module framework**, not three.
- A module's size is one of a **small fixed set of standard size classes** (combining rack height and footprint width).
- Layout is **compact and space-saving**, with minimal visual noise.
- Adding or restyling a module in the future is a small, declarative change — not bespoke layout code.

**Non-Goals**
- No new audio features or DSP modules. (One append-only exception: four enable bools were added so LFO/Noise/Filter/Distortion get a real header enable — see NFR3.)
- No change to the preset format or existing parameter IDs, and no audio-engine rework.
- No theming system / multiple skins (single minimal look).
- No resizable/zoomable editor: the rack targets a fixed window (resizing may come later).

## 3. Success Criteria

- **Consistency:** every module uses the shared framework; zero modules remain on bespoke inline layout code.
- **Size discipline:** every module maps to exactly one of **5 standard size classes** (XS/S/M/L/XL, see §5). No one-off dimensions.
- **Coverage:** all existing modules are migrated (see §6 inventory) with no loss of any current control, display, or behavior (reset ↺, enable, modulation rings, scope/spectrum, ADSR curve, played-frequency display, etc.).
- **Footprint:** the full rack fits the fixed target window (~1920×1200, as today) without scrolling.
- **Qualitative:** the user judges the result as "from one mold" / professional (the core acceptance bar).
- **Maintainability:** layout of a module is expressed declaratively (size class + control list); per-module `resized()` math is eliminated.

## 4. Users & Context

Single operator: the developer-musician using JASS standalone for sound design and play. No multi-role or multi-stakeholder concerns, so no formal personas or user journeys. The relevant "journey" is: open JASS → scan the rack → find a module by its consistent position/shape → tweak its knobs → see consistent feedback (rings, displays).

## 5. The Rack Model (core design)

The editor is a **vertical 19″ rack**: a fixed-width frame holding rows of modules. The **zone grouping** is: a **MASTER BUS** row on top (right-aligned) plus GENERATORS → MODULATION → PROCESSING, each introduced by a zone header.

**Standard size classes.** The rack is a fixed **12-column proportional grid** (the raster is decoupled from knob diameter). A size class is a **column span**; a module declares only its class and the rack places it:

| Class | Footprint (cols × units) | Intended for |
|------|--------------------------|--------------|
| **XS** | 2 × 1 | 1–2 controls (e.g. Master, Mix-Mode) |
| **S** | 3 × 1 | small modules |
| **M** | 4 × 1 | mid modules |
| **L** | 4 × 2 | rich modules (e.g. ADSR: knobs + envelope curve) |
| **XL** | 6 × 2 | wide visualisers (scope / spectrum) |

- One **rack-unit height** and the **12-column width** are fixed constants; modules occupy whole grid multiples only. Modules flow left-to-right within a zone and wrap to the next row.
- A module **never** sets its own arbitrary size; it declares its size class and the rack places it. A module's body derives its internal column count from its **content** (knobs centred), not from the knob diameter.

## 6. Functional Requirements

### FR Group A — Shared Module Framework
- **FR1.** A single module framework defines the uniform anatomy every module renders: **header row** (title, enable/bypass toggle, reset ↺) + **body** (controls and/or display) laid out by the framework, not by the module.
- **FR2.** The framework renders **one consistent control style** for knobs, combo boxes, toggles, and labels across all modules (one knob diameter per size class, consistent label placement, consistent spacing).
- **FR3.** Each module is defined **declaratively**: its size class, title, color/type tag, enable parameter (optional), reset parameter set, and an ordered list of controls. The framework lays these out automatically. The control vocabulary must cover every control kind that exists today:
  - **knob** (bound to a parameter ID, with optional display-value transform — see FR4),
  - **combo box** with either static items or **dynamically populated items** (e.g. the Wavetable bank list, refreshed at runtime),
  - **toggle**,
  - **action/trigger button** that is *not* a parameter — fires a callback (e.g. Karplus **PLUCK**),
  - **file-action button** that opens a file chooser and applies the result (e.g. Wavetable **LOAD WAV**).
- **FR4.** The framework preserves all current per-module affordances generically: **reset ↺**, **enable toggle**, **live modulation rings** on knobs, right-click value entry, shift-fine / wheel-step behavior, and a per-knob **display-value transform** so a knob can show a derived value while editing the underlying parameter (e.g. the OSC/Wavetable FREQ knob shows the actually-played frequency = base × played-note ratio, writing the base back on edit).

### FR Group B — Module Anatomy & Identity
- **FR5.** Every module shows: a **title**, an **enable/bypass control** (where the module can be on/off), a **reset ↺**, and its controls. Modules with no on/off (e.g. Master, ADSR) omit the toggle but keep identical header geometry.
- **FR6.** Each module carries a consistent **type/color tag** so a glance distinguishes generator vs modulator vs processor, while staying within the minimal palette.
- **FR7.** A module's enabled/bypassed state is shown by one consistent rule across all modules: when disabled, the module **body is dimmed** (reduced opacity) while the header stays fully lit, and the enable toggle is the single source of truth for the state. Modules without an on/off are always shown lit.

### FR Group C — Standard Sizes & Rack Layout
- **FR8.** Size classes live in a single data-driven table; **5 are defined and in use (XS/S/M/L/XL)** on the 12-column grid and every module is assigned one. The set stays extensible by one table row — never by per-module custom sizing.
- **FR9.** The rack arranges modules on a **fixed grid** of rack-units × columns; modules align to the grid with consistent gutters. No module overlaps a grid boundary.
- **FR10.** **Zone headers** (MASTER BUS / GENERATORS / MODULATION / PROCESSING) separate the groups and span the rack width. The MASTER BUS row sits on top (right-aligned) and holds the Master + Stereo modules.
- **FR11.** Graphical displays — **oscilloscope, spectrum, ADSR-envelope curve** — are themselves modules (size **L**) within the rack, not specially-placed exceptions.

### FR Group D — Migration & Header
- **FR12.** All existing modules are re-expressed on the framework: OSC 1–3, Sub, Noise, Karplus, Wavetable, ADSR, LFO, Arpeggiator, Filter, Distortion, Wavefolder, Bitcrusher, Chorus, Delay, Reverb, Stereo, Master — plus Mix-Mode and the display modules.
- **FR13.** No existing control, parameter binding, or behavior is dropped in migration; the played-frequency FREQ display and Mix-Mode coupling between OSCs are preserved.
- **FR14.** The **global header** (preset SAVE / LOAD / RANDOM / RESET, centred title, preset-name / "Current State" indicator in the Save/Load cluster) and the **on-screen keyboard** remain as fixed chrome (restyled to match the rack) outside the module grid. **Master volume and Stereo (width/time + enable) are themselves rack modules** in a dedicated **MASTER BUS** zone (top row of the rack, right-aligned) — consistent with "every module from one mold". The legacy header Master/Stereo controls are removed.

## 7. Non-Functional Requirements
- **NFR1 — Maintainability:** no module defines its own `resized()` geometry; layout is data-driven via the framework. This is the primary engineering win and a success gate.
- **NFR2 — Audio-thread safety:** the redesign is UI-only and must not violate the project's real-time rules (no allocation/locking on the audio thread; UI↔audio handoff stays via the existing `std::atomic` channels and APVTS attachments).
- **NFR3 — No format impact; one append-only param change:** the `.synthy` on-disk preset format is **untouched** and interop with the C# app is preserved. The redesign added **four append-only enable bools** to the APVTS (`filterOn`, `distortionOn`, `lfoOn`, `noiseOn`) so those four modules gain a real header enable like every other module — the sole parameter change. It is append-only (no existing ID renamed or reordered), and `"Off"` remains the on-disk disabled marker, mapped via `PresetIO::choiceOrOff`. No `kFormatVersion` bump.
- **NFR4 — VST3 parity:** the same editor should render correctly as the VST3 plugin editor; to be **validated** (currently untested). Not a launch blocker.
- **NFR5 — Performance:** repaint cost stays at or below today's (modulation rings, scope, spectrum continue to repaint only on meaningful change).

## 8. Open Questions
_(For the architecture phase — not blockers for this PRD.)_
- **OQ1.** Exact size-class math: rack column count, rack-unit height in pixels, and exact per-class control capacity (the §5 table fixes the model; the constants are still to set).
- **OQ2.** Per-module size-class assignment: which of the ~20 modules is S, M, or L (a first-pass mapping will accompany the architecture/epics).

## 9. Out of Scope
New audio features or DSP; preset-format changes; renaming existing parameter IDs; multiple themes/skins; resizable/zoomable editor; renaming the internal "Synthy" identifiers. (The four append-only enable bools of NFR3 are the one sanctioned parameter addition.)
