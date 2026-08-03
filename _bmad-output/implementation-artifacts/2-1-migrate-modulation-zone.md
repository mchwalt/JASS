---
baseline_commit: 69e8fda
---

# Story 2.1: Migrate the MODULATION zone

Status: done

<!-- Note: Validation is optional. Run validate-create-story for quality check before dev-story. -->

## Story

As a JASS player,
I want the ADSR envelope (with its curve display), the LFO and the Arpeggiator rebuilt as authoritative rack modules,
so that the modulation section looks and behaves as one consistent unit — matching the generators.

## Acceptance Criteria

1. **Coverage + anatomy.** ENVELOPE - ADSR (size **L**), LFO (size **M**) and ARPEGGIATOR (size **M**) appear in the MODULATION zone, each with the uniform header anatomy and its assigned size class. No control, binding or behaviour is lost versus the old UI (FR12/FR13). ADSR has **no** enable toggle (always-on/always-lit, header geometry identical — FR5); LFO's enable is `lfoOn`, Arpeggiator's enable is `arpOn`.
2. **ADSR curve is a real `Display` body element (headline of this story).** The ADSR module's `Display` slot renders the **real `EnvelopeDisplay`** (the attack→decay→sustain-hold→release curve), **not** the throwaway `SampleDisplayPlaceholder`. The curve updates as A/D/S/R change (the component already polls the four params at 20 Hz and repaints on change). It occupies the second unit-row of the L module (4 slots) beneath the A/D/S/R knobs, and dims uniformly with the body when… (ADSR is always-on, so it never dims — but the display must sit under the same frame-level dim overlay as any other body element, per AD-5).
3. **LFO WAVE combo lists the param's OWN choices in order (correctness fix).** The LFO WAVE `Combo` must be built from the `lfoWave` parameter's actual choice list **`{ "Sine", "Triangle", "Square", "Sawtooth" }`** — **NOT** the shared `waves` array (`{ "Saw", "Square", "Sine", "Triangle" }`) currently used in `buildSampleRack`. Because a `ComboBoxAttachment` maps by **index** (item _i_ ↔ choice _i_), the shared array currently mislabels every LFO waveform (selecting "Saw" actually sets choice 0 = Sine, etc.). After the fix, each visible WAVE label produces the matching LFO waveform. (LFO TARGET already correctly lists `{ "Frequency", "Amplitude", "Filter Cutoff" }` = the `lfoTarget` choices with `"Off"` removed per the enable-split.)
4. **All controls bound and behaviour preserved.**
   - **LFO:** WAVE (combo, fixed per AC3), TARGET (combo), RATE (knob), DEPTH (knob); enable = `lfoOn`. All bound via frame-owned attachments (AD-6). The LFO drives modulation **rings on its target knobs elsewhere** (OSC FREQ/AMP, FILTER CUTOFF) via the existing single-timer mechanism — the LFO module's own knobs carry **no** `modTarget`. No ring wiring changes here.
   - **Arpeggiator:** MODE (combo), RATE (knob), OCT (knob = `arpOctaves`, `AudioParameterInt` 1–4), GATE (knob); enable = `arpOn`. All bound.
   - **ADSR:** ATK/DEC/SUS/REL knobs bound to `attack`/`decay`/`sustain`/`release`.
5. **Reset ↺ behaviour (uniform-anatomy divergence, accepted).** The header reset resets every body param to its default **except the enable flag** (`ModuleFrame::doReset` derives the set from the body). This means LFO reset now also resets **`lfoTarget`** (the legacy LFO reset deliberately kept the routing; see Dev Notes) and Arp reset covers MODE/RATE/OCT/GATE but not `arpOn` (matches legacy, which excluded `arpOn`). The LFO-routing-preservation nicety is intentionally dropped in favour of the uniform framework reset — **do not** re-introduce a per-module reset-exclusion path.
6. **No engine/param/format change.** Parameter IDs, APVTS layout and the `.synthy` format are untouched (NFR2/NFR3). The project builds clean with JUCE warning flags on; the rack renders and every modulation control is bound and functional.

## Tasks / Subtasks

- [x] **Task 1 — Wire the real `EnvelopeDisplay` into the ADSR module (AC: 2)**
  - [x] In `buildSampleRack` (`PluginEditor.cpp`), for the ENVELOPE - ADSR descriptor, replaced `display("ADSR", 4)` with `Display{ sampleOwned.add(new EnvelopeDisplay(apvts, juce::Colour(0xff22d3ee))), 4 }` — a real `EnvelopeDisplay` owned by `sampleOwned` (same lifetime pattern as the placeholders).
  - [x] Colour: used the MODULATION zone/title accent cyan `0xff22d3ee` for zone consistency (legacy used green `0xff4ade80`).
  - [x] Separate instance from the legacy `adsrEnvDisplay` (a `juce::Component` has one parent; legacy panel still owns its own — kept for Story 3.3).
  - [x] SCOPE and SPECTRUM `display(...)` placeholders left as-is (real wiring is Story 2.3).
