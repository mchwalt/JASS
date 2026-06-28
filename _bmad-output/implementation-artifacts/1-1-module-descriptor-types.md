---
baseline_commit: 11ff0bfd5b724c10d360a809ec8c42a17debb8a0
---

# Story 1.1: Module descriptor & control-vocabulary types

Status: done

<!-- Note: Validation is optional. Run validate-create-story for quality check before dev-story. -->

## Story

As a JASS developer,
I want a single declarative `ModuleDescriptor` data model with the full control vocabulary and a data-driven size-class table,
so that every module can later be expressed as data instead of bespoke layout code (the root fix for the "cobbled-together" UI).

## Acceptance Criteria

1. **Given** a new header `Source/UI/rack/ModuleDescriptor.h`, **When** the project builds (C++20, MSVC, JUCE warning flags on), **Then** it compiles cleanly with no new warnings and introduces **no `.cpp`** (pure type/`inline` definitions — header-only, so no `CMakeLists.txt` change in this story).
2. **Given** the descriptor model, **Then** `ModuleDescriptor { SizeClass sizeClass; juce::String title; ModuleType type; juce::String enableParam; std::vector<juce::String> resetParams; std::vector<BodyElement> body; }` exists, where an empty `enableParam` means "always-on" (Master, ADSR, Mix-Mode).
3. **Given** the body vocabulary, **Then** `BodyElement` is a `std::variant` over exactly: `Knob`, `Combo`, `Toggle`, `Action`, `FileAction`, `Label`, `Display` — with fields matching AD-4 (see Dev Notes for the exact shape, including `Knob.displayTransform` as a guarded pair and `Knob.modTarget`, `Combo.items` as static-or-dynamic, `Action/FileAction.refreshes`).
4. **Given** the size-class table, **Then** a single source maps each `SizeClass` to `{ cols, units, slotCapacity, knobDiameter }` with **S = 1×1 / 3 slots**, **M = 2×1 / 6 slots**, **L = 2×2 / 12 slots**, and adding a 4th class is a single new table entry (no other code path). `knobDiameter` is currently the **same uniform value** for all classes (AD-3 provisional).
5. **Given** a descriptor, **Then** a helper `bodySlots(body)` returns the consumed slot count (1 per control element; `Display.slots` for a display) and a helper asserts (`jassert`) `bodySlots(body) ≤ slotCapacity(sizeClass)` so an overflowing descriptor (e.g. Wavetable in M) trips in debug.
6. **Given** the project's constraints, **Then** the header introduces **no audio-thread code, no parameter definitions, and no APVTS ownership** — it is pure UI-layer data describing modules; parameter IDs are stored as `juce::String` (filled later from `Parameters::ID`, never hardcoded literals).

## Tasks / Subtasks

- [x] **Task 1: Create the rack framework folder + header** (AC: 1)
  - [x] Create `Source/UI/rack/` and `Source/UI/rack/ModuleDescriptor.h`
  - [x] `#pragma once` + `#include <JuceHeader.h>`, `<variant>`, `<vector>`, `<functional>`; put everything in `namespace rack`
  - [x] Confirm header-only: no `.cpp`, so `CMakeLists.txt` `target_sources` is unchanged this story (note for later stories that add `.cpp`)
- [x] **Task 2: Define the enums** (AC: 2, 3, 4)
  - [x] `enum class SizeClass { S, M, L };` · `enum class ModuleType { Generator, Modulator, Processor };` · `enum class ModTarget { None, Frequency, Amplitude, FilterCutoff };`
- [x] **Task 3: Define the BodyElement vocabulary** (AC: 3)
  - [x] Define `Knob`, `Combo`, `Toggle`, `Action`, `FileAction`, `Label`, `Display` structs per the shape in Dev Notes
  - [x] `using BodyElement = std::variant<Knob, Combo, Toggle, Action, FileAction, Label, Display>;`
- [x] **Task 4: Define ModuleDescriptor** (AC: 2)
- [x] **Task 5: Size-class table + slot helpers** (AC: 4, 5)
  - [x] `struct SizeClassSpec { int cols; int units; int slotCapacity; int knobDiameter; };`
  - [x] `inline SizeClassSpec sizeClassSpec(SizeClass)` returning S/M/L specs (knobDiameter = `KnobSize::Small` for all — AD-3)
  - [x] `inline int bodySlots(const std::vector<BodyElement>&)` (sum 1 per control, `Display.slots` for a display — use `std::visit`)
  - [x] `inline void assertFitsClass(const ModuleDescriptor&)` → `jassert(bodySlots(d.body) <= sizeClassSpec(d.sizeClass).slotCapacity);`
