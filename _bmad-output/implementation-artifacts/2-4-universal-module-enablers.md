---
baseline_commit: e759783
---

# Story 2.4: Universal module enablers (Master / ADSR / Mix-Mode overridable)

Status: done

<!-- Note: Validation is optional. Run validate-create-story for quality check before dev-story. -->

## Story

As a JASS player,
I want EVERY module to have a real, user-overridable enable toggle (not just the ones that happen to have a bypass param today),
so that the rack has one uniform anatomy — enabler + reset on every module — and I can actually switch Master, the ADSR envelope, and Mix-Mode on/off.

## Context & Spec Impact (mid-sprint change)

This story arose from a user decision on 2026-07-05 that **all** modules must carry an enabler + reset, and that the enabler may be static, externally-driven, or a real overridable param. It **revises previously-adopted spec**:

- **FR5 (revised):** previously "Modules with no on/off (Master, ADSR, Mix-Mode) **omit the toggle** but keep identical header geometry." Now: **every module shows an enable toggle**; always-on modules get a real enable param defaulting to on. Header geometry unchanged (already uniform).
- **FR7 (unchanged intent, wider scope):** disabled ⇒ body dimmed / header lit — now applies to Master/ADSR/Mix-Mode too.
- **NFR3 (softened, as with the 2026-07-01 enable-split):** the `.synthy` format gains three **append-only, backward-compatible** bool fields (`MasterOn`/`AdsrOn`/`MixModeOn`); a preset lacking them reads as **enabled**, so old presets and the C# app keep loading. No `kFormatVersion` bump. The C# app owes a matching mirror (see deferred-work).

_A prior UI-only step (commit `e759783`) already made the enabler **visible** on every module: param-backed modules interactive, Master/ADSR shown static-on, Mix-Mode a derived read-only toggle (osc1&&osc2). This story makes Master/ADSR/Mix-Mode **real, interactive, audio-affecting** enables._

## Acceptance Criteria

1. **Three new enable params.** `masterOn`, `adsrOn`, `mixModeOn` are added as `AudioParameterBool` (version 1), each **default `true`**, appended to `Parameters::createLayout()` (never reordering existing IDs). IDs declared in the `Parameters::ID` namespace. (NFR: append-only, no ID renamed/renumbered.)
2. **MASTER off = mute.** When `masterOn` is false, the master output is silenced (gain 0), gated at the single master-volume apply site in the processor. When true, behaviour is exactly as today.
3. **ADSR off = envelope bypassed.** When `adsrOn` is false, the amplitude envelope is not applied (constant gain 1.0 — the note sounds ungated by A/D/S/R while held/drone); the envelope still advances its state so toggling back mid-note doesn't glitch. When true, behaviour is exactly as today.
4. **MIX MODE off = plain additive.** When `mixModeOn` is false, the three oscillators are summed additively regardless of the `mixMode` value (RingMod/FM ignored). When true, `mixMode` behaves exactly as today.
5. **Interactive toggles + Mix-Mode dual condition.** Master, ADSR and Mix-Mode now render **interactive** enable toggles bound to their new params (frame-owned attachment, AD-6). Mix-Mode additionally keeps its derived `osc1On && osc2On` condition: its **effective enabled/lit state = `mixModeOn && osc1On && osc2On`** (dims if the user turns it off **or** if either OSC is off). The **audio** additive-fallback (AC4) keys off `mixModeOn` only; the `osc1&&osc2` part is a UI "meaningful" indicator, not an audio gate.
6. **Preset round-trip, backward compatible.** The three bools are written to and read from the `.synthy` format; a preset that lacks a field loads it as **enabled** (default true), so pre-existing presets and the current C# app are unaffected. `getStateInformation`/`setStateInformation` (host state via APVTS) already covers the new params automatically.
7. **RT-safe + build clean.** All gates are simple per-block/per-sample bool checks (no allocation/locking on the audio thread, NFR2); the final `[-1,1]` clamp and signal-chain order are unchanged. Project builds clean with JUCE warning flags on; the three modules toggle audibly + visually in the running app.

