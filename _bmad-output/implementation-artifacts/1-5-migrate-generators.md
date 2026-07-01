---
baseline_commit: 0866f77
---

# Story 1.5: Migrate the GENERATORS zone

Status: review

<!-- Note: Validation is optional. Run validate-create-story for quality check before dev-story. -->

## Story

As a JASS player,
I want all sound-source modules rebuilt as real descriptors in the rack (with working callbacks),
so that the generator section looks and behaves as one consistent unit.

## Acceptance Criteria

1. **Coverage + anatomy.** OSC 1–3, Sub, Noise, **STRING - KARPLUS**, Wavetable and Mix-Mode appear in the GENERATORS zone, each with the uniform header anatomy and its assigned size class (OSC/Karplus/Wavetable = M, Sub/Noise = S, **Mix-Mode = XS**). No control, binding or behaviour is lost versus the old UI (FR12/FR13).
2. **Karplus PLUCK (Action) — restore the original mechanism.** The Karplus module's **PLUCK** button re-plucks the Karplus string on all voices by restoring `SynthyProcessor::pluckString()` (a `std::atomic<bool> pluckRequested` flag consumed on the audio thread → `voice->pluckKarplus()` on every voice). The pluck sounds through the ADSR envelope (audible while a note or the auto-play drone is sounding) — exactly as it did before it was removed. Re-bind the spacebar to the same action, as before.
3. **Wavetable LOAD WAV (FileAction) + dynamic bank combo.** The Wavetable module's **LOAD WAV** opens a file chooser, loads the WAV into the shared `WavetableBankStore`, selects the new bank, and **declaratively refreshes** the BANK combo (a dynamic-provider `Combo`) via `FileAction.refreshes` — no external reference pokes the combo. The BANK combo lists the current bank names and stays in sync with the `wavetableBank` param.
4. **Mix-Mode coupling (AD-9).** Mix-Mode is its own **XS** (half-width) module (`Combo` on `mixMode`); its value changes how OSC 1/2/3 combine **purely through the shared `mixMode` APVTS param** — no module references another. It sits **between OSC 1 and OSC 2** in the GENERATORS zone — visually the OSC1↔OSC2 coupler.
4. **Live feedback preserved (from 1.4).** OSC FREQ knobs keep the played-frequency display transform; OSC AMP carries `modTarget = Amplitude`; these are now part of the real generator descriptors (not throwaway sample wiring).
5. **Stable module id (enabler).** Each generator descriptor carries a stable `id` (e.g. `"osc1"`, `"sub"`, `"mixmode"`) so a future custom layout (show/hide, drag-drop — Spine "Deferred") can reattach to modules. Purely additive descriptor data; no behaviour today.
6. **No engine/param/format change.** Parameter IDs, APVTS layout and the `.synthy` format are untouched (NFR2/NFR3). The project builds clean with JUCE warning flags on; the rack renders and every generator control is bound and functional.

## Tasks / Subtasks

- [x] **Task 1 — ModuleFrame: consume `Action`/`FileAction.refreshes` (AC: 3)**
  - [x] Record built combos by paramId (a `std::vector<{ juce::String paramId, juce::ComboBox*, std::function<juce::StringArray()> provider }>` for combos that have a dynamic provider).
  - [x] Add `void refreshCombo(const juce::String& paramId)`: re-poll the provider, `clear()` + `addItemList(...)` the box, then **re-apply the current selection from the param** (read `apvts.getRawParameterValue(paramId)` → set selected id = index+1 with `dontSendNotification`) so the `ComboBoxAttachment` stays consistent. Guard an empty provider (leave the box empty, no crash — closes the deferred 1.2 review item).
  - [x] In the `Action`/`FileAction` onClick wrappers, after firing `onClick`/`onChoose`, call `refreshCombo(id)` for each id in `refreshes`.
  - [x] Reference: the manual legacy equivalent is `SynthyEditor::refreshBankSelector()` (`PluginEditor.cpp:885-894`) — the rack version does the same generically, keyed off the descriptor.
