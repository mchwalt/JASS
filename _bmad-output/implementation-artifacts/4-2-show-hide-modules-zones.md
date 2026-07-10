# Story 4.2: Show / hide modules and zones (auto-fit height)

Status: review

<!-- Note: Validation is optional. Run validate-create-story for quality check before dev-story. -->

## Story

As a JASS player,
I want to hide modules I don't use and hide whole zones,
so that I can pare the rack down to just what I'm working with.

## Acceptance Criteria

1. **Hide a module.** The user can hide any individual module. Its `visible` flag flips to `false` in the `RackLayout` model (Story 4.1), the frame is removed from view (not merely dimmed per FR7), and the remaining modules **re-pack** to close the gap.
2. **Audio unaffected (hiding is UI-only).** A hidden module keeps its parameters and audio processing running — a hidden Filter still filters, a hidden LFO still modulates. No parameter, APVTS entry, `.synthy` field, or audio behaviour changes (NFR2, NFR3).
3. **Hide/show a zone.** The user can hide or show an entire zone as a unit — its zone header band and all its modules disappear/return together.
4. **Re-show.** A hidden module or zone can be brought back (there must be a discoverable path to re-show, since a hidden module has no header to click). A re-shown module returns with all bindings, modulation rings, and displays intact.
5. **Auto-fit height (AD-12).** After any show/hide the window **width stays fixed (1520 px)** and the **height auto-fits** the currently visible modules: the rack recomputes `preferredHeight(width)` and the editor re-applies `setSize`; the existing small-display auto-fit-down transform still applies.
6. **Session-only in 4.2.** Visibility lives in the in-memory model only; surviving save/load is Story 4.3. (So after a restart the rack shows the default — that is expected here.)
7. **Clean build**; verification = build + running app (toggle a few modules and a zone, watch re-pack + height change; confirm audio of a hidden module still sounds).

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
- **MODULES menu:** `juce::TextButton modulesBtn` in the header (right edge, clear of the centred title) → `showModulesMenu()` builds a `PopupMenu` with one submenu per zone (a tickable "Show this zone" + tickable module items; module items disabled when the zone is hidden). Toggling calls the Rack API, which re-packs + re-fits height.
- **Click-order rule (user, 2026-07-10):** re-showing a module appends it at the END of its zone (`position = maxPosInZone + 1`) so module order follows (re-)selection order, not the original build slot. MASTER BUS stays right-aligned (existing `layout()` colShift); other zones left-aligned.
- **Session-only:** visibility is in-memory (no `PresetIO`/`Parameters.h`/`.synthy` touch) — persistence is Story 4.3. No audio/param/format change; no new `.cpp` → no `CMakeLists.txt` change.

### File List

- `Source/UI/rack/Rack.h` (visibility API, `hiddenZones`, `onLayoutChanged`, `ModuleInfo`, `relayout`, `zoneName`)
- `Source/UI/rack/Rack.cpp` (visibility methods incl. append-on-show; `layout()` hidden-zone skip + frame `setVisible` pass; `zoneName`)
- `Source/UI/rack/ModuleFrame.h` (`moduleTitle()` accessor)
- `Source/UI/PluginEditor.h` (`modulesBtn`, `showModulesMenu`, `refitHeight` decls)
- `Source/UI/PluginEditor.cpp` (MODULES button + menu; `refitHeight()` extracted; `onLayoutChanged` wired; `<map>`/`<memory>` includes)
