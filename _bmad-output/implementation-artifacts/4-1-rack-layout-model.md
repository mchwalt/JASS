# Story 4.1: Ordered RackLayout model & descriptor-declared default zone/order

Status: review

<!-- Note: Validation is optional. Run validate-create-story for quality check before dev-story. -->

## Story

As a JASS developer,
I want the rack to render from one ordered `RackLayout` data model (with each module's default zone and order declared on its descriptor),
so that later customization features (show/hide, drag & drop, persistence) all mutate a single authoritative model instead of relying on implicit insertion order.

## Acceptance Criteria

1. **Default zone/order on the descriptor.** Each `ModuleDescriptor` declares its **default zone** (and default within-zone order), keyed by its existing stable `id`. The zone is no longer passed extrinsically as a bare argument the model can't see. `typeTag`/`type` stays identity/colour only and is unchanged.
2. **Single ordered model drives placement.** The `Rack` builds an ordered `RackLayout` model — a list of `{ id, zone, position, visible }` — from the descriptor defaults and **renders exclusively from that model**. Nothing reads raw insertion order for placement anymore; the packing path (`layout()`) walks the model.
3. **`layout()` stays the single packing path (AD-2 / NFR1).** No component computes its own bounds; placement still flows model → `layout()`. `preferredHeight()`, `resized()`, `moduleById()`, `updateLiveFeed()` behave exactly as before.
4. **Pure internal refactor — visually identical.** With the default model (every module `visible = true`, default zone/order = today's), the running app is **pixel-identical** to before this story: same modules, same zones, same order, same positions, same window height.
5. **No audio / param / format impact (NFR2, NFR3).** No parameter ID, APVTS entry, `.synthy` field, or audio behaviour is touched. `visible` exists in the model but is always `true` in 4.1 (no persistence and no toggle UI yet — those are Stories 4.2 / 4.3).
6. **Clean build.** The project builds cleanly (Standalone) with the existing JUCE warning flags; any new `.cpp` is added to `target_sources` in `CMakeLists.txt` (none expected — this is header + `Rack.cpp` work).

## Tasks / Subtasks

- [ ] **Task 1 — Introduce a shared `Zone` enum reachable from the descriptor (AC: 1)**
  - [ ] Move `enum class Zone { Generators, Modulation, Processing, MasterBus }` **out of `Rack`** into `ModuleDescriptor.h` (in `namespace rack`), because `Rack.h` includes `ModuleDescriptor.h` — the descriptor cannot reference `Rack::Zone` (include-cycle). 
  - [ ] In `Rack.h` add `using Zone = rack::Zone;` (or reference `rack::Zone`) so existing `Rack::Zone::…` call-sites keep compiling unchanged.
  - [ ] Verify `zoneTag()` / `zoneText()` in `Rack.cpp` still switch over the same enumerators.
- [ ] **Task 2 — Declare default zone + order on `ModuleDescriptor` (AC: 1)**
  - [ ] Add `Zone defaultZone {};` (and, if you choose explicit ordering, `int defaultOrder = 0;`) to `ModuleDescriptor`. Keep it append-only; existing fields untouched.
  - [ ] In `PluginEditor.cpp buildSampleRack()`'s `add` lambda, set `d.defaultZone = zone;` before constructing the frame (the lambda already receives `zone`).
- [ ] **Task 3 — Build the ordered `RackLayout` model in the Rack (AC: 2)**
  - [ ] Define `struct RackLayoutEntry { juce::String id; Zone zone; int position; bool visible = true; };` and a `std::vector<RackLayoutEntry> layoutModel;` member.
  - [ ] Change `addModule` to read the zone from the descriptor (`addModule(ModuleDescriptor)`), appending a `RackLayoutEntry` with `zone = desc.defaultZone`, `position` = running per-zone counter (preserves today's order), `visible = true`. Keep an id→footprint/frame lookup (extend or replace the existing `placed` struct; retain `cols`/`units`).
  - [ ] Update the single call-site pattern in `buildSampleRack` (`add` lambda → `addModule(std::move(d))`).
- [ ] **Task 4 — Render `layout()` from the model, not insertion order (AC: 2, 3, 4)**
  - [ ] In `layout()`, for each `zone` in `zoneOrder`, iterate the `layoutModel` entries whose `zone` matches **and** `visible == true`, **ordered by `position`**, resolve each entry's frame by `id`, and pack it with the existing row-major first-fit logic (unchanged geometry math).
  - [ ] Confirm the MASTER BUS right-align shift and the zone-header bands are unchanged.
  - [ ] Ensure `preferredHeight()` (calls `layout(w, false)`) and `resized()` (`layout(w, true)`) still work through the model.
- [ ] **Task 5 — Verify (AC: 4, 5, 6)**
  - [ ] Incremental Release/Standalone build via `build/JASS_Standalone.vcxproj` (MSBuild, PowerShell). Build must be clean.
  - [ ] Launch the app; confirm the rack is **visually identical** (all zones, modules, order, positions, window height) — the user verifies on the running app (screenshot into scratchpad if useful).
  - [ ] Confirm no `.synthy`/param change: load an existing preset, values unchanged; save produces the same field set (no new keys in 4.1).

## Dev Notes

### What this story is (and is not)
This is the **foundation** of Epic 4 and a **pure internal refactor**: it introduces the data model and moves the default zone onto the descriptor, but adds **no user-visible feature**. Show/hide is Story 4.3, drag & drop is 4.4, persistence is 4.2. The single success bar here: the app looks and behaves exactly as before, and placement now flows through `RackLayout`. This mirrors the successful "prove the foundation before layering behaviour" pattern from Story 3.3 (the clean-first-build that proved the legacy code was unreferenced).

### Current state of the files being modified (READ before editing)
- **`Source/UI/rack/Rack.h`** — declares `enum class Zone { Generators, Modulation, Processing, MasterBus }` **inside** `Rack`; `addModule(Zone zone, ModuleDescriptor desc)`; private `struct Placed { ModuleFrame* frame; Zone zone; int cols, units; }` and `std::vector<Placed> placed;`. Grid constants: `kDefaultCols=12`, `kGutter=10`, `kHu=114`, `kZoneHeaderH=28`, `kPad=8`.
- **`Source/UI/rack/Rack.cpp`** — `addModule` pushes `{ f, zone, spec.cols, spec.units }` into `placed`. `layout(width, apply)` iterates `zoneOrder`; **within each zone it iterates `placed` in vector order** (= insertion order) filtering `p.zone != zone`, and packs row-major first-fit. `preferredHeight` calls `layout(w,false)`; `resized` calls `layout(w,true)`. `moduleById` iterates `frames`. `zoneTag`/`zoneText` (anon namespace) map the enum.
- **`Source/UI/rack/ModuleDescriptor.h`** — `struct ModuleDescriptor { SizeClass sizeClass; juce::String id; juce::String title; ModuleType type; juce::String enableParam; std::function<bool()> enabledWhen; std::vector<BodyElement> body; … }`. `id` already exists and is documented as "future layout key for show/hide + drag-drop". `ModuleType { Generator, Modulator, Processor }` is the colour/identity tag — **distinct** from `Zone`.
- **`Source/UI/rack/ModuleFrame.h`** — owns the descriptor `desc`; exposes `const juce::String& moduleId() const { return desc.id; }` (used by `Rack::moduleById`). No change needed here.
- **`Source/UI/PluginEditor.cpp` `buildSampleRack()`** — the ONLY caller of `addModule`. An `add` lambda builds each `ModuleDescriptor` (derives `d.id` from the title via `retainCharacters`) and calls `sampleRack->addModule(zone, std::move(d))`. ~20 `add(...)` calls across MasterBus / Generators / Modulation / Processing. (Naming note: `sampleRack`/`buildSampleRack` are 1.3-scaffold names — cosmetic debt, do **not** rename in this story.)

### Key implementation guardrails
- **Include-cycle is the main trap.** `Rack.h` `#include "ModuleDescriptor.h"`. Therefore `Zone` must be defined where the descriptor can see it (in `ModuleDescriptor.h`, `namespace rack`). Add `using Zone = rack::Zone;` in `Rack` to keep every `Rack::Zone::…` reference (both in `Rack.cpp` and `PluginEditor.cpp`) compiling with no edit. This keeps the diff small and regression-safe.
- **Regression is proven by construction.** If `layoutModel` entries are appended in the same order `add(...)` is called today, and `layout()` walks them per-zone in `position` order, the first-fit packing sees the same module sequence per zone → identical geometry. Do not change any pixel math in `layout()` (gutters, `wc`, `kHu`, MASTER BUS right-align).
- **`visible` is plumbing only in 4.1.** Always `true`. Filtering `visible == false` should already be wired in `layout()` so 4.3 needs no `layout()` change — but there is no way to set it false yet (no persistence, no UI).
- **Do not touch** `Parameters.h`, `PresetIO`, the audio engine, or `.synthy`. AD-11 persistence is Story 4.2.
- **`placed` vs `layoutModel`.** Simplest safe shape: keep an id-keyed lookup for footprint+frame (`cols`, `units`, `ModuleFrame*`) and let `layoutModel` be the ordered placement authority. Either refactor `placed` to be keyed by id, or keep `placed` as the footprint store and add `layoutModel` as the ordering/visibility layer that `layout()` consults. Pick the one that keeps `layout()` cleanest.

### Testing standards
No unit-test framework in this project. Verification = **clean incremental build + the running app** (the user inspects it themselves; see the UI-verification habit). The decisive check for this refactor is visual identity to the pre-change rack plus an unchanged preset round-trip.

### Project Structure Notes
- All work stays under `Source/UI/rack/` (+ the one call-site block in `Source/UI/PluginEditor.cpp`). No new files expected → no `CMakeLists.txt` change; if you do split a file, add it to `target_sources`.
- Naming aligns with the framework convention (`Rack`, `ModuleFrame`, `ModuleDescriptor`). The `Zone` move is the only structural naming change; `Rack::Zone` remains a valid spelling via the alias.

### References
- [Source: _bmad-output/planning-artifacts/epics.md#Story 4.1] — story + ACs.
- [Source: _bmad-output/planning-artifacts/architecture/architecture-JASS-2026-06-28/ARCHITECTURE-SPINE.md#AD-10] — ordered RackLayout model as single source of truth; default zone/order on descriptor; `layout()` stays single packing path; `typeTag` never changes on move.
- [Source: prds/prd-JASS-2026-06-28/prd.md#FR20, FR15] — stable id + declared default zone; hide-is-UI-only (future).
- [Source: Source/UI/rack/Rack.cpp] — `addModule`, `layout()`, `preferredHeight`, `moduleById` (current placement path).
- [Source: Source/UI/rack/ModuleDescriptor.h] — descriptor fields; `id`; `ModuleType` vs `Zone` distinction.
- [Source: Source/UI/PluginEditor.cpp#buildSampleRack] — the sole `addModule` call-site.

## Dev Agent Record

### Agent Model Used

claude-opus-4-8[1m]

### Debug Log References

- Incremental Release build via `build/JASS_Standalone.vcxproj` (MSBuild x64) — clean, `JASS.exe` produced; `PluginEditor.cpp`, `ModuleFrame.cpp`, `Rack.cpp` recompiled with no errors/warnings.

### Completion Notes List

- **Zone enum moved** from inside `Rack` to `ModuleDescriptor.h` (`namespace rack`) to break the include-cycle; `Rack` keeps `using Zone = rack::Zone;` so all ~35 `Rack::Zone::…` call-sites (in `Rack.cpp` and `PluginEditor.cpp`) compile unchanged.
- **`ModuleDescriptor::defaultZone`** added (append-only field); `typeTag`/`type` left independent of zone.
- **`RackLayout` model** added: `struct RackLayoutEntry { id, zone, position, visible }` + `std::vector<RackLayoutEntry> layoutModel`. `Placed` re-keyed to carry `id` (dropped its `zone`, which now lives in the model). Added `placedById()` lookup.
- **`addModule(Zone, desc)` → `addModule(desc)`**: reads `desc.defaultZone`, appends a layout entry with `position` = running per-zone index (call order) and `visible = true`.
- **`layout()` now walks `layoutModel`** (per zone, `visible` only, `stable_sort` by `position`), resolves each frame via `placedById`, and packs with the unchanged row-major first-fit geometry. Because position is seeded in call order, the default layout packs byte-identically to before (regression-safe by construction).
- **`visible` is plumbing only** in 4.1 (always true; the `layout()` filter is already wired so Story 4.3 needs no `layout()` change). No persistence yet (Story 4.2).
- **No audio/param/format touch**: no `Parameters.h`, `PresetIO`, or `.synthy` change; no new `.cpp` → no `CMakeLists.txt` change.
- Visual identity + preset round-trip to be confirmed by the user on the running app (launched).

### File List

- `Source/UI/rack/ModuleDescriptor.h` (Zone enum + `defaultZone` field)
- `Source/UI/rack/Rack.h` (`using Zone`; `addModule(desc)`; `Placed` re-keyed; `RackLayoutEntry` + `layoutModel`; `placedById`)
- `Source/UI/rack/Rack.cpp` (`addModule` seeds model; `placedById`; `layout()` walks model; `#include <algorithm>`)
- `Source/UI/PluginEditor.cpp` (both `addModule` call-sites set `defaultZone`, drop the zone arg)
