# Story 4.2: Rack customization panel — show/hide, reorder & move between zones (auto-fit height)

Status: done

<!-- Note: Validation is optional. Run validate-create-story for quality check before dev-story. -->

## Story

As a JASS player,
I want a panel with a reorderable list of all modules where I can toggle visibility and drag items to change their order and zone,
so that I can tailor which modules show and where they sit — the list order is the on-screen order.

_(Approach revised 2026-07-11: a reorderable customization **list-panel** replaces on-rack drag & drop and the interim show/hide popup. It unifies show/hide + reorder + zone-move in one place; former Story 4.4 is folded in here.)_

## Acceptance Criteria

1. **Show/hide a module.** A left checkbox per module toggles `visible` in the `RackLayout` model (Story 4.1); hidden = removed from the rack (not merely dimmed per FR7) and the rack **re-packs**.
2. **Audio unaffected (hiding is UI-only).** A hidden module keeps its parameters and audio running — a hidden Filter still filters. No parameter, APVTS entry, `.synthy` field, or audio behaviour changes (NFR2, NFR3).
3. **Reorder + move between zones by dragging list rows.** The panel lists modules grouped by zone in current order; dragging a module row within its zone reorders it, and dragging it across a zone header moves it to that zone (updates `zone` + `position`). **List order = on-screen placement order.**
4. **Identity preserved on move.** A module keeps its type/colour tag when moved to another zone (Reverb dragged into GENERATORS stays a Processor).
5. **Zone show/hide.** A zone header row's checkbox hides/shows the whole zone.
6. **Single packing path (NFR1).** All mutations go through the Rack API (`setModuleVisible` / `setZoneVisible` / `applyLayoutOrder`) → the one `layout()` path re-packs; no ad-hoc bounds. MASTER BUS stays right-aligned, other zones left-aligned.
7. **Auto-fit height (AD-12).** After any change the window **width stays fixed (1520 px)** and the **height auto-fits** the visible modules; the small-display auto-fit-down transform still applies.
8. **Session-only in 4.2.** The layout lives in the in-memory model only; surviving save/load is Story 4.3 (so a restart shows the default — expected here).
9. **Clean build**; verification = build + running app (toggle modules + a zone, drag to reorder and across zones, watch re-pack + height; confirm a hidden module still sounds).

## Tasks / Subtasks

- [ ] **Task 1 — Rack: visibility state + mutation API (AC: 1, 3)**
  - [ ] Module visibility already exists as `RackLayoutEntry.visible` (Story 4.1). Add public `void setModuleVisible(const juce::String& id, bool);` that flips the entry's `visible`, then triggers a re-layout + notifies the editor (see Task 3).
  - [ ] Add zone visibility: a `std::set<Zone>` (or `bool` per zone) of hidden zones, plus `void setZoneVisible(Zone, bool);`. In `layout()`, a hidden zone contributes **no header band and no modules** (skip it entirely, before the per-module `visible` filter).
  - [ ] Add read accessors for the menu: e.g. `bool isModuleVisible(id)`, `bool isZoneVisible(Zone)`, and a way to enumerate modules per zone with their titles (a small snapshot struct, or expose the ordered ids per zone). Keep `layoutModel` private; expose queries, not the vector.
- [ ] **Task 2 — Rack: hidden frames don't paint; visible ones re-pack (AC: 1, 2)**
  - [ ] In `layout(apply=true)`, after placing the visible entries, call `frame->setVisible(false)` on every frame whose module is hidden (or whose zone is hidden) and `setVisible(true)` on the rest. Hidden frames keep existing (APVTS attachments/audio untouched) — they are just not shown or placed.
  - [ ] Confirm `preferredHeight()` (`layout(w,false)`) already accounts for visibility (it walks the same model/zone filter) so the measured height matches the applied one.
- [ ] **Task 3 — Editor: re-fit window height on layout change (AC: 5)**
  - [ ] Extract the height-fit math from the `SynthyEditor` constructor (currently computes `kDesignH = jmax(1015, rackH + kBodyTop + kBodyBottom + 2*kMargin)` and calls `setSize`) into a reusable `void refitHeight();`.
  - [ ] Wire a change signal from Rack → editor (e.g. `std::function<void()> Rack::onLayoutChanged;` invoked after any visibility mutation) and set it to call `refitHeight()`. Keep width fixed at `kDesignW = 1520`.
  - [ ] Verify the auto-fit-down transform for small displays still recomputes correctly at the new size.
