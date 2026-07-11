# Story 6.1: Per-module online help — info icon, movable panel, EN/DE (resource-backed)

Status: done

<!-- Note: Validation is optional. Run validate-create-story for quality check before dev-story. -->

## Story

As a JASS player,
I want a circled-"i" info icon in each module's header that opens a movable description panel in my chosen language,
so that I can learn what each module does without leaving the synth or reading a separate manual.

## Acceptance Criteria

1. **Resource-backed help, keyed by module id.** Help texts live in **language resource files** in a `Resources/` folder (`help_en.json`, `help_de.json`), each a flat map of **module id → description**. The `ModuleDescriptor` gains **no** text field — help is looked up by the descriptor's existing stable `id`.
2. **Opt-in per module.** A module whose `id` has a non-empty entry in the (active-or-EN) resource shows a **circled-"i"** icon in its header; a module with no entry shows **no icon** and is unchanged.
3. **Header info icon.** Header geometry stays uniform across all modules (reserve the info slot like the enable/reset slots).
4. **Click opens the panel.** Clicking the info icon opens a help panel showing the module's **title** + its **description in the active language** (EN fallback if the active language lacks that id).
5. **Movable.** The panel is draggable by its title bar and does not snap back.
6. **Explicit close only.** The panel stays open until closed via its **top-right "✕"** or the **ESC** key. No hover trigger, no auto-dismiss on mouse-leave/outside-click.
7. **Single panel.** Opening help for another module (or re-clicking) reuses one shared panel — re-target/reposition, never stack.
8. **Language selector in the JASS header.** A combo box in the header offers **EN** and **DE**. Changing it sets the active help language; a **currently-open** panel updates its text immediately on change.
9. **Both languages authored.** Every currently-shipping module has an accurate 1–2 sentence description in **both** `help_en.json` and `help_de.json`. OSC help mentions the new **FB** (Self-FM) control.
10. **Extensible.** Adding a third language later = a new `help_xx.json` resource + a new combo entry, no code-structure change (lookup is keyed by language code + module id).
11. **No interaction regressions.** Info icon, panel, and selector don't disturb knob drag, value-box edit, combo dropdowns, enable toggle, reset ↺, Action/FileAction buttons, or the customization panel.
12. **UI-only (audio/preset).** No new/renamed parameter, no APVTS change, no `.synthy` schema change, no audio-thread work (NFR2, NFR3); repaint cost negligible (NFR5). (A `CMakeLists.txt` change to embed the resources IS expected — see AC 13.)
13. **Resources embedded via BinaryData (DECIDED).** The `Resources/*.json` files are compiled into the app via `juce_add_binary_data` and read from `BinaryData` at runtime — robust for **both** Standalone and VST3 (no external file dependency). Editing help text requires a rebuild (accepted).
14. **Language persistence (DECIDED — global app setting).** The chosen language is remembered as a **global app setting** under `%AppData%\Roaming\Synthy` (via `PresetIO::synthyFolder()`), surviving restarts, **without** touching `.synthy` or any preset. Default `"EN"` when absent.
15. **Clean build + verified in the running app:** icon only where a help entry exists; click opens the panel; drag moves it; "✕" and ESC both close; another module reuses the panel; EN↔DE switch re-renders (incl. an open panel); language sticks across restart; all existing controls still behave.

## Tasks / Subtasks