- [x] **Task 6: Verify** (AC: 1, 5, 6)
  - [x] Build the JASS target; confirm clean compile, no new warnings
  - [x] Add a temporary local example descriptor in a function (or `static_assert`/debug block) to confirm the types instantiate and `assertFitsClass` fires for an over-capacity body, then remove it

### Review Findings

_Code review 2026-06-28 (Blind Hunter + Edge Case Hunter + Acceptance Auditor). 0 decision-needed, 4 patch, 6 deferred, 6 dismissed._

- [x] [Review][Patch] `Display.slots` defaults to 0 → zero-slot display silently passes `assertFitsClass` (under-count) [Source/UI/rack/ModuleDescriptor.h:74,120] — a `Display{component}` with slots unset counts as 0; guard it (debug-assert a Display claims ≥1 slot).
- [x] [Review][Patch] `Knob` display-transform pair has no all-or-nothing invariant [Source/UI/rack/ModuleDescriptor.h:31-32] — `toDisplay` set with `fromDisplay` empty (or vice versa) is constructible; on FREQ edit Story 1.4 would hit an empty `std::function` (UB) or write the displayed value as base. Document the contract: a partial pair is treated as identity (both must be set to be active).
- [x] [Review][Patch] `SizeClassSpec` members are uninitialized [Source/UI/rack/ModuleDescriptor.h:95-101] — a default-constructed `SizeClassSpec` holds indeterminate ints. Add `= 0` member initializers.
- [x] [Review][Patch] ARCHITECTURE-SPINE AD-4 stale vs shipped code [ARCHITECTURE-SPINE.md AD-4] — code renamed the element `Label{caption}` → `Caption{text}` (justified: `juce::Label` clash). Update AD-4 wording to match so the binding architecture doc isn't contradicted.
- [x] [Review][Defer] Over-capacity body is silent in release [ModuleDescriptor.h:135] — `assertFitsClass` is debug-only/void; no release signal. Belongs to Rack (Story 1.3) where layout consumes capacity — handle gracefully there.
- [x] [Review][Defer] `sizeClassSpec` release fallback returns S-spec for an unhandled future enum [ModuleDescriptor.h:111-112] — when the 4th class (`W`) is added, a forgotten case silently sizes as S in release. Revisit when adding the 4th class.
- [x] [Review][Defer] Descriptor copy/ownership policy: copyable `std::function` closures + non-owning `Display.component*` [ModuleDescriptor.h:71-77] — copying a descriptor duplicates captures/pointers; dangling/aliasing risk once Rack stores descriptors. Decide copy-vs-move + null/lifetime checks in Stories 1.2/1.3.
- [x] [Review][Defer] `Combo.items` default-constructs to an empty `StringArray` [ModuleDescriptor.h:43] — a combo with no items set renders empty. Builders (Story 1.5) always set items; add a debug check when wiring combos.
- [x] [Review][Defer] Default-constructed `ModuleDescriptor{}` is valid but semantically empty [ModuleDescriptor.h:81] — an uninitialized descriptor is indistinguishable from a deliberate empty always-on module. Builders populate fully; revisit if descriptors are ever default-constructed in a container.
- [x] [Review][Defer] No referential cross-check of `modTarget`/`enableParam` against the body/APVTS [ModuleDescriptor.h:33,86] — a dead mod-ring or non-functional toggle only surfaces later. Add an optional debug validator in Story 1.2/1.4.

## Dev Notes

### What this story is (and is NOT)
- **IS:** the pure data model + size-class table + slot assertion — the seam every later story builds on. No rendering, no rack, no editor wiring.
- **IS NOT:** `ModuleFrame` rendering (Story 1.2), the `Rack` grid engine (1.3), modulation-ring/display-transform *wiring* (1.4), or any module migration (1.5). Define the *fields* for those here; do not implement their behavior.

