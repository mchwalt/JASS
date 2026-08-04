# Deferred Work

## ✅ RESOLVED 2026-07-06 — C# Synthy mirrored the new module enables MasterOn/AdsrOn/MixModeOn (Story 2.4)

JASS (C++) gave the three formerly always-on modules real enable bools: **`masterOn`, `adsrOn`,
`mixModeOn`** (APVTS, default true), serialised to the shared `.synthy` as `MasterOn`/`AdsrOn`/`MixModeOn`.

**Done in the C# Synthy** (`D:\Projects\C#\Synthesizer\Synthy\`, separate local git repo): added the three
`bool` fields (default true) to the `Preset` class — System.Text.Json keeps the initializer for a
missing field ⇒ reads as ON (back-compat, no FormatVersion bump); added `MasterOn`/`AdsrOn`/`MixModeOn`
to `SynthEngine` with the semantics (Master off = mute, ADSR off = bypass envelope w/ constant gain,
Mix-Mode off = additive) + VM properties + BuildPreset/ApplyPreset round-trip + three UI checkboxes
(Master header, ADSR header, MIX header). Full parity incl. audio. Build clean.


## ✅ RESOLVED 2026-07-05 — OSC WAVE combo item order mismatched the `oscWave` param

The rack shared `const juce::StringArray waves { "Saw", "Square", "Sine", "Triangle" }` for the
OSC WAVE combos, but `oscWave`'s param choices are `{ "Sine", "Sawtooth", "Square", "Triangle" }`
(`Parameters.h:148`). Because a `ComboBoxAttachment` maps by **index**, 3 of 4 OSC WAVE labels
were wrong (selecting "Saw" actually produced a Sine, etc.) — the same class of bug as the LFO
WAVE defect fixed in Story 2.1.

**Fix:** the `waves` array (now OSC-only after 2.1 inlined the LFO list) was re-ordered to match
`oscWave` = `{ "Sine", "Sawtooth", "Square", "Triangle" }`, with a comment. Pure UI/descriptor
change; no param/format impact. (Ideal long-term: drive every choice-combo's item list from the
param's own `getAllValueStrings()` so a combo can never drift — noted, not built.)

## ✅ RESOLVED 2026-07-06 — C# Synthy mirrored the LFO/NOISE/FILTER/DISTORTION enable-split

JASS (C++) split the bypass out of four choice params into their own enable toggle
(LFO/NOISE/FILTER/DISTORTION), dropping `"Off"` from the combos; the shared `.synthy` stays
unchanged (`"Off"` remains the on-disk disabled marker).

**Done in the C# Synthy** (separate local git repo): kept `Off` in the enums (it is the format
value + the DSP's off-check) but dropped it from the four ViewModel combo item-sources; added
`LfoEnabled`/`NoiseEnabled`/`FilterEnabled`/`DistortionEnabled` bools whose setters drive the
engine enum = `Enabled ? selectedValue : Off`; combo props now default to the first real value.
Serialisation mirrors C++ `choiceOrOff`/`setChoiceOrOff`: BuildPreset writes `Off` when disabled,
ApplyPreset sets `Enabled = value != Off` and only assigns the combo when non-Off (preserves the
last real choice). Four UI checkboxes added (Filter/Distortion/LFO headers + Noise header). No
`.synthy`/format change. Build clean.

## ✅ FORMALIZED 2026-07-01 — Prototype decisions (was: pending correct-course)

Formalized via `sprint-change-proposal-2026-07-01.md` (approved). PRD, Architecture Spine and
Epics were updated to match the built code (edits A–N). The three decisions below are now
ADOPTED spec, not prototype. **The C# Synthy enable-mirror ToDo (top of this file) stays open.**

These were prototyped in the throwaway sample rack (built on top of Story 1.3) to see them
working; they revised the PRD/architecture and are now formalised:

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

## Deferred from: code review of stories 12-2 + 12-3 (2026-08-04)

- SAMPLER: synchronous message-thread import can freeze the UI for a few seconds on a full
  300 s multisample set (and startup preload grew accordingly). Bounded by the caps; a proper
  fix is async loading with progress — its own story if it ever bites.
- SAMPLER STRETCH: toggling STRETCH ON mid-note inserts ~60 ms of warm-up silence on all held
  voices (a per-voice outputSeek on toggle would burst 16 × 0.57 ms in one callback — worse).
  Rare edit action; revisit only if users toggle stretch as a performance gesture.
- SAMPLER/SplendidGrand: metallic clicking on SOME keys — user assessment: in the source
  samples themselves (only certain keys). offset= is honoured since 2026-08-04; if it persists,
  inspect the affected FLACs before suspecting playback. (Tracked in story 12.4 notes.)