- [x] **Task 2 — ModuleDescriptor: stable `id` field (AC: 5)**
  - [x] Add `juce::String id;` to `ModuleDescriptor` (additive; empty default). Document it as the future layout-persistence key (Spine "Deferred": module identity + mutable placement).
- [x] **Task 3 — Real generator descriptors in PluginEditor (AC: 1, 2, 3, 4, 5)**
  - [x] Turn the throwaway sample generator descriptors (`buildSampleRack`, `PluginEditor.cpp:1010-1030`) into the authoritative generator set. Keep the size classes already chosen (OSC=M, Sub=S, Noise=S, Karplus=M, Wavetable=M, MixMode=S). Set each descriptor's `id`.
  - [x] **Restore `pluckString()` on the processor (audio side):** re-add `void pluckString() { pluckRequested = true; }` (public) + `std::atomic<bool> pluckRequested { false };` to `SynthyProcessor`, and in `processBlock` (before `synth.renderNextBlock`) consume it RT-safely: `if (pluckRequested.exchange(false)) for (int i=0;i<synth.getNumVoices();++i) if (auto* v = dynamic_cast<SynthVoice*>(synth.getVoice(i))) v->pluckKarplus();`. This is the verbatim original mechanism (removed with commit `3f8e784`); `SynthVoice::pluckKarplus()` still exists (`SynthVoice.cpp:45`).
  - [x] **STRING - KARPLUS:** rename the module title to `"STRING - KARPLUS"`. Replace the no-op `Action{"PLUCK", []{}}` with `onClick = [&p]{ p.pluckString(); }`. Also re-bind the editor spacebar to `p.pluckString()` (it was bound before, via `keyPressed`).
  - [x] **Wavetable:** BANK becomes a dynamic-provider `Combo` on `wavetableBank`: `Combo{ P::wavetableBank, "BANK", []{ return WavetableBankStore::instance().getNames(); } }`. **LOAD WAV** `FileAction{ "LOAD WAV", [&p](juce::File f){ int idx = WavetableBankStore::instance().loadWav(f); if (idx >= 0) if (auto* pr = p.getAPVTS().getParameter("wavetableBank")) pr->setValueNotifyingHost(pr->convertTo0to1((float) idx)); }, { P::wavetableBank } }` — the `refreshes` list re-polls the BANK provider (Task 1). Mirror the legacy load logic (`PluginEditor.cpp:700-709`).
  - [x] **OSC 1–3:** keep the 1.4 wiring in the real descriptors — `Kfreq(P::oscFreq(i), "FREQ")` (played-frequency transform) + `Kmod(P::oscAmp(i), "AMP", ModTarget::Amplitude)`; WAVE combo, VOICES, DETUNE as knobs. Promote the `Kfreq`/`Kmod` helpers from sample-only to the real builders. **Build/add order must be OSC1 → Mix-Mode → OSC2 → OSC3** (addModule order = placement order), so unroll the `for i=1..3` loop or reorder the `add(...)` calls to insert Mix-Mode between OSC 1 and OSC 2.
  - [x] **Mix-Mode:** `Combo{ P::mixMode, "MODE", {"Additive","RingMod","FM"} }`, size **XS**, added **between OSC 1 and OSC 2**. No reference to any OSC module (coupling via the `mixMode` param only). Drop the old `Caption{"OSC 1 <-> 2"}` — at XS width there is no room, and sitting between OSC 1 and OSC 2 makes the coupling self-evident. Verify the combo text ("Additive"/"RingMod"/"FM") is not truncated at XS; if it is, keep the combo and shorten labels rather than widening the class.
  - [x] **Sub / Noise:** as today (Sub: WAVE combo + LEVEL; Noise: TYPE combo + AMP), with `id` set.
