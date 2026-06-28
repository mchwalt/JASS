# Deferred Work

## ToDo — C# Synthy must mirror the LFO/NOISE/FILTER/DISTORTION enable change (2026-06-28)

JASS (C++) split the bypass out of four choice params into their own enable toggle:
**LFO, NOISE, FILTER, DISTORTION** each got a dedicated `<x>On` bool, and `"Off"` was
removed from their comboboxes (`lfoTarget`→{Frequency,Amplitude,FilterCutoff},
`noiseType`→{White,Pink}, `filterType`→{Lowpass,Highpass},
`distortionType`→{SoftClip,HardClip,Foldback}).

**The shared `.synthy` format is UNCHANGED** (no version bump): `"Off"` is still written
to/read from `NoiseType`/`LfoTarget`/`FilterType`/`DistortionType` as the disabled marker
(JASS maps it to/from the new on-flag in `PresetIO::choiceOrOff` / `setChoiceOrOff`). So
existing presets and the current C# app keep loading correctly.

**C# action required:** mirror the *UI/model* change in the C# app — add an enable toggle
for LFO/Noise/Filter/Distortion and drop the `"Off"` entry from those combos, deriving the
toggle from (and serialising it back to) the `"Off"` string in the shared format. Pure
C#-side UI/model work; no format change needed. (Raised while prototyping the rack-UI
"everything is a module" pass — these four now have header enables like every other module.)

## Prototype decisions pending formalization — run correct-course (2026-06-28)

These were prototyped in the throwaway sample rack (built on top of Story 1.3) to see them
working; they revise the PRD/architecture and must be formalised before they leave prototype:

- **FR14 revised — Stereo + Master are now rack modules**, not header chrome. They live in a
  new **MASTER BUS** zone (top row of the main rack, right-aligned). The legacy header
  Stereo/Master controls were deleted. Header was flattened; "Current State" moved to the
  Save/Load cluster; title centred. → revises **FR14**, touches **Story 3.1** (chrome) and
  the legacy-deletion **Story 3.3**.
- **AD-2 size-class model replaced** — the grid was refined **8 → 12 columns** (a pure
  proportional raster, decoupled from knob size). Size classes are now **column spans**:
  XS=2×1, S=3×1, M=4×1, L=4×2 (ADSR), XL=6×2 (scope/spectrum). `ModuleFrame` body now derives
  its column count from CONTENT (`nCols = ceil(contentSlots / units)`) and fills the module
  width, with knobs centred — no longer `cols×3` (which coupled layout to the knob diameter).
  The old S/M/L (1×1 / 2×1 / 2×2, slotCapacity 3/6/12) is gone. → rewrites **AD-2**.
- **Enable split for LFO/NOISE/FILTER/DISTORTION** (audio) — see the C# ToDo above; also needs
  the PRD/feature list updated to reflect the per-module enable + trimmed combos.
- **Open for next session:** column-width / row-packing optimisation (trailing empty cells in
  partial rows; MASTER a touch airy as a 1-knob XS).

## Deferred from: code review of story 1-2-module-frame (2026-06-28)

- ✅ **RESOLVED in Story 1.3** — _resized() grid overflow for spanning cells_: `ModuleFrame::resized()` now derives the row count from the actually-placed cells (not `bodySlots`) and clamps placement to `nRows-1` so a spanning cell can no longer fall below the body. (The suspected double column-advance was verified NOT to be a bug.)
- **Unguarded attachment construction against a bad paramId** — `Slider/ComboBox/ButtonAttachment` + the enable attachment dereference `getParameter(id)` with no null check (unlike `doReset()`/`enableValue`). Matches existing convention; debug `jassert` catches typos. Add a graceful skip/validation when descriptors become authored data (Story 1.5).
- **Combo dynamic-provider edge cases** — an empty-returning provider leaves a bound but item-less ComboBox; the provider is polled once at build time and `Action/FileAction.refreshes` is never consumed, so dependent combos won't refresh after a load. Wire in Story 1.5.
- **AC5 in-app visual verification** — the "renders correctly in the running app (header + body + dim)" clause was not confirmed (no Rack yet). Verify during Story 1.3 integration.

## Deferred from: code review of story 1-1-module-descriptor-types (2026-06-28)

- **Over-capacity body is silent in release** — `assertFitsClass` is a debug-only `jassert` returning `void`; in release an over-capacity descriptor has no signal. Handle gracefully where layout consumes capacity (Rack, Story 1.3).
- **`sizeClassSpec` release fallback** — for an unhandled future `SizeClass` enumerator, release silently returns the S-class spec `{1,1,3}`. Revisit when adding the anticipated 4th class (`W`, wide-display).
- **Descriptor copy/ownership policy** — `ModuleDescriptor`/`BodyElement` are freely copyable and hold `std::function` closures + a non-owning `Display.component*`; copying risks dangling/aliasing once descriptors are stored. Decide copy-vs-move semantics and null/lifetime checks in Stories 1.2/1.3.
- **`Combo.items` empty default** — `std::variant` default-constructs to an empty `StringArray`; a combo with unset items renders empty. Add a debug check when wiring combos (Story 1.5).
- **Default-constructed `ModuleDescriptor{}`** — a valid-but-empty descriptor is indistinguishable from a deliberate empty always-on module. Revisit only if descriptors are ever default-constructed in a container.
- **No referential cross-check** of `Knob.modTarget` / `enableParam` against the body or APVTS — mismatches surface later as a dead modulation ring or non-functional enable toggle. Add an optional debug validator in Story 1.2/1.4.