- [x] **Task 2 — Fix the LFO WAVE combo choice list (AC: 3)**
  - [x] LFO WAVE combo now built from `{ "Sine", "Triangle", "Square", "Sawtooth" }` (the `lfoWave` param's own choice order) inlined at the LFO call-site; shared `waves` array left untouched (OSC still uses it).
  - [x] LFO TARGET left as-is (`{ "Frequency", "Amplitude", "Filter Cutoff" }` matches `lfoTarget`).
- [x] **Task 3 — Promote the MODULATION descriptors to authoritative (AC: 1, 4, 5, 6)**
  - [x] Confirmed sizes (ADSR=L, LFO=M, ARP=M), enables (ADSR=none, LFO=`lfoOn`, ARP=`arpOn`), control sets. No size/order changes.
  - [x] Arp MODE combo items `{ "Up", "Down", "UpDown", "Random" }` match `arpMode` choices (index-aligned; canonical "UpDown" kept).
  - [x] Stable `id` auto-derived by `add()` (`lfo`/`arpeggiator`/`envelopeadsr`); left as-is.
- [x] **Task 4 — Build + in-app verification (AC: all)**
  - [x] Incremental Release build via `build/JASS_Standalone.vcxproj` (MSBuild, VS2022 path). Only `PluginEditor.cpp` recompiled → `JASS.exe`. No new files ⇒ no CMake change. Build clean (no warnings/errors).
  - [x] App launched for live verification (user confirms per [[feedback-ui-verification]]).

## Dev Notes

### What this story actually is

Like Story 1.5 for the generators: the throwaway sample rack (`buildSampleRack`) **already** renders ADSR, LFO and Arpeggiator as descriptors bound to the real params. So 2.1 is **not** "build the modulation modules from scratch." The real, load-bearing work is small and specific:

1. **Swap the ADSR `Display` placeholder for the real `EnvelopeDisplay`** (the one visible payoff of the story — the curve now renders inside the rack module, FR11 realized for the ADSR curve).
2. **Fix the LFO WAVE combo** — a genuine correctness bug where the shared `waves` array mislabels the LFO waveforms (see below).
3. Confirm the descriptors are authoritative (sizes, enables, bindings) and reset behaves per the uniform framework.

The legacy inline modulation panels (`lfo*`, `arp*`, `adsrEnvDisplay`, ADSR sliders in `PluginEditor`) are **not** deleted here — that is **Story 3.3**. The opaque rack already sits on top of them.

### The LFO WAVE mismatch (why AC3 matters)

The rack defines one shared `const juce::StringArray waves { "Saw", "Square", "Sine", "Triangle" }` (`PluginEditor.cpp:1035`) and reuses it for both OSC WAVE and LFO WAVE combos. But the params define different choice orders:

- `oscWave` → `{ "Sine", "Sawtooth", "Square", "Triangle" }` (`Parameters.h:148`)
- `lfoWave` → `{ "Sine", "Triangle", "Square", "Sawtooth" }` (`Parameters.h:219`)

A `juce::ComboBoxParameterAttachment` binds the combo's **selected index** to the parameter's choice index (it does **not** match by item text). So a combo whose item order differs from the param's choice order shows the wrong label for the actual value. For the LFO the shared `waves` order matches **neither** the display text nor the behaviour — this story fixes it by listing `lfoWave`'s own choices at the call-site.

**Out of scope but flag it:** the same shared `waves` array is *also* wrong for the OSC WAVE combos (order ≠ `oscWave` choices) — a latent Story 1.5 defect. **Do not fix the OSC combos in this story** (scope = MODULATION), but add a line to `deferred-work.md` so it's tracked as a 1.5 follow-up. (Ideal long-term fix: drive every choice-combo's item list from the param's own `getAllValueStrings()` so a combo can never drift from its param — note it, don't build it here.)

### EnvelopeDisplay — the component to wire

`EnvelopeDisplay` already exists (`PluginEditor.h:77-107`, `paint()` in `PluginEditor.cpp:259-306`). Constructor: `EnvelopeDisplay(juce::AudioProcessorValueTreeState& apvts, juce::Colour colour)`. It grabs the four raw param atomics (`attack`/`decay`/`sustain`/`release`), starts a 20 Hz timer, and repaints only when one of the four moves — so "updates as A/D/S/R change" (AC2) is satisfied for free. It is a plain `juce::Component`, so it drops straight into a `Display{ component, slots }` and is placed/dimmed by the frame like any other body element (AD-5). No polling/animation wiring is needed beyond constructing it.