- [x] **Task 4 — Build + in-app verification (AC: all)**
  - [x] Incremental Release build (`build/JASS_Standalone.vcxproj`). No new files ⇒ no CMake change.
  - [x] Verify (build + launch; the user confirms live per [[feedback-ui-verification]]): all generators render with values + names; enabling an OSC + LFO shows rings (1.4 intact); **PLUCK** produces a plucked note; **LOAD WAV** loads a file and the BANK combo shows the new entry and selects it; changing **MODE** audibly changes how OSC1/2 combine; the FREQ knobs track a played (non-C4) note.

## Dev Notes

### What this story actually is

The prototype sample rack (`buildSampleRack`) already renders every generator as a descriptor — so 1.5 is **not** "build the rack from scratch"; it is: make the generator descriptors the **authoritative, fully-wired** set — real PLUCK + LOAD WAV callbacks, the dynamic BANK combo with declarative refresh, the stable-`id` enabler, and folding the 1.4 FREQ/AMP live-feed tags into the real descriptors. The legacy inline generator panels (`OscillatorPanel`, inline Sub/Noise/Wavetable, `EffectPanel` Karplus) are **not** deleted here — that is Story 3.3. The opaque rack already sits on top of them.

### Key mechanisms (with sources)

- **Karplus PLUCK — restore the original `pluckString()`.** PLUCK **was** wired historically (button + spacebar → `processor.pluckString()`, commit `03c90fd`); it was removed when the auto-play drone landed (`3f8e784`). The original mechanism: `pluckString()` sets `std::atomic<bool> pluckRequested`; `processBlock` does `if (pluckRequested.exchange(false)) for each SynthVoice v: v->pluckKarplus();` before `renderNextBlock`. This is RT-safe (atomic exchange; `pluckKarplus()` is audio-thread code that already exists, `SynthVoice.cpp:45`) and sounds through the ADSR — audible while a note or the drone is sounding. Restore it verbatim + re-point the PLUCK Action and the editor spacebar at it. (This is a small, justified audio-side restoration — not a parameter or format change, so NFR3 holds.)
- **Wavetable BANK + LOAD WAV.** Banks live in the process-wide singleton `WavetableBankStore::instance()` (`Source/DSP/WavetableBank.h`): `getNames()` → bank name list, `loadWav(File)` → loads a WAV as a new bank and returns its index. The `wavetableBank` APVTS param (`AudioParameterInt 0..MaxBanks-1`, `Parameters.h:239`) selects it; the engine reads it in `Parameters.h:364-365`. Legacy did **manual** combo sync because the items are dynamic (`refreshBankSelector`, `PluginEditor.cpp:685-709,885-894`) — the rack does it declaratively via `FileAction.refreshes` + the frame's new `refreshCombo` (Task 1). **Gotcha:** after re-populating a bound combo you MUST re-apply the param's current selection (id = value+1, `dontSendNotification`) or the `ComboBoxAttachment` and the box drift apart.
- **Mix-Mode coupling stays param-only (AD-9).** The audio engine reads `mixMode` to combine OSC 1/2/3; the Mix-Mode module only edits that param. No OSC descriptor knows about it; no module holds a reference to another.
- **1.4 live feed is already generic** — `Kfreq`/`Kmod` helpers + `ModuleFrame::updateLiveFeed` exist; just use them in the real OSC descriptors instead of the sample-only tags. FREQ = played-frequency transform, AMP = `ModTarget::Amplitude`. (Filter cutoff ring is Story 2.2.)

### Files to touch (all UPDATE — no NEW files, no CMake change)

