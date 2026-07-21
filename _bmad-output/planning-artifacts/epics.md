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
FR5: Every module shows a title, an enable toggle, a reset ↺, and its controls, with identical header geometry. _(Revised 2026-07-05, Story 2.4: EVERY module has an enable toggle — including the formerly always-on Master, ADSR and Mix-Mode, which gained real `masterOn`/`adsrOn`/`mixModeOn` params (default on). The enabler's value source may be a user param, a derived predicate (Mix-Mode = osc1&&osc2 for its dim state), or static; the toggle is always present.)_
FR6: Each module carries a consistent type/color tag (Generator / Modulator / Processor) within a minimal palette.
FR7: Enabled/bypassed state is unambiguous and consistent: disabled ⇒ body dimmed, header lit; the enable toggle is the single source of truth; always-on modules stay lit.
FR8: Size classes live in a single data-driven table; 3 are defined and in use (S/M/L), each module assigned one. The set is extensible by one table entry (a 4th class anticipated, not implemented now) — never by per-module custom sizing.
FR9: The rack arranges modules on a fixed grid of rack-units × columns, aligned with consistent gutters; no module overlaps a grid boundary.
FR10: Zone headers (GENERATORS / MODULATION / PROCESSING) separate the three groups and span the rack width.
FR11: Graphical displays — oscilloscope, spectrum, ADSR-envelope curve — are themselves modules (size L) within the rack.
FR12: All existing modules are re-expressed on the framework: OSC 1–3, Sub, Noise, Karplus, Wavetable, ADSR, LFO, Arpeggiator, Filter, Distortion, Wavefolder, Bitcrusher, Chorus, Delay, Reverb, plus Mix-Mode and the display modules (Scope, Spectrum). (Master + Stereo move to the header chrome — see FR14.)
FR13: No existing control, parameter binding, or behavior is dropped in migration; the played-frequency FREQ display and the Mix-Mode coupling between OSCs are preserved.
FR14: The global header (preset SAVE / LOAD / RANDOM / RESET, preset-name / "Current State" indicator, the master-bus Master volume and Stereo width/time) and the on-screen keyboard remain as fixed chrome outside the module grid.

_Epic 4 — Rack Customization (added 2026-07-10):_
FR15: The user can show or hide individual modules; a hidden module is removed from the layout (not merely dimmed) and the rest re-pack. Hiding is UI-only — a hidden module's parameters and audio processing are unaffected.
FR16: The user can show or hide an entire zone (with its zone header) as a unit.
FR17: The user can move a module to a different zone via drag & drop; the module keeps its identity/type tag, only its placement changes.
FR18: The user can reorder modules within a zone via drag & drop.
FR19: The customized layout (per-module visibility, zone assignment, position/order) persists with the preset and restores on load; a single "reset layout" affordance restores the built-in default without touching audio parameters.
FR20: Every module has a stable identity and a declared default zone so the default layout is reproducible and a customized layout is a delta against it.

_Epic 5 — Flexible Mix Routing (added 2026-07-11):_
FR21: MIX MODE's RingMod/FM coupling operates on two user-selectable oscillators (Source A, Source B ∈ {OSC 1, OSC 2, OSC 3}) instead of the fixed OSC1↔OSC2. FM: A modulates B (carrier); RingMod: A×B; the remaining OSC is summed plainly; Additive unchanged. Defaults A=OSC1/B=OSC2 (prior behaviour). Append-only params; missing ⇒ default coupling. First sanctioned DSP change, scoped to the 3 OSCs.

_Epic 6 — Module Discoverability (added 2026-07-11):_
FR22: Each module can carry a short help description. The module header shows a small circled-"i" info icon; clicking it opens a **movable** help panel with the module's title + description. The panel stays open until the user closes it via its top-right "✕" or the ESC key (it is NOT a hover/auto-dismiss popup). The help text is optional per module (a module without one shows no info icon). UI-only: no audio/param/`.synthy` impact.
FR23: The online help is **multi-language**, starting with **English (EN)** and **German (DE)**. Help texts live in **language resource files** (`Resources/help_en.json`, `help_de.json`, keyed by module id) embedded via `juce_add_binary_data` — not inline in code. A language selector combo box in the JASS header switches the active language; the help panel renders the description in the selected language (EN fallback), and an already-open panel updates on switch. The language choice persists as a global app setting (not in `.synthy`). Extensible: a new language = a new resource file + combo entry.

_Epic 7 — UI Polish (added 2026-07-12):_
FR24: Module size classes are tightened so each module's footprint fits its actual control count — no module is unnecessarily large/wide — while honouring the size-class rules (AD-2/AD-3): rotaries keep their minimum diameter, combos keep enough width, single-control modules use the smallest class, and the uniform header anatomy (title · info · reset · enable) is unchanged. OSC 1/2/3 stay at the 3-per-row width. UI-only; no audio/param/`.synthy` impact.
FR25: The rack grid is refined from 12 to 24 columns and the size classes are renamed to their grid footprint (`W{cols}H{rows}`, e.g. `W8H1`) instead of T-shirt sizes — giving the finer granularity needed to size small modules tightly and making the grid maths explicit. Visually neutral (each old class maps to the doubled column count); enables FR24.

_Epic 8 — Modulation Matrix (added 2026-07-14):_
FR26: Modulation is routed through a **matrix of `{source, target, amount}` slots** instead of a single hard-wired source→target. Multiple slots may target the same destination; their bipolar amounts **sum** and are applied **once** around the captured base value, reusing the existing per-target application curves/clamps. Source vocabulary v1: LFO 1, Envelope (ADSR), Velocity. Target vocabulary v1 = the seven currently-modulatable destinations (Pitch, Amplitude, Filter Cutoff, Filter Resonance, WT Position, Formant Vowel, Wavefold Drive). Both vocabularies are append-only (LFO 2–4, Macros, Voice-Random, Evolution, Pan, FM-Amount… are later stories). The change replaces the single-target if/else apply in `SynthVoice` and the single-target mod-ring system; it is the enabler for macros, per-voice random, an evolution module, and additional LFOs. Second sanctioned DSP change; kept surgical (engine + slots + the MOD MATRIX module + ring generalization). Default (no active slots) is audibly identical to before.
FR27: The matrix persists **append-only, interop-safe**: N fixed slots as appended APVTS params (`modSlot{n}Source/Target/Amount` + `modMatrixOn`), serialized to `.synthy` as appended fields and to DAW state via APVTS; **no `kFormatVersion` bump**, no existing ID renamed/reordered. A preset lacking them (older build or the C# app) loads with all slots Off / matrix on (missing ⇒ default), so old presets and C# are unaffected. The existing LFO's built-in TARGET/DEPTH continue to work as an implicit routing on the same engine (zero-regression back-compat).

_Epic 10 — Spatialization (added 2026-07-21):_
FR28: Each generator (OSC 1–3, SUB, NOISE, KARPLUS, WAVETABLE) can be placed in the **stereo field** via a per-generator **PAN** control (−1 L … +1 R, default 0 = center). The per-voice mix becomes 2-channel: each generator's contribution is equal-power panned into L/R before the effect chain. Append-only param (`<gen>Pan`, missing ⇒ center), no `FormatVersion` bump; the default (all centered) is audibly identical to today's mono sum folded to both channels. This is the mono→N-channel voice refactor and a future mod-matrix target (Pan). Ships to every device/host.
FR29: **Optional** SURROUND output (Quad 4.0 / 4.1 / 5.1) the user selects when the device/host grants a multi-channel output bus. Each generator can be assigned a discrete channel — **FL, FR, FM (front-center ≈ mono), RL, RR**, plus SUB/LFE for `.1` — generalizing FR28's pan. When the surround bus is unavailable the engine renders internally and **down-mixes to stereo** so no preset is silent. First change to the output bus (`isBusesLayoutSupported` + alternative `BusesProperties`). Opt-in, gated on a surround-capable test setup.
FR31: **Binaural (Kunstkopf)** output mode — the per-generator spatial/surround placement rendered to **stereo via HRTF** so it is heard in 3D on ordinary headphones (no surround hardware). JUCE has no built-in HRTF renderer/data; implementable via `juce::dsp::Convolution` with embedded HRIRs (dataset, licence check) or a lighter **parametric** binaural (ITD + ILD + head-shadow low-pass, no assets — recommended first). Consumes the same position data as FR28/FR29; it is a rendering mode, always available (stereo out).
FR30: Spatialization is a switchable **Output Mode** — **Mono / Pseudo-Stereo / Stereo-Pan / Surround / Binaural** — NOT a replacement. The existing **mono sum and pseudo-stereo** paths are preserved verbatim as the first two modes (default = Pseudo-Stereo, exactly as today); PAN and channel assignment only take effect in Stereo-Pan / Surround. The mode is an append-only persisted param (missing ⇒ Pseudo-Stereo), so every existing preset is byte-identical. Hard user constraint: the current solution must not be broken.

### NonFunctional Requirements

NFR1: Maintainability — no module defines its own `resized()` geometry; layout is data-driven via the framework. Primary engineering win and a success gate.
NFR2: Audio-thread safety — UI-only; must not violate the project's real-time rules. UI↔audio handoff stays via the existing `std::atomic` channels and APVTS attachments.
NFR3: No state/format impact — parameter IDs, APVTS layout, and the `.synthy` preset format are untouched.
NFR4: VST3 parity — the same editor should render correctly as the VST3 plugin editor; to be validated (currently untested). Not a launch blocker.
NFR5: Performance — repaint cost stays at or below today's; modulation rings, scope, and spectrum repaint only on meaningful change.
NFR6: Layout persistence is append-only & interop-safe (Epic 4) — the custom layout is stored as append-only standard APVTS params in `.synthy`; no format-version bump, no existing ID renamed/reordered; a preset lacking the layout params (older build or the C# app) loads with the built-in default layout (missing ⇒ default).

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

_Epic 4 additions (AD-10…AD-12):_

- **Ordered layout model is the single source of truth (AD-10):** `RackLayout = [ { id, zone, position, visible } ]` keyed by the descriptor's stable `id`. Show/hide, move and reorder mutate **only** this model, then the rack re-runs the single `layout()` packing path — no component computes its own bounds (NFR1 holds dynamically). **Default zone + default order move onto the `ModuleDescriptor`** (off the `addModule` call-site); default layout is reproducible from descriptors, custom layout is a delta. `typeTag` stays identity/colour and never changes on move.
- **Persistence is append-only, interop-safe, default-on-missing (AD-11):** `RackLayout` serializes as append-only standard APVTS params in `.synthy`; no `kFormatVersion` bump; missing ⇒ built-in default (mirrors the `MasterOn/AdsrOn/MixModeOn` back-compat pattern); C# ignores until it mirrors. "Reset layout" restores descriptor defaults, touches no audio param.
- **Width fixed, height auto-fits (AD-12):** window width stays 1520 px; after any layout mutation the rack recomputes `preferredHeight(width)` from the visible modules and the editor re-applies `setSize`. Automatic, not user-drag/zoom.

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
FR15: Epic 4 — show/hide individual modules
FR16: Epic 4 — show/hide entire zones
FR17: Epic 4 — drag & drop modules between zones
FR18: Epic 4 — reorder modules within a zone
FR19: Epic 4 — persist custom layout + reset layout
FR20: Epic 4 — stable id + declared default zone on descriptor (foundation)
NFR6: Epic 4 — append-only, interop-safe layout persistence
FR21: Epic 5 — selectable MIX MODE sources (A/B among OSC 1/2/3)
FR22: Epic 6 — per-module online help (header info icon → movable description panel)
FR23: Epic 6 — multi-language help (EN/DE) with a header language selector
FR24: Epic 7 — tighten module size classes to fit control counts (UI polish)
FR25: Epic 7 — finer 24-column grid + column-based size-class names (enabler)
FR26: Epic 8 — accumulating modulation matrix (source→target slots, sum-per-target) replacing the single-target apply
FR27: Epic 8 — append-only, interop-safe matrix persistence + implicit legacy-LFO routing (zero regression)
FR28: Epic 10 — per-generator stereo PAN (mono→2-channel voice mix; ships to everyone)
FR29: Epic 10 — optional surround output (4.0/4.1/5.1) + per-generator channel assignment (FL/FR/FM/RL/RR + LFE), stereo down-mix fallback
FR30: Epic 10 — switchable Output Mode (Mono / Pseudo-Stereo / Stereo-Pan / Surround / Binaural); legacy mono + pseudo-stereo preserved as default (not broken)
FR31: Epic 10 — binaural (Kunstkopf/HRTF) rendering to stereo headphones (dsp::Convolution + HRIR, or parametric ITD/ILD)

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

### Epic 4: Rack Customization
Let the user tailor the rack: show/hide individual modules and whole zones, drag modules between zones, and reorder within a zone — with the customized layout persisted in the preset and resettable to the built-in default. Built foundation-first: 4.1 introduces an ordered `RackLayout` data model (default zone/order on the descriptor) as the single source of truth; 4.2 adds the first visible mutation (show/hide) so there is real non-default state; 4.3 then persists that state and adds reset-layout; 4.4 adds drag & drop. The window height auto-fits the visible modules.
**FRs covered:** FR15, FR16, FR17, FR18, FR19, FR20, NFR6 (and NFR1 held dynamically).
_(Story order revised 2026-07-10: show/hide (4.2) precedes persistence (4.3) — persisting before any layout-mutating UI would only ever store the default, leaving nothing to verify.)_

### Epic 5: Flexible Mix Routing
Make the MIX MODE (RingMod / FM) coupling operate on two user-selectable oscillators (A/B ∈ OSC 1/2/3) instead of the fixed OSC1↔OSC2 — the natural consequence of freely arrangeable modules. The first sanctioned audio/DSP change, kept surgical (only the OSC-mix block + two append-only params); scoped to the three OSCs.
**FRs covered:** FR21.

### Epic 6: Module Discoverability
Help players learn what each module does without a manual: an optional per-module help description, opened from a circled-"i" info icon in the module header into a movable help panel (closed via "✕" or ESC). The help is multi-language (EN/DE to start), switched by a language selector in the JASS header. UI-only, built on the existing `ModuleDescriptor`/`ModuleFrame`/`Rack` framework. Pulled forward from the backlog (`docs/Feature_Ideas.md`) after Epic 5 + the Self-FM feature.
**FRs covered:** FR22, FR23.

### Epic 7: UI Polish
Tighten module sizing so each module's footprint matches its real control count. Done in two steps: **7.1** refines the grid 12→24 columns and renames the size classes to their column footprint (`W{cols}H{rows}`) — the enabler that makes tight small widths possible and the maths explicit (visually neutral); **7.2** assigns the tighter classes module by module (with the user's eye), incl. STEREO and the MASTER header-title fix. Data-driven size-class table (AD-2), rotary-minimum (AD-3), combo-width and single-control rules honoured; uniform header anatomy kept. UI-only; no audio/DSP/param/`.synthy` change.
**FRs covered:** FR24, FR25.

### Epic 8: Modulation Matrix
Give JASS the "movement layer" (`docs/JASS_Ideen_Merge.md` §2): replace the single hard-wired source→target modulation with an **accumulating matrix** of `{source, target, amount}` slots. 8.1 builds the engine (decouple source from target, sum per target, apply once) + the source set (LFO 1, Envelope, Velocity) + the seven existing targets + N generic slots + the MOD MATRIX module + generalized mod rings + append-only persistence — with the existing LFO folded in as an implicit routing so the default is byte-identical. This is the **foundation** that makes the rest of the roadmap cheap: later stories add LFO 2–4, Macros + A/B morph, a Per-Voice-Random/Drift source, an Evolution module, and new targets (Pan, FM-Amount, FX mixes) — each just "another source/target on the matrix". Answers the 2026-07-14 LFO question (multiply vs. extend): neither — sources become value providers on the matrix. Second sanctioned DSP change, kept surgical.
**FRs covered:** FR26, FR27.

### Epic 10: Spatialization (per-generator panning & optional surround)
Move JASS out of mono: place each generator in space. **10.1** (ships to everyone) gives every generator a **PAN** control and refactors the per-voice mix from mono to 2-channel (equal-power L/R) — the foundation, with the default byte-identical to today. **10.2** (opt-in) adds a user-selectable **surround** output (4.0/4.1/5.1) with per-generator **channel assignment** (FL/FR/FM-center/RL/RR + LFE) when the device/host grants a multi-channel bus, and an automatic stereo down-mix otherwise. First change to the output bus and the largest sanctioned DSP change so far → staged, stereo-first, surround gated on a surround-capable test setup. **Hard invariant:** it is a switchable **Output Mode** (Mono / Pseudo-Stereo / Stereo-Pan / Surround / **Binaural**) — the existing mono + pseudo-stereo paths are preserved verbatim (default), never broken. **10.3** adds **Binaural/Kunstkopf** (HRTF) rendering so the spatial mix is heard in 3D on ordinary headphones — likely the most practical way to deliver "surround" to typical users. Full design, feasibility ("does it make sense"), and open questions in `_bmad-output/implementation-artifacts/10-1-spatialization-panning-surround.md`.
**FRs covered:** FR28, FR29, FR30, FR31.

### Epic 9: Release Readiness & Versioning
Make JASS shippable and trackable. **9.1** prepared the repository and pushed it to a **private** GitHub repo (README, LICENSE/GPLv3, `.gitattributes`, docs cleanup, screenshots). **9.2** adds a **CalVer** app version (shown in the UI/About), a `CHANGELOG.md` (Keep a Changelog style) coupled to GitHub releases/tags, and hardens **preset-format versioning & migration** (robust conversion with backups, fix silent conversion failures, surface the preset's format version) — motivated by the 2026-07 conversion mishaps (Helikopter/whuwhu) that showed migration needs to be dependable. The app CalVer and the preset `FormatVersion` integer remain independent contracts.

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
**And** the MASTER BUS / GENERATORS / MODULATION / PROCESSING / VISUALIZATION zone headers span the rack width and separate the groups
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

_Decision (2026-07-01, REVERSED 2026-07-11): originally the scope + spectrum got no dedicated zone (a header for two passive visualisers seemed to waste a row in the fixed static layout). With Epic 4 making zones user-customizable (show/hide, move, reorder), a dedicated **VISUALIZATION** zone now earns its place — the visualizers are a group you'd want to hide or relocate as a unit. Scope + Spectrum live in the **VISUALIZATION** zone (bottom); zones are now MASTER BUS / GENERATORS / MODULATION / PROCESSING / VISUALIZATION._

_Requirement (2026-07-02, restore C# features): (a) the Oscilloscope module gets a **selectable time-base / zoom** (1 / 2 / 5 / 10 / 25 ms) as a Combo + a **left-side ms scale** (axis ticks + labels in `WaveformDisplay::paint`); displayed samples = `ms × sampleRate/1000`. (b) the **Spectrum** module gets its own **scale** too (frequency axis, and level/dB axis, drawn in `SpectrumDisplay::paint`). Both are pure UI/drawing, no audio change._

### Story 2.4: Universal module enablers (Master / ADSR / Mix-Mode overridable)

_Added 2026-07-05 (mid-sprint change, user-directed). Makes the enabler truly universal: the three formerly always-on modules gain real, user-overridable enable params._

As a JASS player,
I want every module — including Master, the ADSR envelope, and Mix-Mode — to have a real on/off enable,
So that the rack has one uniform anatomy and I can actually bypass those modules.

**Acceptance Criteria:**

**Given** the rack framework
**When** JASS opens
**Then** Master, ENVELOPE-ADSR and MIX MODE show interactive enable toggles bound to new `masterOn`/`adsrOn`/`mixModeOn` params (default on)
**And** Master off = muted output, ADSR off = envelope bypassed (constant gain), Mix-Mode off = plain additive OSC sum
**And** Mix-Mode's effective lit state = `mixModeOn && osc1On && osc2On` (the derived condition dims it; the audio additive-fallback keys off `mixModeOn` only)
**And** the three new bools round-trip through `.synthy` append-only (missing field = enabled), so old presets and the C# app are unaffected (NFR3 softened; C# mirror owed — see deferred-work).

_Revises FR5/FR7 (every module has an enabler) and softens NFR3 (append-only format extension), consistent with the 2026-07-01 enable-split precedent._

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

## Epic 4: Rack Customization

Layer user-tailorable layout on top of the unified rack via a **reorderable customization list** (not on-rack drag & drop): 4.1 builds the ordered `RackLayout` model; 4.2 is a customization panel where the list controls per-module visibility, order, and zone (list order = screen position); 4.3 persists that layout and adds reset. No audio feature, DSP, parameter-ID or preset-format-structure change — only an append-only layout field. Verification for every story = clean build + the running app (no unit-test framework); the built-in default layout must be byte-for-byte unchanged until the user customizes.

_(Approach revised 2026-07-11: a reorderable list-panel replaces on-rack drag & drop — simpler and more robust, and it unifies show/hide + reorder + zone-move in one place. Former Story 4.4 (on-rack drag & drop) is folded into Story 4.2.)_

### Story 4.1: Ordered RackLayout model + descriptor-declared default zone/order

As a JASS developer,
I want the rack to render from one ordered `RackLayout` data model (with each module's default zone and order declared on its descriptor),
So that later customization features (show/hide, drag & drop, persistence) all mutate a single authoritative model instead of relying on implicit insertion order.

**Acceptance Criteria:**

**Given** the current rack (zone passed to `Rack::addModule(zone, desc)`, within-zone order = insertion order, no visibility concept)
**When** the model is introduced
**Then** each `ModuleDescriptor` declares its **default zone** and **default order** (moved off the `addModule` call-site), keyed by its existing stable `id`
**And** the rack builds a `RackLayout = [ { id, zone, position, visible } ]` from those defaults and **renders exclusively from that model** (placement flows model → `layout()`; nothing reads insertion order anymore)
**And** `layout()` remains the single packing path (AD-2/NFR1): no component computes its own bounds
**And** `typeTag` stays identity/colour only (unchanged by the refactor)
**And** the app is visually **identical** to before — this is a pure internal refactor with no user-visible change, and no audio/param/format impact (NFR1, NFR2, NFR3).

### Story 4.2: Rack customization panel — show/hide, reorder & move between zones (auto-fit height)

As a JASS player,
I want a panel with a reorderable list of all modules where I can toggle visibility and drag items to change their order and zone,
so that I can tailor which modules show and where they sit — the list order is the on-screen order.

**Acceptance Criteria:**

**Given** the `RackLayout` model from Story 4.1
**When** I open the customization panel (from the MODULES button) and toggle a module's visibility
**Then** its `visible` flips in the layout model, it is added to / removed from the rack (hidden = removed, not merely dimmed per FR7), the rack **re-packs**
**And** visibility is **coupled to the module's enable on the transition** (revised 2026-07-11): hiding also disables the module, showing re-enables it once — thereafter, while visible, its enable toggle is free (the coupling fires only on this interactive toggle, not on load/reset)
**And** the panel lists modules grouped by zone in their current order; **dragging a module within its zone reorders it**, and **dragging it into another zone's section moves it there** (updates `zone` + `position`) — the list order maps directly to the on-screen placement order
**And** **reordering / moving a module does NOT change its state** — only `zone` + `position` change; `visible`, the enable, the type/colour tag, bindings and displays are untouched (a Reverb dragged into GENERATORS stays a Processor, still enabled/visible as before)
**And** the zone header row has a **tri-state bulk checkbox** (all / none / mixed) that turns the whole zone on or off; there is **no separate zone-visibility state** — a zone's header on the rack is **derived** (shown iff ≥1 module in it is visible), so emptying a zone makes its header disappear and re-enabling any module brings it back
**And** all mutations go **only through the `RackLayout` model** and re-run the single `layout()` path (no ad-hoc bounds; NFR1); MASTER BUS stays right-aligned, other zones left-aligned
**And** after any change the window **width stays fixed (1520 px)** and the **height auto-fits** the visible modules (AD-12)
**And** no audio/param/format change (NFR2, NFR3).

_Note: in 4.2 the customization is session-only (in-memory model). Making it survive save/load is Story 4.3 (persistence). Interim implementation note: a first pass shipped show/hide via a `PopupMenu` (commit `6d8610a`); this story replaces that popup with the reorderable panel._

### Story 4.3: Persist the custom layout to `.synthy` + reset layout

As a JASS player,
I want my customized rack layout (what's hidden, and later where things sit) saved in the preset and restorable to the default,
so that a layout I set up survives save/load and I can always get back to the stock arrangement.

**Acceptance Criteria:**

**Given** the layout model + show/hide (Stories 4.1–4.2), so there is real non-default state to store
**When** a preset (and the shared LiveState) is saved and reloaded
**Then** the layout (per-module `visible`, `zone`, `position`) round-trips as a **single append-only structured field** (`"RackLayout"`) in `.synthy` — **not** ~20×3 individual automatable APVTS params — with **no `kFormatVersion` bump** and no existing field renamed/reordered (NFR6)
**And** the same layout also round-trips in the plugin's DAW state (`getStateInformation`) via a non-parameter property on the APVTS ValueTree, so it survives with the editor closed
**And** a preset lacking the field — older build or the C# app — loads with the **built-in default layout** (missing ⇒ default), and the C# app still loads the preset unaffected
**And** a **"reset layout"** affordance restores the descriptor-default layout **without touching any audio parameter**
**And** the audio engine, parameter values and existing `.synthy` fields are otherwise untouched (NFR2, NFR3).

_(AD-11 precision 2026-07-10: layout persists as ONE structured `"RackLayout"` JSON field + a ValueTree property, not as dozens of standalone APVTS parameters — same append-only/interop guarantees, without polluting DAW automation.)_

### Story 4.4: ~~Drag & drop between zones + reorder within a zone~~ — FOLDED INTO 4.2

_Superseded 2026-07-11. Reordering and moving modules between zones is delivered by the **reorderable customization list in Story 4.2** instead of on-rack drag & drop (simpler, more robust, one place for show/hide + order + zone). This story is intentionally left empty; Epic 4 = Stories 4.1, 4.2, 4.3._

## Epic 5: Flexible Mix Routing

Generalize the MIX MODE coupling from the fixed OSC1↔OSC2 to two user-selectable operands. First sanctioned DSP change — surgical: only the OSC-mix block in `SynthVoice` + two append-only params + the MIX MODE descriptor. No other signal-chain change.

### Story 5.1: Selectable CROSS MOD sources (A/B among OSC 1/2/3)

As a JASS sound designer,
I want to choose which two oscillators the cross-mod (RingMod / FM) couples,
so that I can ring-mod / FM any pair (1-2, 1-3, 2-3), not only OSC1↔OSC2.

**Acceptance Criteria:**

**Given** the CROSS MOD module (renamed from MIX MODE)
**When** I set Source A and Source B (each OSC 1/2/3)
**Then** FM makes A modulate B (carrier) and RingMod computes A×B, with the remaining OSC summed plainly
**And** every oscillator still advances exactly once per sample (no pitch/phase drift)
**And** Source A and B are always kept **distinct** (picking the same one bumps the other to a free OSC)
**And** the MODE combo is **{RingMod, FM}** only — "no coupling" = the module **disabled** (enable toggle off ⇒ plain additive sum), matching the Filter/LFO pattern; default disabled ⇒ additive (identical to before)
**And** the new `mixSrcA`/`mixSrcB` params are append-only (defaults OSC1/OSC2); the `.synthy` keeps the `Additive/RingMod/FM` marker via `choiceOrOff`, so older presets round-trip
**And** the default patch is audibly identical to before (regression), RT rules respected (NFR2), build clean.

_(Option B + rename applied 2026-07-11: dropped "Additive" as a mode — the module-off IS additive — and renamed MIX MODE → CROSS MOD, since with Additive gone the module is purely oscillator cross-modulation. Internal id/param IDs kept stable.)_

## Epic 6: Module Discoverability

Give each module an optional short help description, revealed on demand, so a player can learn what a module does in place. Built entirely on the existing rack framework (`ModuleDescriptor` + `ModuleFrame`) and the `CallOutBox` mechanism already used for the customization panel. No audio, DSP, parameter-ID or `.synthy` change. Verification = clean build + the running app (no unit-test framework).

### Story 6.1: Per-module online help — info icon, movable panel, EN/DE

As a JASS player,
I want a circled-"i" info icon in each module's header that opens a movable description panel, in my chosen language,
so that I can learn what each module does without leaving the synth or reading a separate manual.

**Acceptance Criteria:**

**Given** a module whose descriptor carries help text
**When** JASS opens
**Then** the module header shows a small circled-"i" info icon (only for modules that have help text; a module without help shows no icon and is unchanged)
**And** clicking the info icon opens a help panel showing the module's title + description
**And** the panel is **movable** (drag by its title bar) and stays open until closed via its top-right "✕" or the ESC key (no hover/auto-dismiss)
**And** the JASS header has a **language selector combo box** offering **EN** and **DE**; the panel shows the description in the selected language, and an already-open panel updates when the language changes
**And** every currently-shipping module has an accurate 1–2 sentence description **in both EN and DE**
**And** the info icon, panel, and selector do not disturb existing interactions (knob drag, value-box edit, combos, enable toggle, reset ↺, customization panel), and header geometry stays uniform across modules
**And** it is UI-only: no parameter, APVTS, audio-thread, or `.synthy` change (NFR2, NFR3), repaint cost negligible (NFR5).

## Epic 7: UI Polish

Two steps: 7.1 refines the grid + renames the classes (enabler, visually neutral); 7.2 assigns the tighter sizes with the user's eye. No audio/DSP/param/`.synthy` change; verification = clean build + the running app.

### Story 7.1: Finer 24-column grid + column-based size-class names (foundation) — DONE

As a JASS developer,
I want the grid refined 12→24 columns and the size classes renamed to their footprint (`W{cols}H{rows}`),
so that small modules can be sized tightly and every size reads as explicit grid maths.

**Acceptance Criteria:**

**Given** the 12-column grid and T-shirt size names (XXS…XL)
**When** the grid is set to 24 columns and the classes renamed to `W2H1/W4H1/W6H1/W8H1/W8H2/W12H2` (old counts doubled)
**Then** the rack is **visually identical** (each module keeps its exact width/height), OSC stays 3-per-row
**And** the enum, `sizeClassSpec` table and every `SizeClass::…` call site use the new names (no old names remain)
**And** it is UI-only with zero persistence impact (size class is compile-time, not stored in `.synthy`).

_(Done 2026-07-12. See `7-1-grid-and-size-names.md`.)_

### Story 7.2: Tighten module size classes to fit control counts

As a JASS player,
I want each module sized to its actual controls (no oversized/over-wide modules),
so that the rack reads tight and balanced — as densely packed as the OSC modules.

**Acceptance Criteria:**

**Given** the 24-col grid + column-based names (Story 7.1) and OSC (`W8H1`, 3-per-row) as the density benchmark
**When** the user specifies each module's target width and it is applied (changing only the `SizeClass` arg; adding a new `W{cols}H{rows}` class as one table+enum case if the width is new)
**Then** loose modules are tightened toward the OSC density, subject to the guardrails below
**And** rotaries keep at least the AD-3 minimum diameter; combos keep enough width to read their items
**And** STEREO is tightened (its 2 knobs currently float in `W4H1`) and the MASTER title truncation (title + 3 header icons don't fit `W2H1` since Epic 6) is fixed by widening MASTER slightly or compacting the header
**And** single-control modules use the smallest class, MASTER BUS stays right-aligned / other zones left, uniform header anatomy kept, OSC stays `W8H1`
**And** it is UI-only (no param/APVTS/DSP/`.synthy`), verified per module in the running app: balanced, controls usable/readable, no truncation or grid-boundary breach.

_(Sizing is a judgement call confirmed by eye — the user drives the per-module target widths. See `7-2-module-size-tuning.md`.)_

## Epic 8: Modulation Matrix

Replace the single hard-wired source→target modulation with an accumulating `{source, target, amount}` matrix — the "movement layer" that unlocks macros, per-voice random, an evolution module, and additional LFOs. The refactor a 2nd LFO would force (accumulate per target, decouple source from target) IS the matrix, so it is built once. Second sanctioned DSP change; surgical (engine + slots + MOD MATRIX module + ring generalization + append-only persistence). Verification = clean build + the running app; the default patch (no active slots + the folded-in legacy LFO) must be audibly identical to before.

### Story 8.1: Modulation Matrix — accumulating source→target routing engine

As a JASS sound designer,
I want a modulation matrix where any source can route to any target with its own amount and multiple sources can stack on one target,
so that JASS gains the movement layer and macros / per-voice random / evolution / more LFOs become cheap follow-ons.

**Acceptance Criteria:**

**Given** the current single-target if/else apply (`SynthVoice.cpp:116-141,188-189`) and single-target mod rings
**When** the matrix engine is introduced
**Then** modulation is routed as `{source, target, amount}` slots whose bipolar amounts **sum per target** and are applied **once** around the captured base value, reusing today's exact per-target curves/clamps — and two slots on the same target (e.g. LFO 1 → Cutoff **and** Envelope → Cutoff) both audibly contribute (impossible today)
**And** source vocabulary v1 = {LFO 1, Envelope, Velocity}; target vocabulary v1 = the seven existing destinations; both append-only
**And** 4 fixed slots (decision 2026-07-14) are appended APVTS params (`modSlot{n}Source/Target/Amount` + `modMatrixOn`), persisted append-only to `.synthy` + DAW state, no `kFormatVersion` bump, missing ⇒ Off/on (old presets + C# unaffected); RANDOM leaves the matrix untouched in v1
**And** the existing LFO's TARGET/DEPTH keep working as an **implicit routing** on the same engine, so the default patch is audibly identical (zero regression)
**And** a MOD MATRIX rack module (MODULATION zone) lists the slots (Source combo · Target combo · bipolar Amount knob) with the uniform header + enable + info + reset, and the mod rings light **every** knob whose target currently receives (periodic-source) modulation, not just one
**And** it is RT-safe (no alloc/lock in the callback; fixed-size per-voice slot array), builds clean, and is verified by ear per the story's test list.

_See `_bmad-output/implementation-artifacts/8-1-modulation-matrix.md` for full dev context, the exact apply-block to replace, and the Open Design Questions (LFO-target keep-vs-fold, slot count, source set, RANDOM handling, module-vs-panel presentation)._

## Epic 10: Spatialization (per-generator panning & optional surround)

Take JASS from mono to spatial. Built stereo-first so it benefits every device/host, with true
surround as an opt-in that degrades gracefully to a stereo down-mix. First change to the output bus;
largest sanctioned DSP change so far → surgical and staged, default byte-identical. Verification =
clean build + running app + ear (no unit tests).

### Story 10.1: Per-generator stereo PAN (mono → 2-channel voice mix)

As a JASS sound designer,
I want a PAN control on each generator,
so that I can place OSC 1–3, SUB, NOISE, KARPLUS and WAVETABLE across the stereo field.

**Acceptance Criteria:**

**Given** the mono voice engine (generators summed to one mono sample in `SynthVoice`)
**When** each generator gains an append-only `<gen>Pan` param (default 0 = center) with a PAN knob in
its module body
**Then** each generator's contribution is equal-power panned into a 2-channel per-voice mix before the
effect chain, the default (all centered) is audibly identical to today's mono sum folded to L/R
(regression gate), pans round-trip append-only (missing ⇒ center, no `FormatVersion` bump), it is
RT-safe, and PAN is exposed for a later mod-matrix Pan target.

### Story 10.2: Optional surround output + per-generator channel assignment

As a JASS sound designer with a surround setup,
I want to output 4.0/4.1/5.1 and assign each generator to a channel (FL/FR/FM/RL/RR + LFE),
so that I can build a discrete multi-speaker patch.

**Acceptance Criteria:**

**Given** Story 10.1's channel-agnostic voice mix
**When** the user selects a surround mode AND the device/host grants a ≥4/≥6-channel output bus
**Then** each generator can be routed to a discrete channel (FL, FR, FM/center, RL, RR; SUB/LFE for
`.1`); when the surround bus is unavailable the engine down-mixes to stereo (no silent preset); the
effect-chain placement decision (dry-pan + shared FX bus, per §5 of the brief) is implemented;
stereo stays the default and stereo users see no regression; persistence stays append-only.

### Story 10.3: Binaural (Kunstkopf / HRTF) rendering over headphones

As a JASS player on ordinary headphones,
I want a binaural output mode,
so that the spatial/surround placement is heard in 3D without any surround hardware.

**Acceptance Criteria:**

**Given** the per-generator position data (Story 10.1/10.2) and a stereo output
**When** the user selects the Binaural output mode
**Then** each generator's position is rendered to binaural stereo — via a parametric model
(ITD fractional-delay + ILD + head-shadow low-pass; no external assets, recommended first) or HRIR
convolution (`juce::dsp::Convolution` with an embedded HRIR set; licence-checked) — giving a 3D image
on headphones; it is RT-safe; and it is one selectable mode among the others (default unchanged).

_See `_bmad-output/implementation-artifacts/10-1-spatialization-panning-surround.md` for the full
feasibility assessment ("does it make sense"), the staged Phase A/B plan, dev anchors (mono sum in
`SynthVoice`, stereo bus in `PluginProcessor.cpp:7-8`, pseudo-stereo `DSP/StereoWidth.h`), and the
Open Design Questions (stereo-first vs surround-first, discrete-channel vs continuous panner, where
the effects run, fate of the pseudo-stereo STEREO module, LFE handling, standalone device config)._