## Tasks / Subtasks

- [x] **Task 1 — New enable params (AC: 1)** — `masterOn`/`adsrOn`/`mixModeOn` IDs added; three `AudioParameterBool(…, true)` appended in `createLayout()` before the final return (append-only).
- [x] **Task 2 — Engine gating (AC: 2, 3, 4, 7)**
  - [x] MASTER: master `applyGain` gated by `masterOn` (else 0.0f) in `PluginProcessor::processBlock`.
  - [x] Plumbing: `Parameters::applyToVoice` signature extended with `bool& adsrOn, bool& mixModeOn`, set from APVTS; call site updated; `SynthVoice` got `adsrOn`/`mixModeOn` fields + `getAdsrOnRef()`/`getMixModeOnRef()`.
  - [x] ADSR gate: `const float envGain = envelope.process(); mixedSample *= (adsrOn ? envGain : 1.0f);` (envelope still advances).
  - [x] MIX MODE gate: FM + RingMod branches now require `mixModeOn`; else falls through to additive.
- [x] **Task 3 — Preset round-trip, backward compatible (AC: 6)** — `MasterOn`/`AdsrOn`/`MixModeOn` written in `toVar`; read in `applyVar` via `jbool(v, "…", rawB(a, ID::…))` so a missing field keeps the default (on).
- [x] **Task 4 — Editor descriptors (AC: 5)** — `enableParam` set: MASTER→`masterOn`, ENVELOPE-ADSR→`adsrOn`, MIX MODE→`mixModeOn` (inline; keeps `enabledWhen` osc1&&osc2).
- [x] **Task 5 — ModuleFrame: AND param-enable with derived predicate (AC: 5)** — `moduleEnabled()` now returns `paramOn && derivedOn`. Mix-Mode toggle is interactive (attachment); dim folds in the predicate; timer guard prevents overwriting the attached toggle.
- [x] **Task 6 — Spec + interop bookkeeping** — FR5 revised + Story 2.4 added in `epics.md`; `project-context.md` enable-note extended; C#-mirror ToDo added to `deferred-work.md`.
- [x] **Task 7 — Build + in-app verification (AC: all)** — Release build clean (after fixing a `std::atomic`/`float` ternary ambiguity at the master-gain line); `JASS.exe` launched for live verification.

## Dev Notes

### Exact touch points (from recon 2026-07-05)

- **`Source/Audio/Parameters.h`** — IDs in the `Parameters::ID` namespace (alongside the other `…On` bools); append 3 `AudioParameterBool(ParameterID(ID::x,1), "…", true)` in `createLayout()` before the final `return { params.begin(), params.end() };`. Extend `applyToVoice(...)` (the per-block param→voice apply) with `bool& adsrOn, bool& mixModeOn` and set them from `getRawParameterValue`.
- **`Source/PluginProcessor.cpp`** — master gain apply is a single line `buffer.applyGain(*apvts.getRawParameterValue(Parameters::ID::masterVol));` (in `processBlock`, right after the stereo stage). Gate it with `masterOn`. Also update the `Parameters::applyToVoice(...)` call to pass the two new voice bool refs.
- **`Source/Audio/SynthVoice.{h,cpp}`** — add `bool adsrOn { true };`, `bool mixModeOn { true };` fields + ref accessors (mirror `getMixMode()`/`getSubOctaveRef()`). Envelope apply is `mixedSample *= envelope.process();` — gate the multiply (keep the call). Mix-mode branch (FM / RingMod / additive) — require `mixModeOn` on the FM+RingMod branches.
- **`Source/Audio/PresetIO.h`** — `rawB`/`setRaw`/`jbool` helpers exist; follow the `WavefoldEnabled`/`StereoEnabled` write pattern and the `setRaw(a, ID::x, jbool(v, "Key", rawB(a, ID::x)) ? 1.f : 0.f)` read pattern (missing = current default = true).
- **`Source/UI/PluginEditor.cpp`** — set `enableParam` on MASTER, ENVELOPE-ADSR, MIX MODE descriptors; Mix-Mode keeps its `enabledWhen` predicate.
- **`Source/UI/rack/ModuleFrame.h`** — `moduleEnabled()` becomes `paramOn && derivedOn` (AND).