### Exact type shape (target this — derived from Architecture AD-4)
```cpp
namespace rack {

enum class SizeClass  { S, M, L };
enum class ModuleType { Generator, Modulator, Processor };
enum class ModTarget  { None, Frequency, Amplitude, FilterCutoff };

struct Knob {
    juce::String paramId;
    juce::String label;
    // displayTransform = guarded PAIR; empty => identity. ratio<=0 must mean
    // identity + no write-back (Story 1.4 enforces; here just hold the fns).
    std::function<double(double base,  double ratio)> toDisplay;   // shown value
    std::function<double(double shown, double ratio)> fromDisplay; // base to store
    ModTarget modTarget = ModTarget::None;
};
struct Combo {
    juce::String paramId;
    juce::String label;
    // static items OR a provider polled at refresh time (Story 1.5 wavetable bank)
    std::variant<juce::StringArray, std::function<juce::StringArray()>> items;
};
struct Toggle     { juce::String paramId; juce::String label; };
struct Action     { juce::String label; std::function<void()> onClick;
                    std::vector<juce::String> refreshes; };          // combo paramIds to re-poll
struct FileAction { juce::String label; std::function<void(juce::File)> onChoose;
                    std::vector<juce::String> refreshes; };
struct Label      { juce::String caption; };
struct Display    { juce::Component* component = nullptr; int slots = 0; }; // NON-owning; editor owns lifetime

using BodyElement = std::variant<Knob, Combo, Toggle, Action, FileAction, Label, Display>;

struct ModuleDescriptor {
    SizeClass    sizeClass {};
    juce::String title;
    ModuleType   type {};
    juce::String enableParam;             // empty => always-on (no toggle)
    std::vector<juce::String> resetParams;
    std::vector<BodyElement>  body;
};

struct SizeClassSpec { int cols; int units; int slotCapacity; int knobDiameter; };

inline SizeClassSpec sizeClassSpec(SizeClass c) {
    switch (c) {
        case SizeClass::S: return { 1, 1, 3,  KnobSize::Small };
        case SizeClass::M: return { 2, 1, 6,  KnobSize::Small }; // uniform knob (AD-3)
        case SizeClass::L: return { 2, 2, 12, KnobSize::Small };
    }
    return { 1, 1, 3, KnobSize::Small };
}
// bodySlots via std::visit: 1 for each control kind, d.slots for Display.
// assertFitsClass: jassert(bodySlots(body) <= sizeClassSpec(sizeClass).slotCapacity).

} // namespace rack
```

### Must reuse — do NOT reinvent
- **`KnobSize`** already exists in `Source/UI/SynthySlider.h` (`Large=74, Medium=56, Small=46`; comment says all modules currently use `Small`). Include `SynthySlider.h` and reference `KnobSize::Small` for `knobDiameter` — do not invent new size constants. [Source: Source/UI/SynthySlider.h lines 7–12]
- **`SynthySlider`** is the standard knob (rotary, right-click value entry, shift-fine, wheel-step, `setModAmount` ring via `--modAmount`). Story 1.2/1.4 will use it; this story only references `KnobSize`. [Source: Source/UI/SynthySlider.h]
- **Parameter IDs** come only from `Parameters::ID` in `Source/Audio/Parameters.h` (`ID::oscFreq(i)` etc.). The descriptor stores them as `juce::String`; builders (Story 1.5) pass `ID::xxx`. NEVER hardcode an ID string. [Source: Source/Audio/Parameters.h; project-context.md "APVTS is the single source of truth"]
- **`displayTransform` semantics** mirror today's `OscillatorPanel::setPlayedRatio`: FREQ display = base × playedRatio; turning the knob writes display ÷ ratio back as the new base. The guard (ratio ≤ 0 ⇒ identity, no write-back) prevents divide-by-zero / base corruption. [Source: Source/UI/PluginEditor.h lines 23–26; ARCHITECTURE-SPINE.md AD-4]
- **Display components** to be wrapped later are existing `juce::Component`s: `WaveformDisplay`, `SpectrumDisplay` (`Source/UI/`), `EnvelopeDisplay` (`Source/UI/PluginEditor.h`). `Display.component` is a **non-owning** raw pointer — the editor keeps owning these; the descriptor only references them. [Source: ARCHITECTURE-SPINE.md AD-5]

### Hard constraints (project-context.md — binding, do not violate)
- **No audio-thread / DSP / parameter code here.** This is UI-layer data only; it must not touch `processBlock`, allocate on the audio thread, or define parameters. [project-context.md: Real-Time rules; NFR2]
- **No parameter-ID, APVTS, or `.synthy` change.** Pure additive header. [NFR3]
- **Naming dualism stays:** new framework lives under `Source/UI/rack/`; keep the `Synthy*` prefix on existing classes (`SynthySlider`, `SynthyLookAndFeel`). Do not rename anything. New rack types use the `rack::` namespace (no prefix needed). [project-context.md: Naming dualism]
- C++20 is available (`std::variant`, `std::visit`) — `CMAKE_CXX_STANDARD 20`. [CMakeLists.txt]