**Ownership:** the existing `display()` lambda stores placeholders in `sampleOwned` (`PluginEditor.h:262`, a `juce::OwnedArray<juce::Component>`), and `Display.component` is **non-owning** (the editor owns lifetime — AD-5). Add the `EnvelopeDisplay` to `sampleOwned` the same way so lifetime/destruction-order parity with the shipped placeholder pattern is preserved (no new lifetime concern introduced).

### Reset semantics (AC5) — deliberate divergence from legacy

`ModuleFrame::doReset` (`ModuleFrame.cpp:214-235`) derives the reset set from the **body** params, skipping `desc.enableParam`. Consequences vs legacy:

- **LFO:** legacy reset excluded `lfoTarget` to "keep routing" (`PluginEditor.cpp:525`). The rack reset **includes** `lfoTarget` (resets it to default = Frequency). This is an accepted trade for uniform anatomy — **do not** add a per-module reset-exclusion mechanism to preserve the old nicety.
- **Arp:** legacy reset excluded `arpOn` (`PluginEditor.cpp:561`); the rack skips `arpOn` because it is the `enableParam`. Behaviour matches.
- **ADSR:** no enable, body = the four knobs → reset restores A/D/S/R defaults. Matches.

### Files to touch (all UPDATE — no NEW files, no CMake change)

- **`Source/UI/PluginEditor.cpp`** — the MODULATION section of `buildSampleRack` (`:1087-1097`): (a) ADSR `display("ADSR",4)` → `Display{ sampleOwned.add(new EnvelopeDisplay(apvts, <colour>)), 4 }`; (b) LFO WAVE combo → inline `{ "Sine", "Triangle", "Square", "Sawtooth" }` instead of `waves`. `EnvelopeDisplay` is declared in the already-included `PluginEditor.h`.
- **`_bmad-output/implementation-artifacts/deferred-work.md`** — add the OSC WAVE combo mismatch as a tracked 1.5 follow-up (documentation only).

_No processor / DSP / params / preset changes at all this story._

### Guardrails (project-context + ADs)

- **No new params / no `.synthy` change (NFR3):** every modulation param already exists; add/rename nothing. This story is pure UI descriptor wiring.
- **Frame owns attachments (AD-6):** all four ADSR knobs, LFO WAVE/TARGET/RATE/DEPTH and Arp MODE/RATE/OCT/GATE bind via the frame's attachments built from `paramId`. No `*Attachment` members in the editor for these.
- **Display = BodyElement (AD-5):** the ADSR curve is a `Display`, placed and dimmed by the same mechanism as controls. Don't special-case it.
- **RT-safety (NFR2):** UI-only; `EnvelopeDisplay` reads atomics on the message thread. No audio-thread code is touched.
- **Don't touch layout/anatomy** — sizes (L/M/M), the grid, `kHu`, header geometry and `resized()` are done. Don't reopen them.
- **Don't delete legacy modulation code** — that's Story 3.3. Leave the legacy `lfo*`/`arp*`/`adsrEnvDisplay`/ADSR sliders in place behind the opaque rack.
- **Naming dualism:** keep `Synthy*` / `rack::` / `buildSampleRack` (rename is Story 3.3 territory).

### Project Structure Notes

- Descriptor assembly stays in `PluginEditor` (AD-1 layer map). `EnvelopeDisplay` currently lives in `PluginEditor.h` — the Spine allows it to stay a display component or move into `rack/`; **leave it where it is** for this story (moving it is unnecessary churn and risks the legacy panel that still uses it).
- Auto-derived `id`s: `envelopeadsr` / `lfo` / `arpeggiator` (future layout-persistence key, Spine "Deferred"; unused today).

### Previous-story intelligence (1.1–1.5 + deferred review)