### The Mix-Mode dual-condition decision (AC5) — why AND, not replace

The user wants Mix-Mode (a) only *meaningful* when OSC1 **and** OSC2 are on (the original request) **and** (b) a real user-overridable enable. These are two different signals, so we combine them:

- `mixModeOn` (new param) — the **user** enable; also the **audio** gate (off ⇒ additive).
- `osc1On && osc2On` (existing `enabledWhen` predicate) — the **auto/meaningful** condition; **UI dim only**.
- **Effective lit** = both true. The interactive toggle reflects the user's `mixModeOn`; the dim overlay reflects the AND. When OSC1 is off, the module dims even though the user left Mix-Mode on — the audio still combines whatever OSCs are active per `mixModeOn` (a degenerate but harmless case). This keeps the two concerns independent and predictable. **Confirm this reads well in-app during review** — if the user prefers the audio to also force-additive when an OSC is off, that's a one-line change (gate the audio on `mixModeOn && …`), but keep it simple unless asked.

### Guardrails (project-context + ADs)

- **Append-only params (critical):** never rename/reorder existing IDs; add the three at the end of `createLayout()`. Default **true** so nothing changes for existing users until they toggle.
- **RT-safety (NFR2):** all three gates are bool reads already fetched per block (`applyToVoice`) or a single per-block `applyGain` — no allocation/locking. Keep `envelope.process()` called every sample (only the multiply is gated) so the envelope state never desyncs.
- **Frame owns attachments (AD-6):** the three new toggles bind via the frame from `enableParam`; no `*Attachment` members in the editor.
- **Format interop (Preset & State rules):** `.synthy` field names must match the C# `Preset` class; add the C# mirror ToDo (deferred-work). Missing field ⇒ enabled, so no `kFormatVersion` bump.
- **Signal chain untouched:** wavefold → filter → (envelope gate) → distortion → … → clamp. The gates change *whether* a stage's factor is applied, not the order or the final `[-1,1]` clamp.
- **Naming dualism / no folder rename** — unchanged.

### Previous-story intelligence

