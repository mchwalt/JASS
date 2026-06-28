# Deferred Work

## Deferred from: code review of story 1-2-module-frame (2026-06-28)

- **resized() grid overflow for spanning cells** — a `Display` with `slots>1` can force a wrap that pushes a later row below the body's bottom edge; `row` is never clamped to `nRows`, and `nRows` is computed from `bodySlots(desc.body)` (which counts a skipped null-`Display`), overcounting rows. The body-grid is provisional per dev notes; resolve with exact `Wc`/`Hu` tuning in Story 1.3. (The suspected double column-advance was verified NOT to be a bug.)
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