- [ ] **Task 4 — UI affordance: the "MODULES" menu (AC: 1, 3, 4)**
  - [ ] Add a `juce::TextButton modulesBtn` to the header chrome (same pattern as `saveBtn`/`loadBtn`: `addAndMakeVisible`, `onClick`, positioned in `resized()`'s header band). Label e.g. "MODULES" (or a ☰ glyph consistent with the restyled header).
  - [ ] `onClick` builds a `juce::PopupMenu`: for each zone in rack order, add a **section header** + a tickable "Show <ZONE>" item (tick = zone visible), then the zone's modules as tickable items (tick = module visible). Selecting an item flips that module/zone visibility via the Rack API (Task 1), which re-lays-out and re-fits height.
  - [ ] Tick state reflects current visibility; the menu is rebuilt each open from the Rack queries.
- [ ] **Task 5 — Verify (AC: 1–7)**
  - [ ] Clean incremental Standalone build; add any new `.cpp` to `target_sources` (none expected — header + `Rack.cpp` + `PluginEditor.cpp`).
  - [ ] Run the app: hide OSC 3 → it vanishes, others re-pack, window shortens; play a note with a hidden Filter enabled → still filtered (audio unaffected). Hide the PROCESSING zone → header + all effects vanish. Re-show from the MODULES menu → returns intact.

## Dev Notes

### Foundation from Story 4.1 (already in place)
- `RackLayout` model exists: `Rack` holds `std::vector<RackLayoutEntry> layoutModel` where `RackLayoutEntry = { juce::String id; Zone zone; int position; bool visible = true; }`. `layout()` already walks the model per zone, **visible only**, `stable_sort` by `position`, resolving frames via `placedById(id)`. So the *rendering* side of hide already works — this story adds the **state mutation, the frame `setVisible`, the zone-hide skip, the height re-fit, and the menu UI.**
- `Zone` lives in `ModuleDescriptor.h` (`namespace rack`); `Rack::Zone` is an alias. Each descriptor carries `defaultZone`.

### Current state of files to modify (READ before editing)
- **`Source/UI/rack/Rack.h` / `Rack.cpp`** — `layout(width, apply)` is the single packing path; per-zone it filters `e.zone == zone && e.visible`. `preferredHeight()` calls `layout(w,false)`. `frames` (OwnedArray) currently all visible. Add: `setModuleVisible`/`setZoneVisible`, hidden-zone set, visibility queries, `onLayoutChanged` callback, and the `frame->setVisible(...)` pass in `layout(apply)`.
- **`Source/UI/PluginEditor.cpp`** — constructor builds header buttons (`saveBtn`, `loadBtn`, …) as `juce::TextButton` members with `onClick` lambdas; `resized()` lays out the header band + `sampleRack->setBounds(rb)`. The height math lives in the constructor (`kDesignW=1520`, `kBodyTop/Bottom=72`, `kMargin=12`, `rackH = sampleRack->preferredHeight(rackW)`, `setSize(kDesignW, kDesignH)`) — extract into `refitHeight()`. Add `modulesBtn` member (declare in `PluginEditor.h`).
- **`Source/UI/PluginEditor.h`** — declare the new `juce::TextButton modulesBtn;` and `void refitHeight();`.

### Design decision — the show/hide affordance (chosen; override in review if you prefer)
A single **"MODULES" menu button** in the header opening a `juce::PopupMenu` (zones as sections + tickable module items). Rationale: (a) a hidden module has no header to right-click, so re-show needs a central entry point; (b) one button is far less header clutter than a per-module hide control (the header is already tight — MASTER is XXS); (c) it uniformly covers module-hide, module-show, and zone show/hide. A per-module right-click "Hide" could be added later as a shortcut, but the menu is the primary and sufficient path for this story.

### Guardrails
- **Hiding is strictly UI.** Never gate audio on visibility. A hidden module's `ModuleFrame` keeps its APVTS attachments; do not destroy/detach it — only `setVisible(false)` + exclude from `layout()`. This keeps AC-2 true by construction and avoids any RT/parameter impact.
- **`layout()` stays the single packing path (NFR1/AD-2).** All re-packing goes through `layout()`; the menu/mutation code must not set any frame bounds directly.
- **Height only, width fixed (AD-12).** `refitHeight()` must not change width; keep `kDesignW`.
- **No persistence here.** Do not touch `PresetIO`/`Parameters.h`/`.synthy` — that is Story 4.3. Visibility resets to default on relaunch, which is the expected 4.2 behaviour (AC-6).
- **Zone visibility is separate model state** from per-module `visible`; both will be persisted together in 4.3, so keep zone-hidden state in a form that is easy to serialize (e.g. a set of zone enums).

### Testing standards
No unit-test framework. Verify via clean build + running app: toggle modules and a zone from the MODULES menu, confirm re-pack + height change, and confirm a hidden-but-enabled module still affects the audio.

### Project Structure Notes
- Work stays in `Source/UI/rack/Rack.{h,cpp}` + `Source/UI/PluginEditor.{h,cpp}`. No new files expected → no `CMakeLists.txt` change.
- Naming: `modulesBtn`, `refitHeight`, `setModuleVisible`, `setZoneVisible`, `onLayoutChanged` follow the existing camelCase member/callback conventions.

### References
- [Source: _bmad-output/planning-artifacts/epics.md#Story 4.2] — story + ACs (reordered 2026-07-10: show/hide before persistence).
- [Source: ARCHITECTURE-SPINE.md#AD-10] — RackLayout single source of truth; mutate model → re-run `layout()`.
- [Source: ARCHITECTURE-SPINE.md#AD-12] — width fixed, height auto-fits visible modules.
- [Source: PRD FR15, FR16] — show/hide modules and zones; hiding is UI-only.
- [Source: Source/UI/rack/Rack.cpp] — `layout()` visible-filter (Story 4.1); `preferredHeight`.
- [Source: Source/UI/PluginEditor.cpp#SynthyEditor ctor] — header button pattern + height-fit math to extract.

## Dev Agent Record

### Agent Model Used

claude-opus-4-8[1m]

### Debug Log References

- Clean incremental Release build (x64). First link hit `LNK1104` because the Story-4.1 `JASS.exe` was still running and locking the exe → closed the process, relinked clean.

### Completion Notes List

- **Rack visibility API:** `setModuleVisible(id,bool)`, `setZoneVisible(Zone,bool)`, `isModuleVisible`, `isZoneVisible`, `modulesInZone(Zone)` (ordered `{id,title,visible}` for the menu), `zones()`, static `zoneName(Zone)`, and `onLayoutChanged` callback. Hidden zones tracked in a `std::vector<Zone> hiddenZones`.
- **`layout()`:** skips hidden zones entirely (no header band, no modules, no height), and after the apply pass sets `frame->setVisible(...)` so hidden modules/zones are taken out of view while keeping their APVTS attachments + audio (hiding is UI-only). `preferredHeight()` measures the same visible set.
- **Editor height auto-fit (AD-12):** extracted `refitHeight()` from the constructor (fixed `kDesignW=1520`, height from `preferredHeight`, plus the small-display auto-fit-down transform — now resets to identity when no scaling is needed). Wired `sampleRack->onLayoutChanged = [this]{ refitHeight(); }`.
- **Interim popup (commit `6d8610a`) → replaced by the panel (this commit).** First pass shipped show/hide via a `PopupMenu`; per the user's 2026-07-11 decision it is now a **reorderable customization list-panel** (`RackCustomizePanel`, anonymous namespace in `PluginEditor.cpp`) shown in a `juce::CallOutBox` anchored to `modulesBtn`. Rows grouped by zone: left checkbox = visibility (module or whole zone via the header row); dragging a module row reorders it and dragging across a zone header moves it to that zone.
- **New Rack API:** `applyLayoutOrder(vector<pair<id,Zone>>)` — the panel hands the full ordered (id,zone) list; the rack assigns each `zone` + a within-zone `position` = running index. `setModuleVisible` reverted to *visibility-only* (order is now owned explicitly by the list, so a toggle keeps position — the earlier append-on-show rule is obsolete under the list paradigm).
- **CallOutBox parent = nullptr (desktop)** so the editor's auto-fit transform doesn't skew the panel's mouse coordinates.
- **Session-only:** layout is in-memory (no `PresetIO`/`Parameters.h`/`.synthy` touch) — persistence is Story 4.3. No audio/param/format change; the panel lives in `PluginEditor.cpp` → no new `.cpp`, no `CMakeLists.txt` change.
- MASTER BUS stays right-aligned (existing `layout()` colShift); other zones left-aligned.
- **Visibility ↔ enable coupling (refinement 2026-07-11, user-driven; revises FR15).** Hiding a module also **disables** it and showing **re-enables** it once (`Rack::driveEnable` writes the module's `enableParam` via APVTS on the interactive `setModuleVisible`/`setZoneVisible` path). Rationale: a hidden module's controls aren't reachable, so running it hidden is pointless. **Reorder / zone-move does NOT change state** — `applyLayoutOrder` only sets `zone`+`position`, never `visible` or enables. The coupling is interactive-only; **load/reset restore `visible` + enables independently** (no `driveEnable`), so a loaded preset isn't clobbered. Needs `ModuleFrame::enableParamId()`.
- **Zone header = derived, not a separate state (refinement 2026-07-11, user-driven).** Removed the `hiddenZones` state / `isZoneVisible`. A zone's header on the rack is now **derived** in `layout()` (drawn iff ≥1 module in the zone is visible), so emptying a zone auto-hides its header and re-enabling any module brings it back — single source of truth = per-module `visible` (AD-10), and 4.3 persistence needs no zone flag. In the panel the zone header checkbox is a **tri-state bidirectional bulk toggle** (`setZoneVisible` now bulk-sets all members): all-on → all-off, none/mixed → all-on. The panel always lists every zone/module (even when empty on the rack) so nothing is unreachable.

### File List

- `Source/UI/rack/Rack.h` (visibility API, `applyLayoutOrder`, `hiddenZones`, `onLayoutChanged`, `ModuleInfo`, `relayout`, `zoneName`, `<utility>` include)
- `Source/UI/rack/Rack.cpp` (visibility methods; `applyLayoutOrder`; `layout()` hidden-zone skip + frame `setVisible` pass; `zoneName`)
- `Source/UI/rack/ModuleFrame.h` (`moduleTitle()` accessor)
- `Source/UI/PluginEditor.h` (`modulesBtn`, `showModulesMenu`, `refitHeight` decls)
- `Source/UI/PluginEditor.cpp` (`RackCustomizePanel` list-panel + `CallOutBox`; MODULES button; `refitHeight()` extracted; `onLayoutChanged` wired)