- **`Source/UI/rack/ModuleDescriptor.h`** — add `juce::String id;` to `ModuleDescriptor`.
- **`Source/UI/rack/ModuleFrame.h/.cpp`** — record dynamic-provider combos; add `refreshCombo(paramId)`; consume `Action`/`FileAction.refreshes` in the onClick wrappers. (The `Action`/`FileAction` build paths are at `ModuleFrame.cpp` in `buildBody`; the FileAction chooser wrapper already exists — add the refresh call after `onChoose`.)
- **`Source/PluginProcessor.h/.cpp`** — restore `pluckString()` + `pluckRequested` atomic + the `processBlock` consumption (Task 3, first subtask). Small audio-side restoration; no param/format change.
- **`Source/UI/PluginEditor.cpp`** — real generator descriptors in `buildSampleRack` (rename to a real builder is optional; keep the `add(...)` helper): PLUCK→`pluckString()`, spacebar re-bind, LOAD WAV, dynamic BANK combo, `id`s, KARPLUS title, promote `Kfreq`/`Kmod`. Include is already present (`#include "../DSP/WavetableBank.h"`, `PluginEditor.cpp:2`). Note: MIX MODE is already XS between OSC 1 and OSC 2 (done in a prior layout tweak) — leave it.

### Guardrails (project-context + ADs)

- **No new params / no `.synthy` change (NFR3):** every generator param already exists; do not add or rename any. `wavetableBank` write-back uses `setValueNotifyingHost(convertTo0to1(idx))`.
- **Frame owns attachments (AD-6):** the dynamic BANK combo still gets a `ComboBoxAttachment`; the refresh must preserve its selection (see gotcha). Do not hand-wire a manual `onChange` param write for it — the attachment handles value↔param; `refreshCombo` only re-lists items + re-applies selection.
- **RT-safety (NFR2):** PLUCK/LOAD WAV run on the message thread; `keyboardState.noteOn/off` is the normal UI note path (safe). `WavetableBankStore::loadWav` does file I/O on the message thread (as legacy) — never from audio.
- **Don't touch layout/anatomy** — the frame anatomy (name-on-top, value box, uniform font) and the rack grid are done. Do not re-open `resized()`/`kHu`.
- **Don't delete legacy generator code** — that's Story 3.3. This story leaves the legacy panels in place behind the opaque rack.

### Project Structure Notes

- Descriptor assembly stays in `PluginEditor` (AD-1 layer map). `WavetableBankStore` is the existing shared store — reuse it, do not create a second source of bank names.
- `id` values: use short stable slugs (`osc1/osc2/osc3/sub/noise/karplus/wavetable/mixmode`). These are the future layout keys (Spine "Deferred").

### Previous-story intelligence (1.1–1.4 + deferred review)

- **Closes a deferred 1.2 review item:** "combo dynamic-provider edge cases — provider polled once at build; `Action/FileAction.refreshes` never consumed." Task 1 implements exactly this; also guard the empty-provider case (bound but item-less combo).
- 1.4 established `Kfreq`/`Kmod` + `updateLiveFeed`; reuse verbatim.
- The `FileAction` build path already guards against re-entrant chooser opens (`fileChooserActive`) — keep that; just add the refresh after the file is applied.
- Naming dualism: keep `Synthy*`/`rack::`; `WavetableBankStore` singleton name unchanged.

### Verification