- **1.5 established the exact pattern** this story follows: promote sample descriptors to authoritative, keep legacy behind the opaque rack, verify live. Reuse that shape.
- **1.4** built the single-timer mod-ring + display-transform mechanism. LFO rings already work on the *target* knobs; this story adds no ring wiring (the LFO module's own knobs are not targets).
- **Combo attachment gotcha (from 1.5):** a bound combo's item list must match its param's choice **order/count** or labels drift from behaviour — exactly the LFO WAVE bug fixed here. (1.5's dynamic-combo `refreshCombo` re-applies selection after re-listing; static combos like these just need the right item list at build time.)
- **Deferred 1.1 review item now relevant:** "`Combo.items` empty/mismatched default renders wrong" — AC3 is a concrete instance; consider the param-driven-items idea noted above (defer, don't build).

### Verification

Build + launch; the user confirms behaviour live in the running app ([[feedback-ui-verification]] — don't reflexively re-read a screenshot). Manual checks in Task 4. No unit-test framework in this project.

### References

- [Source: _bmad-output/planning-artifacts/epics.md#Story 2.1] (MODULATION migration; ADSR curve as Display)
- [Source: ARCHITECTURE-SPINE.md#AD-4/AD-5/AD-6] (control vocabulary; Display is a BodyElement + uniform dim; frame owns binding)
- [Source: _bmad-output/project-context.md] (APVTS single source, RT rules, no param/format change, build workflow)
- Code: rack builder `PluginEditor.cpp:984-1127` (MODULATION at `:1087-1097`, shared `waves` at `:1035`, `display()`/`sampleOwned` at `:1029-1033` / `PluginEditor.h:262`). EnvelopeDisplay `PluginEditor.h:77-107` + `PluginEditor.cpp:259-306`. Reset `ModuleFrame.cpp:214-235`. Display render `ModuleFrame.cpp:203-210`.
- Params: `Parameters.h` — ADSR `:28-32`/`:175-180`; LFO `:88-93`/`:218-223` (`lfoWave` choices `:219`); Arp `:58-63`/`:258-263`. `oscWave` choices `:148` (the OSC mismatch to flag).
- Enums: `DSP/LFO.h:8-9` (`LFOWaveform`, `LFOTarget`), `DSP/Arpeggiator.h:14` (`Mode`).

## Dev Agent Record

### Agent Model Used

claude-opus-4-8[1m] (Opus 4.8, 1M context)

### Debug Log References

- Incremental Release build (MSBuild, VS2022) — clean; only `PluginEditor.cpp` recompiled → `JASS_SharedCode.lib` → `JASS.exe`. No warnings/errors.
- App launched (`JASS.exe`) for live in-app verification per [[feedback-ui-verification]].

### Completion Notes List

- **ADSR curve is now a real Display (AC2):** the ENVELOPE - ADSR module's second unit-row hosts a real `EnvelopeDisplay(apvts, 0xff22d3ee)` owned by `sampleOwned` (parity with the placeholder lifetime pattern). Separate instance from the legacy `adsrEnvDisplay` (one-parent rule; legacy panel untouched until Story 3.3). The component already polls the four ADSR params at 20 Hz and repaints on change, so the curve reshapes as A/D/S/R move — no extra wiring.
- **LFO WAVE mislabel fixed (AC3):** the LFO WAVE combo used the shared `waves` array `{Saw,Square,Sine,Triangle}`, which mismatched the `lfoWave` param choices `{Sine,Triangle,Square,Sawtooth}` — and a `ComboBoxAttachment` maps by index, so every LFO waveform label was wrong. Now inlined with the param's own choice order. LFO TARGET already matched `lfoTarget`.
- **Descriptors authoritative (AC1/4/5/6):** sizes L/M/M, enables (ADSR none, LFO `lfoOn`, ARP `arpOn`), all controls bound via frame-owned attachments (AD-6). No new/renamed params; `.synthy`/APVTS/DSP untouched (NFR2/NFR3). Reset semantics are the uniform framework behaviour (LFO reset now also resets `lfoTarget` — accepted divergence, AC5); no per-module reset-exclusion path added.
- **OSC WAVE mismatch flagged, not fixed:** the same shared `waves` array is also wrong for the OSC WAVE combos (`oscWave` = `{Sine,Sawtooth,Square,Triangle}`) — logged in `deferred-work.md` as a Story 1.5 follow-up (out of scope for 2.1).
- **Legacy modulation panels NOT deleted** (Story 3.3). The opaque rack sits on top of them.

### File List

- `Source/UI/PluginEditor.cpp` (UPDATE) — ADSR `Display` now wraps a real `EnvelopeDisplay` (via `sampleOwned`); LFO WAVE combo uses the `lfoWave` param's own choice order instead of the shared `waves` array.
- `_bmad-output/implementation-artifacts/deferred-work.md` (UPDATE) — logged the OSC WAVE combo item-order mismatch as a Story 1.5 follow-up.

## Change Log

- 2026-07-05 — Story 2.1: MODULATION zone migrated to authoritative descriptors — ADSR curve now a real `EnvelopeDisplay` body element, LFO WAVE combo mislabel fixed (param's own choice order). Build clean. Status → review. OSC WAVE mismatch flagged in deferred-work.