### Project Structure Notes
- New directory `Source/UI/rack/` is introduced here per the Architecture Structural Seed. This story adds only `ModuleDescriptor.h` (header-only) — `ModuleFrame.*`, `Rack.*`, and the moved `SynthyLookAndFeel.*` arrive in Stories 1.2–1.3 and WILL require `target_sources` updates then.
- No conflict with existing structure; `Source/UI/` already holds UI components.

### Testing
- The project has **no unit-test framework** (no test target in `CMakeLists.txt`). Verification for this story = (a) the JASS target builds clean with no new warnings, and (b) a throwaway example descriptor confirms the types instantiate and `assertFitsClass` `jassert`-fires on an over-capacity body (then remove the example). Do not add a test framework in this story.
- Build via MSBuild from PowerShell; `cmake.exe` is not on PATH (full VS path in project memory). [project-context.md: Build & Workflow]

### References
- [Source: _bmad-output/planning-artifacts/epics.md#Story-1.1]
- [Source: _bmad-output/planning-artifacts/architecture/architecture-JASS-2026-06-28/ARCHITECTURE-SPINE.md#AD-1, #AD-2, #AD-4]
- [Source: Source/UI/SynthySlider.h#KnobSize]
- [Source: Source/UI/PluginEditor.h#OscillatorPanel.setPlayedRatio]
- [Source: Source/Audio/Parameters.h#Parameters::ID]
- [Source: _bmad-output/project-context.md]

## Dev Agent Record

### Agent Model Used

claude-opus-4-8[1m] (Claude Opus 4.8, 1M context)

### Debug Log References

- Verification build: `MSBuild build\JASS_Standalone.vcxproj /p:Configuration=Release` — clean (exit 0), produced `JASS.exe`. Header was temporarily `#include`d into `PluginProcessor.cpp` with an exercise function to force full instantiation, then removed; final build with the temp code removed is also clean (exit 0).

### Completion Notes List

- Implemented `Source/UI/rack/ModuleDescriptor.h` (header-only, `namespace rack`): `SizeClass`/`ModuleType`/`ModTarget` enums; `Knob`/`Combo`/`Toggle`/`Action`/`FileAction`/`Caption`/`Display` structs; `BodyElement` `std::variant`; `ModuleDescriptor`; `SizeClassSpec` + `sizeClassSpec()` table (S=1×1/3, M=2×1/6, L=2×2/12, uniform `KnobSize::Small`); `bodySlots()` + `assertFitsClass()`.
- **Deliberate deviation from AD-4 wording:** the static-text element is named **`Caption`** (field `text`), not `Label`. Rationale: `rack::Label` collides with the ubiquitous `juce::Label` whenever a consumer does `using namespace rack;` alongside JUCE (confirmed: it broke the verification build). `Caption` also describes the element's purpose (e.g. Mix-Mode "OSC 1 ↔ 2") more precisely. **Follow-up (not in this story):** update ARCHITECTURE-SPINE AD-4 to read `Caption{text}` instead of `Label{caption}` for consistency.
- Reused `KnobSize::Small` from `Source/UI/SynthySlider.h` (no new size constants); `displayTransform` carried as a guarded `{toDisplay, fromDisplay}` pair (the ratio≤0 guard itself is wired in Story 1.4); `Display.component` is a non-owning pointer (editor owns lifetime).
- No CMake change (header-only, AC1). No `.cpp`, no audio-thread/parameter/APVTS code (AC6). No project test framework exists — verification was the compile/instantiate/assert build per the story's Testing note (no framework added).
- `Source/PluginProcessor.cpp` is byte-identical to baseline (`git diff` empty) after removing the temporary compile-check.

### File List

- **NEW:** `Source/UI/rack/ModuleDescriptor.h`

### Change Log

- 2026-06-28 — Story 1.1 implemented: added the data-driven module descriptor + control vocabulary + size-class table (`Source/UI/rack/ModuleDescriptor.h`). Renamed AD-4 "Label" element to `Caption` to avoid `juce::Label` collision. Verified by clean Release build. Status → review.
- 2026-06-28 — Code review (3 adversarial layers): 4 patches applied — Display ≥1-slot debug guard; documented all-or-nothing display-transform contract; `SizeClassSpec` member initializers; ARCHITECTURE-SPINE AD-4 wording aligned to `Caption`. 6 findings deferred to Stories 1.2/1.3/1.4 (see Review Findings + `deferred-work.md`), 6 dismissed as noise. Re-verified clean Release build + clean spine lint. Status → done.