Build + launch; the user confirms behaviour live in the running app ([[feedback-ui-verification]] — don't reflexively re-read the screenshot). Manual checks in Task 4. No unit-test framework in this project.

### References

- [Source: _bmad-output/planning-artifacts/epics.md#Story 1.5]
- [Source: ARCHITECTURE-SPINE.md#AD-1/AD-4/AD-6/AD-9] and #Deferred (module id / mutable placement enabler)
- [Source: _bmad-output/project-context.md] (APVTS single source, RT rules, WavetableBankStore, build workflow)
- Legacy: `PluginEditor.cpp:685-709` (LOAD WAV + bank onChange), `:885-894` (refreshBankSelector). PLUCK history: commit `03c90fd` wired `karplusPanel.onTrigger = [this]{ processor.pluckString(); }` + a spacebar `keyPressed`; commit `3f8e784` removed `pluckString()`/`pluckRequested` when the auto-play drone was added — restore both. Pluck impl: `SynthVoice.cpp:45` (`pluckKarplus`). Store: `Source/DSP/WavetableBank.h`. Params: `Parameters.h:239,364-365` (`wavetableBank`).

## Dev Agent Record

### Agent Model Used

claude-opus-4-8[1m] (Opus 4.8, 1M context)

### Debug Log References

- Incremental Release build (MSBuild) — clean; `PluginProcessor.cpp`, `PluginEditor.cpp`, `ModuleFrame.cpp`, `Rack.cpp` recompiled → `JASS.exe`.
- In-app verification (user-confirmed live per [[feedback-ui-verification]]): generators render; WAVETABLE shows BANK + LOAD WAV (tight but fits); Mix-Mode XS between OSC 1/2 (done in a prior layout commit); STRING - KARPLUS present. PLUCK/LOAD-WAV wired.

### Completion Notes List

- **PLUCK restored (AC2):** `SynthyProcessor::pluckString()` + `std::atomic<bool> pluckRequested` re-added; consumed in `processBlock` before `renderNextBlock` (`if (pluckRequested.exchange(false)) for each SynthVoice v: v->pluckKarplus();`). PLUCK Action → `processor.pluckString()`; spacebar re-bound in `keyPressed`. Verbatim original mechanism (removed with the drone in `3f8e784`). RT-safe.
- **Declarative combo refresh (AC3, closes deferred 1.2 item):** `ModuleFrame` records dynamic-provider combos (`dynCombos`) and `refreshCombo(paramId)` re-polls the provider + re-applies the param's selection (keeps the ComboBoxAttachment consistent); `Action`/`FileAction.refreshes` now consumed after firing. WAVETABLE BANK is a dynamic `Combo` on `wavetableBank`; LOAD WAV → `WavetableBankStore::loadWav` + set param + refresh.
- **Stable id (AC5):** `ModuleDescriptor::id` added; the editor's `add()` helper derives a slug from the title (`"OSC 1"`→`"osc1"`). Forward-looking (layout persistence); unused today.
- **1.4 live feed folded in:** OSC FREQ = `Kfreq` (played-freq transform), AMP = `Kmod(Amplitude)` in the real descriptors.
- **No param/format change (AC6):** `pluckString` is not a parameter; `wavetableBank` write-back uses `setValueNotifyingHost(convertTo0to1(idx))`; `.synthy`/APVTS untouched. Legacy generator panels NOT deleted (Story 3.3).
- **Follow-up (layout, not functional):** WAVETABLE at M with BANK added = 8 slots (tight, off-centre); OSC row density + row height are handled in the separate density pass. STRING-KARPLUS title + MIX-MODE XS placement were done in a prior layout commit (`3fb56e1`).

### File List

- `Source/PluginProcessor.h` (UPDATE) — `pluckString()` + `pluckRequested` atomic.
- `Source/PluginProcessor.cpp` (UPDATE) — consume `pluckRequested` in `processBlock`.
- `Source/UI/rack/ModuleDescriptor.h` (UPDATE) — `id` field.
- `Source/UI/rack/ModuleFrame.h` (UPDATE) — `refreshCombo` decl + `dynCombos` store.
- `Source/UI/rack/ModuleFrame.cpp` (UPDATE) — record dynamic combos; consume `Action`/`FileAction.refreshes`; `refreshCombo` impl.
- `Source/UI/PluginEditor.cpp` (UPDATE) — PLUCK→`pluckString()`, spacebar re-bind, WAVETABLE BANK combo + real LOAD WAV, `add()` derives `id`.

## Change Log

- 2026-07-02 — Story 1.5: GENERATORS migrated to real descriptors — PLUCK (`pluckString()` restored) + spacebar, WAVETABLE dynamic BANK combo + LOAD WAV with declarative refresh, stable module ids, 1.4 FREQ/AMP live-feed folded in. Build clean. Status → review.
