# Story 4.3: Persist the custom layout to `.synthy` + reset layout

Status: review

<!-- Note: Validation is optional. Run validate-create-story for quality check before dev-story. -->

## Story

As a JASS player,
I want my customized rack layout (visibility, order, zone per module) saved in the preset and restorable to the default,
so that a layout I set up survives save/load and I can always get back to the stock arrangement.

## Acceptance Criteria

1. **Round-trips in `.synthy`.** After customizing (hide/reorder/zone-move) and saving a preset (or the shared LiveState), reloading it restores the exact layout — per-module `visible`, `zone`, `position`.
2. **One append-only field.** The layout is stored as a single structured `"RackLayout"` field in the `.synthy` JSON — **not** dozens of individual APVTS params. **No `kFormatVersion` bump**, no existing field renamed/reordered (NFR6).
3. **Missing ⇒ default.** A preset without `"RackLayout"` (older build, the C# app, or a stock preset) loads with the **built-in default layout**. The C# app still loads such presets unaffected (it ignores the unknown field).
4. **DAW state too.** The layout survives the plugin's `getStateInformation`/`setStateInformation` round-trip (editor closed) — carried as a **string property on the APVTS ValueTree** (XML-safe).
5. **Reset layout.** A **"Reset layout"** control restores the descriptor-default layout (all visible, default zones + order) **without touching any audio parameter**.
6. **Default presets stay clean.** When the layout equals the default, no `"RackLayout"` field / no state property is written (files stay minimal and byte-compatible with pre-4.3).
7. **No audio/param/format-structure change** (NFR2, NFR3); clean build; verify in the running app: customize → save → reset-to-default → load the saved file → layout returns.

## Tasks / Subtasks

- [ ] **Task 1 — Rack: serialize/deserialize the layout + capture defaults (AC: 1, 5, 6)**
  - [ ] Capture a `defaultLayout` snapshot as modules are added (mirror each seeded `RackLayoutEntry` into a `std::vector<RackLayoutEntry> defaultLayout`).
  - [ ] `juce::var layoutToVar() const` — array of `{ "id", "zone"(name string via `zoneName`), "pos", "vis" }` for the current `layoutModel`. `void applyLayoutVar(const juce::var&)` — apply by id (unknown ids ignored; missing modules keep default), then `relayout()` + `onLayoutChanged`.
  - [ ] `bool isDefaultLayout() const` — model equals `defaultLayout` (same order, zones, all visible).
  - [ ] `void resetLayout()` — copy `defaultLayout` → `layoutModel`, re-pack, write-through (Task 2), notify.
  - [ ] Add `static Zone zoneFromName(const juce::String&)` (inverse of `zoneName`; unknown ⇒ Generators).
- [ ] **Task 2 — Bridge the layout to the APVTS ValueTree (AC: 1, 4, 6)**
  - [ ] The APVTS `state` gets a **string property** (e.g. `"rackLayout"`) holding `JSON::toString(layoutToVar())`. The Rack writes it on every layout change (from `relayout`/the mutators or a single `writeThrough()` helper called by `onLayoutChanged`), and **clears it when `isDefaultLayout()`** (AC-6).
  - [ ] On Rack construction (after building defaults + reading descriptors), **read** the property if present and `applyLayoutVar` it, so a preset/DAW-state already loaded into APVTS shows its custom layout.
  - [ ] Guard against write/read feedback (a flag while applying).
- [ ] **Task 3 — PresetIO: mirror the property ↔ `"RackLayout"` field (AC: 1, 2, 3, 6)**
  - [ ] `toVar`: if `apvts.state` has a non-empty `rackLayout` string, parse it and `root->setProperty("RackLayout", parsedVar)` (nested, readable). Else omit.
  - [ ] `applyVar`: if `v` has `"RackLayout"`, set `apvts.state` property `rackLayout = JSON::toString(v["RackLayout"])`; else **remove** the property (⇒ default). (The param-reset-to-default loop already there does not touch this non-param property.)
- [ ] **Task 4 — Editor: re-sync on load + Reset button (AC: 1, 5)**
  - [ ] After the load button's `PresetIO::loadFromFile` (and the reset-to-default / random paths if they change layout), call `sampleRack->reloadLayoutFromState()` so an open editor reflects the just-loaded layout.
  - [ ] Add a **"Reset layout"** affordance in the customization panel (a `TextButton` at the bottom of the CallOutBox) → `sampleRack->resetLayout()` + rebuild the panel rows.
- [ ] **Task 5 — Verify (AC: 1–7)**
  - [ ] Clean Standalone build. In the app: hide a couple modules + reorder + move one across zones → Save preset. Reset layout (rack returns to stock). Load the saved preset → customized layout returns. Confirm a stock/old preset loads as default. Confirm no audio/param change and `.synthy` of an uncustomized patch has no `RackLayout` field.

## Dev Notes

### The persistence path (why a ValueTree string property)
The layout lives editor-side in `Rack`, but `.synthy` (`PresetIO::toVar`) and the DAW state (`getStateInformation`) both read the **processor's APVTS**. So the authoritative persistent copy must sit in the APVTS `state` ValueTree, with the Rack syncing to/from it:

```
Rack.layoutModel  --writeThrough-->  apvts.state["rackLayout"] (JSON string)
                  <--reload/ctor----
                                       |-- PresetIO.toVar  --> .synthy "RackLayout" (nested var, readable)
                                       |-- PresetIO.applyVar <-- .synthy "RackLayout"
                                       |-- getStateInformation (copyState → XML attr) [free]
```

- **Why a string, not a nested var, in the ValueTree:** `ValueTree::createXml` only round-trips primitive property types as XML attributes; a nested Array/Object var would not survive `getStateInformation`. A JSON **string** does. In `.synthy` we parse it back to a nested var so the file stays human-readable.
- **`applyVar` already resets all *parameters* to default first** (existing behaviour) — `rackLayout` is a **non-parameter** property, untouched by that loop, so handle it explicitly (set from field, or remove when absent ⇒ default).

### Current state of files to modify (READ before editing)
- **`Source/Audio/PresetIO.h`** — `toVar(apvts,name,modified)` builds a `DynamicObject` of named fields from APVTS; `applyVar` resets every param to default then reads fields with missing-key fallbacks (`jnum/jbool/jint`, `setChoice`, `choiceOrOff`). `kFormatVersion = 1` (do NOT bump). Add the `RackLayout` read/write around the existing body; access `apvts.state` for the property.
- **`Source/PluginProcessor.cpp`** — `getStateInformation` = `apvts.copyState()` → XML; `setStateInformation` = `replaceState`. LiveState auto-load in the constructor (before the editor exists) + `saveLiveState()` on change. No change needed if the property lives on `apvts.state` (copyState carries it). Optionally resync isn't possible here (no editor) — the editor reads on open.
- **`Source/UI/rack/Rack.{h,cpp}`** — has `layoutModel`, `applyLayoutOrder`, `setModuleVisible/ZoneVisible`, `relayout`, `onLayoutChanged`, `zoneName`. Add `defaultLayout` capture in `addModule`, `layoutToVar/applyLayoutVar/isDefaultLayout/resetLayout/zoneFromName`, and the write-through/reload against `apvts` (the Rack already holds `apvts&`).
- **`Source/UI/PluginEditor.cpp`** — load/reset/random button handlers (call `reloadLayoutFromState()` after they mutate APVTS); the `RackCustomizePanel` (add the Reset-layout `TextButton`).

### Guardrails
- **Append-only, no version bump (NFR3/NFR6/AD-11).** New JSON field + new ValueTree property only. Never rename/reorder existing fields or params. `kFormatVersion` stays 1.
- **Missing ⇒ default; default ⇒ omit.** Both directions, so old/C#/stock presets are unaffected and clean patches don't grow a field (AC-3, AC-6).
- **Reset touches no audio param** — it only rewrites `layoutModel` + the layout property.
- **Interop:** the C# app ignores `"RackLayout"` (unknown field) — no C# change needed now (mirror is a later follow-up, deferred-work).
- **Guard write/read feedback** when the Rack both writes and listens to the property.

### Testing standards
No unit-test framework — verify via clean build + running app (customize → save → reset → load round-trip; confirm stock preset loads default; confirm an uncustomized save has no `RackLayout`). User inspects the running app.

### Project Structure Notes
- Files: `Source/Audio/PresetIO.h`, `Source/UI/rack/Rack.{h,cpp}`, `Source/UI/PluginEditor.cpp` (+ maybe `PluginEditor.h` for the panel's reset button). No new `.cpp` expected → no `CMakeLists.txt` change.

### References
- [Source: _bmad-output/planning-artifacts/epics.md#Story 4.3]
- [Source: ARCHITECTURE-SPINE.md#AD-11] — append-only, interop-safe, missing⇒default; one `RackLayout` field + ValueTree property, not dozens of params.
- [Source: PRD FR19, NFR6]
- [Source: Source/Audio/PresetIO.h] — `.synthy` toVar/applyVar; `kFormatVersion`; missing-key fallbacks; MasterOn/AdsrOn/MixModeOn append-only precedent.
- [Source: Source/PluginProcessor.cpp#getStateInformation] — APVTS copyState/replaceState.

## Dev Agent Record

### Agent Model Used

claude-opus-4-8[1m]

### Debug Log References

- Clean incremental Release build (x64), `PluginEditor.cpp` + `Rack.cpp` recompiled.

### Completion Notes List

- **Persistence hub = a string property `rackLayout` on `apvts.state`.** `Rack::writeLayoutToState()` writes `JSON::toString(layoutToVar())` (array of `{id, zone(name), pos, vis}`) on every interactive mutation (`setModuleVisible`/`setZoneVisible`/`applyLayoutOrder`), or **removes** the property when `isDefaultLayout()` (clean presets). `layoutToVar`/`applyLayoutVar`/`zoneFromName` do the (de)serialization; `defaultLayout` captured in `addModule` powers `isDefaultLayout()` + `resetLayout()`.
- **Auto-save for free:** the processor is a `ValueTree::Listener` on `apvts.state` that sets `liveDirty` on any property change → 1.5s timer → `saveLiveState()`. So layout changes (incl. reorder-only, which touch no param) auto-persist to LiveState.
- **`.synthy` bridge (`PresetIO`):** `toVar` parses the `rackLayout` string and writes a nested readable `"RackLayout"` field (omitted when absent/default); `applyVar` writes the field back into the `rackLayout` property, or **removes** it when the field is missing (⇒ default). No `kFormatVersion` bump; the reset-to-default param loop doesn't touch the non-param property.
- **DAW state:** `getStateInformation`=`copyState()` carries the string property in the XML for free; `setStateInformation` restores it.
- **Editor:** `reloadLayoutFromState()` applies the persisted layout after the rack is built (LiveState already loaded by then) and after the Load button. `RackCustomizePanel` got a **"Reset layout"** button (bottom of the CallOutBox) → `resetLayout()` + rebuild rows.
- **Load/reset do NOT couple enables** — `applyLayoutVar`/`resetLayout` set `visible`/zone/pos only; enables restore from their own params (so a loaded preset isn't clobbered).
- **Invariant fix (user-reported): hidden ⇒ never audible.** A hidden module "does not exist / is unreachable for the synth" (user's model). Bug: the header **RESET** (`resetToDefault` force-enables all 3 OSCs) and **RANDOM** reset param enables under a stale layout → a hidden OSC could play invisibly. Fix: `Rack::enforceHiddenDisabled()` forces every hidden module's `enableParam` off; called after RESET, after RANDOM, and at the end of `reloadLayoutFromState()` (defensive, also catches pre-coupling / hand-edited / C# presets where visible=false but enable=on). Header RESET keeps modules hidden (does not restore visibility) but now silent — matches the user's lean.
- **Known minor:** a DAW `setStateInformation` while the editor is open doesn't auto-resync the open panel (edge case; editor reads on open). Layout changes don't flip the "Current State" modified indicator (params-only) — acceptable. DAW automation of a hidden module's enable while the editor is closed isn't guarded (no editor to enforce) — deferred edge.

### File List

- `Source/UI/rack/Rack.h` (persistence decls + `defaultLayout` + `kLayoutStateProp`)
- `Source/UI/rack/Rack.cpp` (`layoutToVar`/`applyLayoutVar`/`isDefaultLayout`/`writeLayoutToState`/`resetLayout`/`reloadLayoutFromState`/`zoneFromName`; `defaultLayout` capture; write-through in mutators)
- `Source/Audio/PresetIO.h` (`toVar`/`applyVar` ↔ `"RackLayout"` field)
- `Source/UI/PluginEditor.cpp` (`reloadLayoutFromState` on build + load; panel "Reset layout" button)