- [ ] **Task 1 — Resource files (AC: 1, 9)** — new `Resources/help_en.json`, `Resources/help_de.json`
  - [ ] Create a `Resources/` folder at the repo root. Each file is a flat JSON object keyed by **module id** (the descriptor's stable slug — e.g. `osc1`, `osc2`, `osc3`, `sub`, `noise`, `string`/`karplus`, `wavetable`, `mixmode` (CROSS MOD), `adsr`/`envelope`, `lfo`, `arpeggiator`, `filter`, `distortion`, `wavefold`, `bitcrush`, `chorus`, `delay`, `reverb`, `stereo`, `master`, `scope`/`oscilloscope`, `spectrum`). **Confirm each id** against the slugs derived in `PluginEditor.cpp` (`d.id = title.toLowerCase().retainCharacters("a-z0-9")`, `PluginEditor.cpp:513`) — e.g. "OSC 1"→`osc1`, "CROSS MOD"→`crossmod`. Get the exact ids from the running descriptors, don't guess.
  - [ ] `help_en.json`: EN descriptions (suggested copy in Dev Notes). `help_de.json`: natural DE equivalents (user is a native German speaker and will review). OSC entries mention **FB** (Self-FM).
- [ ] **Task 2 — Embed resources via CMake (AC: 12, 13)** — `CMakeLists.txt`
  - [ ] Add `juce_add_binary_data(JASS_Resources SOURCES Resources/help_en.json Resources/help_de.json)` and link it: `target_link_libraries(JASS PRIVATE JASS_Resources ...)`. This generates `BinaryData::help_en_json` / `help_de_json` (verify the exact generated symbol names after the first configure).
  - [ ] Re-generate CMake (build/ already exists; a `CMakeLists.txt` change triggers a re-configure — see [[feedback_build]]).
- [ ] **Task 3 — HelpTextStore (AC: 1, 2, 4, 10)** — new `Source/UI/HelpTextStore.h` (header-only)
  - [ ] A singleton mirroring `WavetableBankStore::instance()` (see `Parameters.h` usage). On first use, parse both embedded JSONs into `std::map<juce::String /*lang*/, std::map<juce::String /*id*/, juce::String>>`.
  - [ ] API: `bool has(const juce::String& id) const` (true if EN — the base language — has a non-empty entry); `juce::String get(const juce::String& id, const juce::String& lang) const` (active lang, else EN, else `{}`); `bool isLoaded()`.
  - [ ] Header-only OK (parsing at first call); no `.cpp` needed → the only `target_sources` impact is the binary-data lib from Task 2.
- [ ] **Task 4 — Info icon in the header (AC: 2, 3, 11)** — `Source/UI/rack/ModuleFrame.{h,cpp}`
  - [ ] `buildHeader()`: when `HelpTextStore::instance().has(desc.id)`, create an info button (circled-"i", UTF-8 "ⓘ" `"\xE2\x93\x98"`), tinted like reset ↺ (`typeColour(desc.type)`); `onClick` fires the frame's `onHelp(desc.id)`. Mirror the conditional creation of `enableBtn`/`resetBtn` (`ModuleFrame.cpp:49`).
  - [ ] `resized()`: reserve the info slot in the header right cluster (currently `enableSlot=header.removeFromRight(24); reset=header.removeFromRight(20)` at `:280-283`), ~18–20 px left of reset, **unconditionally** for uniform geometry; place `infoBtn` there when present. Verify fit on the smallest header (XXS/XS ~114 px); if cramped, place info at far left before the title (pick one, keep it uniform). Must not swallow child/header events.
  - [ ] Add member `std::function<void(const juce::String& id)> onHelp;`. (ModuleFrame only needs the id; the editor resolves title + text + language.)
- [ ] **Task 5 — Wire icon → editor via the Rack (AC: 4, 7)** — `Source/UI/rack/Rack.{h,cpp}`, `Source/UI/PluginEditor.{h,cpp}`
  - [ ] `Rack`: add `std::function<void(const juce::String& id)> onModuleHelp;` (mirror `onLayoutChanged`, `Rack.h:88`). In `addModule`, wire `frame->onHelp = [this](auto& id){ if (onModuleHelp) onModuleHelp(id); };`.
  - [ ] `PluginEditor`: `sampleRack->onModuleHelp = [this](auto& id){ showModuleHelp(id); };`. `showModuleHelp(id)` stores the current id, resolves the module **title** (via `sampleRack->moduleById(id)->moduleTitle()`, `Rack.h:49`/`ModuleFrame.h:36`) + text `HelpTextStore::instance().get(id, currentLang)`, creates-or-reuses the single `HelpPanel`, sets its text, positions it, `setVisible(true)`, `toFront(true)`, `grabKeyboardFocus()`.
- [ ] **Task 6 — Language selector in the header (AC: 8, 14)** — `Source/UI/PluginEditor.{h,cpp}`
  - [ ] Add `juce::ComboBox langBox` member with items `EN`, `DE`. Add + style it in the constructor near the other header controls (`~PluginEditor.cpp:278`); position it in `resized()` on the header right edge next to `modulesBtn` (`:680`, another `headerRow.removeFromRight(...)`), clear of the centred title.
  - [ ] Hold `juce::String currentLang` (init from the persisted setting, else `"EN"`). `langBox.onChange`: update `currentLang`; if the panel is open, re-resolve its text for the stored id and re-set it; persist the choice.
  - [ ] Persistence helpers `loadUiLanguage()` / `saveUiLanguage()` writing a small `juce::PropertiesFile` (or one-line JSON) under `PresetIO::synthyFolder()`. Provide `juce::String uiLanguage() const { return currentLang; }` for future UI localization.
- [ ] **Task 7 — The movable help panel (AC: 4, 5, 6, 7)** — new header-only `Source/UI/HelpPanel.h`
  - [ ] `class HelpPanel : public juce::Component`, child of the editor (floats above the rack), initially hidden; owned by `PluginEditor`.
  - [ ] Content: bold title + word-wrapped description (paint directly, or a non-editable multi-line `juce::TextEditor`/`Label`). Fixed width ~260 px, height fit to wrapped text. `setText(title, body)` re-renders (used on open AND on language switch).
  - [ ] **Drag:** `juce::ComponentDragger`; title-bar `mouseDown`→`startDraggingComponent`, `mouseDrag`→`dragComponent` (optionally clamp within editor bounds).
  - [ ] **Close "✕":** a `TextButton` (UTF-8 "✕" `"\xE2\x9C\x95"`) top-right → hides the panel.
  - [ ] **ESC:** `setWantsKeyboardFocus(true)` + `grabKeyboardFocus()` on show; `keyPressed` closes on `KeyPress::escapeKey`. Fallback: editor's own `keyPressed` also closes it (host focus can be flaky).
- [ ] **Task 8 — Verify (AC: 15)** — clean incremental build (a `CMakeLists.txt` change forces a re-configure; then `build/JASS_Standalone.vcxproj`, MSBuild/PowerShell), launch, confirm all behaviours incl. EN↔DE switch (live update of an open panel), language sticks across restart, + no regressions. User reviews the running app ([[feedback_ui_verification]]).

## Dev Notes

### DESIGN EVOLUTION (user decisions, 2026-07-11) — implement the LATEST
1. Backlog said *hover >2 s / title-click auto-dismiss popup* → **replaced** by a **circled-"i" info icon**, **click-only**, a **movable panel** closed **only** by "✕"/ESC. ⇒ **no hover-dwell timer, no title-click, `CallOutBox` NOT suitable** (not draggable, dismisses on outside-click) — use a custom child panel.
2. Help must be **multi-language (EN + DE to start)**, switched by a **combo box in the JASS header**.
3. **Help texts live in external resource files in a `Resources/` folder** (NOT inline in the descriptor). ⇒ descriptor carries **no** help text; a `HelpTextStore` loads `Resources/help_<lang>.json` (keyed by module `id`). Decided: **embed via `juce_add_binary_data`** (robust for Standalone + VST3); language choice persists as a **global app setting**.

### Why resource-backed, not inline (the key structural choice)
- "Inline" would mean writing the EN/DE strings as C++ literals on each `ModuleDescriptor` in `PluginEditor.cpp` — scattered through UI-build code, translation = code edit. **Rejected** per user.
- Instead: one flat JSON per language, keyed by the descriptor's **existing stable `id`** (`PluginEditor.cpp:513`). The descriptor needs **no new field** — the info icon appears iff `HelpTextStore::has(id)`. Texts sit in one place, are translatable without touching UI code, and adding a language is a new file.

### The rack framework (what this story extends)
- **`ModuleDescriptor`** (`Source/UI/rack/ModuleDescriptor.h`): unchanged for this story (uses existing `id`). No new field.
- **`ModuleFrame`** (`Source/UI/rack/ModuleFrame.{h,cpp}`):
  - `buildHeader()` (`:49`) conditionally creates `enableBtn`/`resetBtn` — mirror for `infoBtn` gated on `HelpTextStore::instance().has(desc.id)`.
  - `resized()` (`:267`) reserves the header right cluster **unconditionally** (enable `removeFromRight(24)`, reset `removeFromRight(20)`, title fills rest, `:280-284`). Add the info slot here.
  - **No timer** needed (click model). Existing dim timer untouched.
- **`Rack`** creates/owns frames in `addModule`, exposes `onLayoutChanged` (`Rack.h:88`) and `moduleById`/`moduleTitle` (`Rack.h:49`, `ModuleFrame.h:36`) — reuse those to resolve the clicked module's title.

### HelpTextStore — mirror the existing store pattern
- The project already has a resource singleton: `WavetableBankStore::instance()` (used in `Parameters.h`). Model `HelpTextStore` on it: a private singleton, lazy-parse the two embedded JSONs once, expose `has(id)` / `get(id, lang)` / `isLoaded()`.
- Read the embedded bytes via `BinaryData::getNamedResource(...)` or the generated `BinaryData::help_en_json` / `..._jsonSize` symbols, then `juce::JSON::parse`. **Verify the generated symbol names** after the first CMake configure (JUCE mangles filenames → identifiers).

### BinaryData / CMake
- No `juce_add_binary_data` exists yet (`CMakeLists.txt` has only `target_sources` + `target_link_libraries`). Add a `JASS_Resources` binary-data target and link it PRIVATE. Editing help text ⇒ rebuild (accepted, AC 13).
- `HelpTextStore.h` and `HelpPanel.h` are header-only ⇒ no `target_sources` change; the only CMake edit is the binary-data target.

### The panel (custom, because CallOutBox can't be dragged / kept open)
- `HelpPanel` is a **child of `PluginEditor`** (not a desktop window) so it floats above the rack, inherits the look, won't steal host focus. `toFront(true)` to layer.
- **Do NOT reuse** the `CallOutBox` at `PluginEditor.cpp:379` (customization panel; its outside-click dismiss contradicts AC 6).
- Drag via `juce::ComponentDragger`; close via "✕"; ESC via `keyPressed` (+ editor fallback).

### Header layout — where the language combo goes
- Header is a 64 px row (`resized()` ~`:675`): left cluster (340 px) = 2×2 Save/Load/Random/Reset + preset name; centred "J A S S" title (`g_titleBounds`); `modulesBtn` overlays the right edge (`removeFromRight(120)`, `:680`). Put `langBox` on the **right edge next to `modulesBtn`**; keep clear of the centred title. Construct/style beside the other header controls (`~:278`).

### Language state & persistence — DECIDED (global app setting)
- App-global UI state, **not** per-preset ⇒ NOT in `.synthy`. Persist via a small `juce::PropertiesFile` / one-line JSON under `PresetIO::synthyFolder()` (`%AppData%\Roaming\Synthy`). Default `"EN"` absent. Isolate behind `loadUiLanguage()` / `saveUiLanguage()`.
- (Rejected: session-only — doesn't persist; APVTS/DAW state — muddles a UI preference into audio state.)

### Suggested EN help copy (author natural DE in help_de.json; OSC must mention FB)
- **osc1/2/3:** "Core oscillator. Pick a waveform and tune FREQ; AMP sets level, VOICES/DETUNE add unison, and FB adds self-FM feedback (sine → sawtooth → gritty)."
- **crossmod (CROSS MOD):** "Ring-modulates or FM-couples two selectable oscillators (SRC A/B). Off = the oscillators just add together."
- **sub:** "A sine/square sub-oscillator tracking OSC 1's pitch one or two octaves down, for low-end weight."
- **noise:** "White or pink noise layer — for percussion, breath, and texture."
- **string (Karplus):** "Plucked-string physical model. PLUCK (or the spacebar) excites the string; DAMPING/STRETCH shape its decay."
- **wavetable:** "Scans a wavetable bank; POSITION morphs the waveform. LOAD WAV imports your own table."
- **adsr (ENVELOPE):** "Shapes each note's amplitude over time — Attack, Decay, Sustain, Release. Off = a flat, un-enveloped tone."
- **lfo:** "A low-frequency oscillator modulating pitch, amplitude, or filter cutoff (TARGET). Modulated knobs show a live ring."
- **arpeggiator:** "Turns a held chord into a rhythmic sequence — direction (MODE), speed (RATE), range (OCT), note length (GATE)."
- **filter:** "Low-/high-pass filter with resonance; the cutoff knob shows the LFO ring when the LFO targets it."
- **distortion / wavefold / bitcrush:** one line each on the flavour of dirt.
- **chorus / delay / reverb:** one line each (modulated doubling / echoes / space).
- **stereo:** "Pseudo-stereo widener for the mono engine — WIDTH and TIME set the spread."
- **master:** "Final output level."
- **scope / spectrum:** "Live oscilloscope / frequency-spectrum view of the output."

### Guardrails
- **UI + build only:** touch `ModuleFrame.{h,cpp}`, `Rack.{h,cpp}`, `PluginEditor.{h,cpp}`, new `HelpPanel.h` + `HelpTextStore.h`, new `Resources/help_*.json`, and `CMakeLists.txt` (binary data + tiny settings helper under `synthyFolder()`). **No** `Parameters.h`, **no** `.synthy` schema change, **no** DSP, **no** `ModuleDescriptor` field.
- **NFR1:** panel is an editor overlay, not part of the rack grid; `ModuleFrame::resized()` stays the single body-layout site. Only header change = one fixed icon slot + the header combo.
- **Header crowding:** enable + reset + info on a ~114 px XXS/XS header is the tightest case — verify; fall back to far-left info placement if needed (uniform across modules).
- **id drift:** the JSON keys MUST match the runtime descriptor ids exactly (derived from the title). Confirm against the built modules; a wrong key = silently no icon. Consider a debug `jassert` that every EN key resolves to a real module id (and/or log unmatched keys) to catch drift.
- **Lifetime/focus:** one editor-owned panel; ESC fallback in the editor if host focus is unreliable. **Don't stack panels** (reuse the instance).

### Testing standards
No unit-test framework → verify by build + running app: (1) icon only where a help entry exists; (2) click → correct title+text; (3) drag moves it; (4) "✕" closes; (5) ESC closes; (6) another module reuses the panel; (7) header combo EN↔DE switches text incl. an open panel; (8) language sticks across restart; (9) regressions: knob drag, value-box edit, combo open, enable toggle, reset ↺, MODULES panel. User reviews visually ([[feedback_ui_verification]]).

### Project Structure Notes
- New feature epic (Epic 6) beyond Epics 1–5; file named per convention (sibling of `5-1-selectable-mix-sources.md`).
- No `sprint-status.yaml` (stories tracked via `epics.md` + story files); registered in `epics.md` (Epic 6 / FR22 + FR23).
- New top-level `Resources/` folder (project assets) embedded via `juce_add_binary_data`; new header-only `Source/UI/HelpPanel.h` + `Source/UI/HelpTextStore.h`.
- **First use of embedded binary data in this project** — establishes the `Resources/` + `juce_add_binary_data` pattern for future assets (icons, more languages, tooltips).

### References
- [Source: _bmad-output/planning-artifacts/epics.md#Epic 6 / Story 6.1] — FR22 (info icon / movable panel) + FR23 (EN/DE + header selector).
- [Source: docs/Feature_Ideas.md#Backlog (2026-07-11)] — original backlog entry (superseded by the info-icon + multi-language + resource-file design; see "DESIGN EVOLUTION").
- [Source: Source/UI/rack/ModuleDescriptor.h] — descriptor (uses existing `id`; no new field).
- [Source: Source/UI/PluginEditor.cpp:513] — how `d.id` is derived from the title (defines the JSON keys).
- [Source: Source/UI/rack/ModuleFrame.cpp:49] — `buildHeader()` conditional icons; [Source: Source/UI/rack/ModuleFrame.cpp:280-284] — header slot reservation + title bounds; [Source: Source/UI/rack/ModuleFrame.h:36] — `moduleTitle()`.
- [Source: Source/UI/rack/Rack.h:88] — `onLayoutChanged` pattern to mirror; [Source: Source/UI/rack/Rack.h:37,49] — `addModule` wiring + `moduleById`.
- [Source: Source/UI/PluginEditor.cpp:278] — header controls constructed; [Source: Source/UI/PluginEditor.cpp:668-694] — header `resized()` (where `langBox` goes); [Source: Source/UI/PluginEditor.cpp:379] — `CallOutBox` (explicitly NOT reused).
- [Source: Source/Audio/Parameters.h] — `WavetableBankStore::instance()` singleton to mirror for `HelpTextStore`.
- [Source: Source/Audio/PresetIO.h] — `synthyFolder()` for the global language setting file.
- [Source: CMakeLists.txt] — add `juce_add_binary_data(JASS_Resources ...)` + link.
- [Constraint: ARCHITECTURE-SPINE AD-1/AD-5/NFR1] — data-driven descriptors; overlays, not per-module layout code.

## Dev Agent Record

### Agent Model Used

claude-opus-4-8[1m]

### Debug Log References

- CMake re-configured after adding `juce_add_binary_data` (new `JASS_Resources` target). Clean Debug x64 build; all sources compiled with no errors. (One transient `LNK1168` because a previously-launched `JASS.exe` still held the exe lock — resolved by killing the process, unrelated to the code.)

### Completion Notes List

- **Resource-backed, not inline.** Help texts live in `Resources/help_en.json` + `help_de.json`, flat maps keyed by module id. Embedded via `juce_add_binary_data(JASS_Resources ...)` in `CMakeLists.txt` (first BinaryData use in the project) and linked PRIVATE into `JASS`. Editing texts ⇒ rebuild.
- **Module ids confirmed against the runtime slugs** (`title.toLowerCase().retainCharacters(a-z0-9)`): `osc1/2/3`, `crossmod`, `sub`, `noise`, `stringkarplus`, `wavetable`, `envelopeadsr`, `lfo`, `arpeggiator`, `filter`, `distortion`, `wavefold`, `bitcrush`, `chorus`, `delay`, `reverb`, `stereo`, `master`, `oscilloscope`, `spectrum`. All 21 have EN + DE text. (`stringkarplus` matches the existing spacebar lookup in `keyPressed`.)
- **`HelpTextStore` (new `Source/UI/HelpTextStore.h`)** — header-only singleton mirroring `WavetableBankStore`; lazy-parses both embedded JSONs into `lang → (id → text)`; `has(id)` (EN base) / `get(id, lang)` (active, else EN, else empty).
- **`ModuleDescriptor` unchanged** — no new field; the info icon shows iff `HelpTextStore::has(desc.id)`.
- **`ModuleFrame`** — new optional `infoBtn` (circled-"i" ⓘ), created in `buildHeader()` only when help exists; header right cluster now reserves an 18px info slot unconditionally (uniform geometry). New `std::function<void(id)> onHelp` fired on click.
- **`Rack`** — new `onModuleHelp` callback (mirrors `onLayoutChanged`); each frame's `onHelp` wired to forward through it in `addModule`.
- **`HelpPanel` (new `Source/UI/HelpPanel.h`)** — header-only movable panel, child of the editor; title-strip drag via `ComponentDragger`, "✕" close button, ESC via `keyPressed`. Read-only, self-sizes to wrapped body via `TextLayout`. NOT a `CallOutBox` (needs drag + explicit-close).
- **`PluginEditor`** — header `langBox` (EN/DE) left of MODULES; owns one shared `HelpPanel` (added AFTER `dropFocus` so it keeps keyboard focus for ESC); `showModuleHelp(id)` resolves title (`moduleById(id)->moduleTitle()`) + text and shows/repositions the panel; `langBox.onChange` live-updates an open panel; ESC also handled in the editor's `keyPressed` as a focus fallback.
- **Language persistence** — global app setting `%AppData%\Roaming\Synthy\ui-language.txt` (`loadUiLanguage`/`saveUiLanguage`), default EN; NOT in `.synthy`.
- **UI-only:** no `Parameters.h`, no `.synthy` schema, no DSP change.
- **Icon rendering (user, 2026-07-11):** the header info + reset icons are **vector-drawn** (`Source/UI/rack/IconButton.h`, `Kind::Info` = circle + "i", `Kind::Reset` = circular arrow), replacing the font glyphs "ⓘ"/"↺" which rendered as tiny "?"/tofu in the shipped font and shrank to the header height. The `IconButton` fills its click box (~0.48·d radius) and tints to the module colour, so both icons are clearly legible. `resetBtn` changed from `juce::TextButton` to `IconButton`.
- **Post-verify refinements (user, 2026-07-11):** (a) panel now **anchors beside the clicked module** (right, else left; top-aligned; clamped inside the editor above the keyboard) instead of top-centre; (b) **re-clicking the same module's ⓘ toggles the panel closed** (`showModuleHelp` early-returns + hides when `isVisible() && currentHelpId == id`); (c) **drag fixed** — `HelpPanel` now drags with a `ComponentBoundsConstrainer` (`setMinimumOnscreenAmounts(kTitleH,…)`) so it stays within the editor and the title strip is always grabbable (previously it could be dragged under the keyboard and become unreachable).

### File List

- `Resources/help_en.json` (new) — EN help texts, keyed by module id
- `Resources/help_de.json` (new) — DE help texts
- `CMakeLists.txt` — `juce_add_binary_data(JASS_Resources ...)` + link PRIVATE
- `Source/UI/HelpTextStore.h` (new) — embedded-JSON help store singleton
- `Source/UI/HelpPanel.h` (new) — movable read-only help panel
- `Source/UI/rack/ModuleFrame.h` / `.cpp` — info icon + `onHelp` callback + header slot
- `Source/UI/rack/Rack.h` / `.cpp` — `onModuleHelp` callback + wiring in `addModule`
- `Source/UI/PluginEditor.h` / `.cpp` — language combo, shared HelpPanel, `showModuleHelp`, persistence helpers, ESC fallback