- **Precedent — the 2026-07-01 enable-split** (`lfoOn`/`noiseOn`/`filterOn`/`distortionOn`) is the exact playbook: append bool, keep `.synthy` back-compat, leave a C# mirror ToDo. This story extends it to the last three always-on modules — but these are *new* fields (no legacy "Off" string to map), so back-compat relies on "missing = default true".
- **UI groundwork already committed (`e759783`):** `ModuleFrame` renders a toggle for every module; `ModuleDescriptor.enabledWhen` exists; Mix-Mode has the `osc1&&osc2` predicate. This story flips Master/ADSR/Mix-Mode from static/derived display to real interactive params and wires the audio.
- **2.1 Sonnet-review lesson:** watch the enable/attachment interaction — the timer must not fight the `ButtonAttachment` (guarded), and reset excludes `enableParam` (so reset won't flip the new enables).

### Verification

Build + launch; the user confirms behaviour live ([[feedback-ui-verification]]). Manual checks in Task 7 (audible mute/bypass/additive + dim logic + preset round-trip incl. an old preset). No unit-test framework in this project.

### References

- [Source: _bmad-output/planning-artifacts/prds/prd-JASS-2026-06-28/prd.md#FR5/FR7/NFR3] (revised here)
- [Source: ARCHITECTURE-SPINE.md#AD-4/AD-6/AD-9] (descriptor + frame-owned binding; cross-module coupling via shared APVTS — the Mix-Mode predicate reads shared params only)
- [Source: _bmad-output/project-context.md] (enable-split precedent; append-only params; RT rules; `.synthy` interop + C# mirror)
- Recon (2026-07-05): Parameters.h IDs `:14-124`, append point `:271-273`, `applyToVoice` `:277-378` (mixMode set `:287`); PluginProcessor.cpp master gain `:351`, applyToVoice call `:249-260`; SynthVoice.cpp mix branch `:107-130`, envelope apply `:150`; PresetIO.h helpers `:52-95`, bool write `~:132-142`, bool read `~:248-258`.

## Dev Agent Record

### Agent Model Used

claude-opus-4-8[1m] (Opus 4.8, 1M context)

### Debug Log References

- Build error C2445 at the master-gain line: `masterOn ? *getRawParameterValue(...) : 0.0f` was ambiguous (`std::atomic<float>&` vs `float`). Fixed by `->load()` into a `const float masterGain` first. Rebuild clean.
- Final Release build (MSBuild, VS2022) clean; `SynthVoice.cpp`/`PluginProcessor.cpp`/`PluginEditor.cpp`/`ModuleFrame.cpp` recompiled → `JASS.exe`. Launched for live verification per [[feedback-ui-verification]].

### Completion Notes List

- **Three append-only enable bools** (`masterOn`/`adsrOn`/`mixModeOn`, default true). Master off = muted `applyGain`; ADSR off = envelope bypassed (constant gain, envelope still advances so no toggle glitch); Mix-Mode off = additive OSC sum regardless of `mixMode`.
- **Plumbing:** `Parameters::applyToVoice` carries the two voice gates (`adsrOn`/`mixModeOn`) as refs, like `mixMode`; master gate lives in the processor (post-render). RT-safe (bool reads only).
- **Mix-Mode dual condition:** `enableParam = mixModeOn` (interactive, gates audio) AND `enabledWhen = osc1&&osc2` (dim only). `ModuleFrame::moduleEnabled()` changed to `paramOn && derivedOn` so both are honoured; the `e759783` non-interactive display branch now applies only to the remaining param-less modules (Scope/Spectrum placeholders).
- **Preset back-compat:** new `.synthy` fields `MasterOn`/`AdsrOn`/`MixModeOn`; missing ⇒ enabled (via `jbool(..., rawB(...))`), no `kFormatVersion` bump. Host state (APVTS) covers them automatically.
- **Spec:** FR5 revised (every module has an enabler); Story 2.4 added to epics; project-context enable-note extended; C#-mirror ToDo logged.
- **Watch in review:** MASTER is XXS — header now has title + toggle + reset + one knob; may be tight (user to confirm). Reset excludes `enableParam`, so ↺ won't flip the new enables (correct).

### File List

- `Source/Audio/Parameters.h` (UPDATE) — `masterOn`/`adsrOn`/`mixModeOn` IDs + params; `applyToVoice` gates.
- `Source/Audio/SynthVoice.h` (UPDATE) — `adsrOn`/`mixModeOn` fields + ref accessors.
- `Source/Audio/SynthVoice.cpp` (UPDATE) — envelope + mix-mode gates.
- `Source/PluginProcessor.cpp` (UPDATE) — master-gain gate + `applyToVoice` call site.
- `Source/Audio/PresetIO.h` (UPDATE) — write/read the three bools (back-compat: missing ⇒ on).
- `Source/UI/PluginEditor.cpp` (UPDATE) — `enableParam` on MASTER/ADSR/MIX MODE.
- `Source/UI/rack/ModuleFrame.h` (UPDATE) — `moduleEnabled()` = param AND predicate.
- `_bmad-output/planning-artifacts/epics.md` (UPDATE) — FR5 revised + Story 2.4.
- `_bmad-output/project-context.md` (UPDATE) — enable-universalization note.
- `_bmad-output/implementation-artifacts/deferred-work.md` (UPDATE) — C#-mirror ToDo.

## Change Log

- 2026-07-05 — Story 2.4: universal module enablers. Master/ADSR/Mix-Mode gained real overridable enable params (`masterOn`/`adsrOn`/`mixModeOn`, default on): Master off=mute, ADSR off=envelope bypass, Mix-Mode off=additive. `.synthy` extended append-only (missing⇒on). `ModuleFrame::moduleEnabled()` = param AND derived predicate. FR5/FR7 revised, NFR3 softened; C#-mirror ToDo. Build clean. Status → review.
