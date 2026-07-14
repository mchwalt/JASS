# Story 8.1: Modulation Matrix — accumulating source→target routing engine

Status: ready-for-dev

<!-- Note: Validation is optional. Run validate-create-story for quality check before dev-story. -->

## Story

As a JASS sound designer,
I want a modulation matrix where any modulation source can be routed to any target with its own amount, and multiple sources can stack on the same target,
so that JASS gains the "movement layer" that makes patches feel alive — and so that macros, per-voice random, an evolution module, and additional LFOs become cheap follow-ons instead of one-off wiring.

## Context & Why This Story Is Foundational

Today modulation is **hard-wired to a single source and a single target**. The whole apply is an
if/else chain in `SynthVoice::renderNextBlock` (`SynthVoice.cpp:116-141` + tremolo at `188-189`)
that reads the one per-voice `lfo` member, picks the one active `LFOTarget`, and **replaces** the
base value. The mod-ring UI likewise transports exactly one (target, value) pair
(`PluginEditor.cpp:417-452` → `Rack.cpp:422` → `ModuleFrame.cpp:436`).

This blocks everything on the roadmap (`docs/JASS_Ideen_Merge.md` §2): a 2nd LFO can't stack on a
target the 1st already drives; macros/per-voice-random/evolution have nowhere to route. Crucially,
**the refactor a 2nd LFO would force — accumulate contributions per target, decouple source from
target — IS the modulation matrix.** So we build the matrix once, and the LFO question ("multiply
or extend?", raised 2026-07-14) is answered: neither — the sources become value providers on the
matrix, and more of them are then trivial.

This is the **second sanctioned DSP change** (after Epic 5). Keep it surgical and RT-safe; it is
bigger than 5.1 because it replaces a mechanism rather than generalizing a formula.

## Acceptance Criteria

1. **Accumulating engine replaces the if/else apply.** A per-voice modulation engine computes, per
   sample, a **summed** modulation offset **per target** from all active routings, then applies it
   **once** around each captured base value — reusing today's exact per-target application curves
   (Pitch = `2^x` octaves; Cutoff = `2^(3x)` clamped 20–20000; Resonance = `2^(1.5x)` clamped
   0.1–10; WT-Pos/Vowel/Fold = `base + 0.5x` clamped 0–1; Amplitude = tremolo `(1+x)/2`). The old
   single-target if/else in `SynthVoice.cpp:116-141,188-189` is gone.
2. **Sources decoupled from targets.** Routing is `{source, target, amount}`, not "the LFO's target".
   Source set v1 (append-only enum): **LFO 1, Envelope (ADSR), Velocity**. Target set v1 (append-only
   enum) = exactly today's seven: **Pitch, Amplitude, Filter Cutoff, Filter Resonance, WT Position,
   Formant Vowel, Wavefold Drive**. (Both sets extend by appending — LFO 2–4, Macro 1–4, Voice-Random,
   Evolution, Mod-Wheel, Keytrack, Pan, FM-Amount… are later stories.)
3. **Stacking works.** Two routings targeting the same destination (e.g. LFO 1 → Cutoff **and**
   Envelope → Cutoff) both contribute; the offsets sum before the single clamp/apply. This is the
   headline behaviour that proves the engine (it is impossible today).
4. **Generic routing slots as append-only params.** **4** fixed slots (decision 2026-07-14; more added
   later append-only), each three params: `modSlot{n}Source` (choice), `modSlot{n}Target` (choice
   **including "Off"** as index 0), `modSlot{n}Amount` (float **−1..+1**, bipolar, default 0). All
   appended at the end of `createLayout`; **no existing ID renamed/reordered**. A slot with
   Target = "Off" or Amount = 0 contributes nothing.
5. **Zero-regression default + legacy LFO back-compat.** With no matrix slots active, the default
   patch is **audibly identical** to before. The existing LFO module (WAVE/RATE/DEPTH/**TARGET**/
   SYNC) keeps working exactly as today: its `lfoOn/lfoTarget/lfoDepth` drive an **implicit LFO→target
   routing** fed into the same accumulating engine (byte-identical when it is the only contributor).
   _(Design fork — see "Open Design Questions": keep the LFO's built-in TARGET as an implicit routing
   [recommended, zero-regression] vs. fold LFO routing entirely into explicit matrix slots now.)_
6. **MOD MATRIX rack module.** A new module in the **MODULATION** zone lists the slots; each slot row
   = **Source combo · Target combo · Amount knob (bipolar)**. It has the uniform header (title · info ·
   reset ↺ · enable `modMatrixOn`); reset writes only the slot params to default (all Off/0).
   Disabled ⇒ engine ignores all explicit slots (the implicit legacy LFO routing still applies per
   AC5). Sizing is a judgement call (a taller class or wider footprint may be needed) — tune in-app
   with the user, honouring AD-2/AD-3.
7. **Mod rings generalized to multiple targets.** The ring system lights **every** knob whose
   `modTarget` currently receives modulation from a **periodic** source (LFO), not just one. Concretely:
   the editor computes, from the display-side LFO + active routings, the set of `(target, amount)`
   currently modulated, and `updateLiveFeed` accepts that set instead of a single target. With only the
   legacy LFO active the visible result is identical to today. _(Velocity/Envelope produce no idle-time
   ring — they need a sounding note — so idle rings still come only from the LFO; acceptable for v1.)_
8. **Persistence — append-only, interop-safe.** The slot params round-trip in `.synthy` as append-only
   fields (`ModSlot{n}Source` / `ModSlot{n}Target` / `ModSlot{n}Amount`, choice names via canonical
   `StringArray`s), and in DAW state via APVTS. **No `kFormatVersion` bump** (append-only). A preset
   without them — older build or the C# app — loads with all slots Off (missing ⇒ Off), so old presets
   and C# are unaffected. C# interop deprioritized (owed in `deferred-work.md`).
9. **RANDOM handling — EXCLUDE for v1** (decision 2026-07-14). Matrix slots + `modMatrixOn` are left
   untouched by RANDOM (like the `arpOn`/`glideOn` input-surfaces), so a random patch can't stack
   extreme routings and blow up the level. Revisit (bounded inclusion) in a later story.
10. **RT-safe; clean build; verified in the app.** No allocation/locking in the audio callback (slots
    are plain ints/floats read per block into a small fixed-size per-voice array). Verify by ear:
    default patch unchanged; route LFO→Cutoff via a slot (matches the old LFO-target behaviour); stack
    Envelope→Cutoff on top and hear both; route Velocity→Amplitude and hear dynamics; rings light on the
    modulated knobs.

## Tasks / Subtasks

- [ ] **Task 1 — Modulation engine (`Source/DSP/ModMatrix.h`, new header-only)** (AC: 1, 2, 3)
  - [ ] Define `enum class ModSource { LFO1, Envelope, Velocity }` and reuse/define a target enum. **Prefer reusing the existing `LFOTarget`** (`DSP/LFO.h:9-12`) as the target vocabulary so the apply curves map 1:1 — its order is append-only and already mirrors the seven targets (+ `Off`). Do NOT reorder it.
  - [ ] `ModMatrix` accumulates: given the current source values (LFO value, envelope value, velocity) and the N slot configs `{source, target, amount}`, produce a `float offset[targetCount]` summed across slots (+ the implicit legacy LFO routing per AC5). Header-only, no allocation, `setSampleRate` not needed (pure combine).
  - [ ] Keep it a plain combiner: the **application** of each summed offset (the `2^x` etc. curves + clamps) stays in `SynthVoice` where the base values live — the matrix only sums amounts per target.
- [ ] **Task 2 — Voice apply refactor (`SynthVoice.h/.cpp`)** (AC: 1, 3, 5)
  - [ ] Add per-voice slot storage: `struct ModSlot { int source; int target; float amount; }` `modSlots[N]` (+ setters/refs like `mixSrcA`), a `bool modMatrixOn`, and `float noteVelocity` (capture in `startNote`).
  - [ ] In `renderNextBlock`, after capturing the base values (`baseFrequencies/baseCutoff/baseReso/basePos/baseVowel/baseFold`, already at `SynthVoice.cpp:95-104`), compute per-target summed offsets each sample from: the LFO value (`lfo.process()`), the envelope value, velocity, weighted by the active slots + the implicit legacy LFO routing (`lfoTarget`/depth already in `lfo`).
  - [ ] **Replace** the if/else block (`SynthVoice.cpp:116-141`) and the tremolo line (`188-189`) with: apply `offset[Pitch]` to the osc/wt/sub frequency factor, `offset[Cutoff]`→cutoff (log), `offset[Reso]`→reso, `offset[WtPos]`→position, `offset[Vowel]`→vowel, `offset[Fold]`→fold, `offset[Amplitude]`→tremolo — each using **today's exact curve+clamp**, applied once. Default (only implicit LFO, one target) ⇒ byte-identical.
  - [ ] Envelope-as-source: read the same `envelope` value already used for gain (bipolar or 0..1 — document which). Velocity-as-source: 0..1 from note-on.
- [ ] **Task 3 — Params (`Parameters.h`)** (AC: 4, 5)
  - [ ] Add ID constants + helpers `modSlotSource(n)/modSlotTarget(n)/modSlotAmount(n)` (mirror the `oscFreq(i)` indexed-helper pattern) and `modMatrixOn`.
  - [ ] `createLayout`: append N×(2 `AudioParameterChoice` + 1 `AudioParameterFloat` −1..+1) + the `modMatrixOn` bool (default on). Source choice = {LFO 1, Envelope, Velocity}; Target choice = {Off, Pitch, Amplitude, Cutoff, Resonance, WT Position, Vowel, Wavefold} (Off at index 0). **Append only — never reorder existing params.**
  - [ ] `applyToVoice`: extend the signature (like the Epic-5 `int& mixSrcA` additions) to set each voice's `modSlots[n]` + `modMatrixOn` from the params. Mind the already-long signature (`Parameters.h:372-384`).
- [ ] **Task 4 — Processor wiring (`PluginProcessor.cpp`)** (AC: 1, 7)
  - [ ] Pass the new voice refs into `Parameters::applyToVoice(...)` (call site ~`PluginProcessor.cpp:254`).
  - [ ] Extend the display-side modulation feed: today `uiLfo` + `lfoDisplayValue` drive one ring target (`PluginProcessor.cpp:475-488`). Compute the set of currently-modulated `(target, amount)` from the display LFO + active slots for the ring UI (AC7). Keep it atomic-based, no locks.
- [ ] **Task 5 — MOD MATRIX module + generalized rings (`PluginEditor.cpp`, `ModuleDescriptor.h`, `Rack.cpp`, `ModuleFrame.cpp`)** (AC: 6, 7)
  - [ ] Add the MOD MATRIX descriptor to the MODULATION zone: N slot rows (Source combo, Target combo, Amount bipolar knob), `enableParam = modMatrixOn`, help id `modmatrix`. Pick a size class that fits the rows (may need a new tall `W{cols}H2/H3` table entry — AD-2 single-table rule).
  - [ ] Generalize the ring path: `ModTarget` set instead of a single `activeTarget`. `updateLiveFeed` takes a small fixed-size list of `(ModTarget, amount)`; `ModuleFrame` lights any knob whose `modTarget` is in the set (replace the `rk.target == activeTarget` equality at `ModuleFrame.cpp:436` with set membership). Keep repaint-only-on-change (NFR5).
  - [ ] Register `modMatrixOn` in RANDOM per AC9 (include-with-bounds or exclude — decide + note).
- [ ] **Task 6 — Persistence (`PresetIO.h`)** (AC: 8)
  - [ ] Add canonical `StringArray`s for the source + target choices (member names, no display spaces). `toVar`: write `ModSlot{n}Source/Target/Amount` + `ModMatrixOn`. `applyVar`: read with missing ⇒ Off/0 and `modMatrixOn` missing ⇒ true (the `jbool(..., rawB(...))` fallback pattern). Append-only; no format-version bump.
- [ ] **Task 7 — Help text (`Resources/EN/modmatrix.md`, `Resources/DE/modmatrix.md`)** (AC: 6)
  - [ ] Short EN + DE description (what a slot is: Source → Target, bipolar Amount; stacking). Rebuild the Help binary-data targets (see the `juce_add_binary_data` build note in project memory).
- [ ] **Task 8 — Verify** (AC: 10) — clean incremental build (`build/JASS_Standalone.vcxproj`, MSBuild/PowerShell; kill running `JASS.exe` first to avoid LNK1168); in-app: default patch identical → route LFO→Cutoff (matches old behaviour) → stack Envelope→Cutoff → Velocity→Amplitude → confirm rings. Standalone first; VST3 rebuild optional.

## Dev Notes

### The mechanism being replaced — `Source/Audio/SynthVoice.cpp:116-141, 188-189`
```cpp
// LFO modulation
float lfoValue = lfo.process();
auto lfoTarget = lfo.getTarget();
const double freqFactor = (lfoTarget == LFOTarget::Frequency) ? std::pow(2.0, lfoValue) : 1.0;
for (int i = 0; i < 3; ++i) oscillators[i].setFrequency(baseFrequencies[i] * ratio * freqFactor);
...
if (lfoTarget == LFOTarget::FilterCutoff)      filter.setCutoff(std::clamp(baseCutoff * std::pow(2.0, lfoValue*3.0), 20.0, 20000.0));
else if (lfoTarget == LFOTarget::WavetablePosition) wavetable.setPosition(std::clamp(basePos + lfoValue*0.5, 0.0, 1.0));
else if (lfoTarget == LFOTarget::FormantVowel)  formant.vowel = std::clamp(baseVowel + lfoValue*0.5, 0.0, 1.0);
else if (lfoTarget == LFOTarget::FilterResonance) filter.setResonance(std::clamp(baseReso * std::pow(2.0, lfoValue*1.5), 0.1, 10.0));
else if (lfoTarget == LFOTarget::WavefolderDrive) wavefolder.drive = std::clamp(baseFold + lfoValue*0.5, 0.0, 1.0);
// ...later, after the mix:
if (lfoTarget == LFOTarget::Amplitude) mixedSample *= (1.0f + lfoValue) * 0.5f;
```
**Refactor target:** compute `offset[target] = Σ slot.amount * sourceValue(slot.source)` (+ the implicit
LFO routing), then apply **each** target's summed offset **once** with the exact curve above. The curves
and clamps are the contract — keep them identical so the default patch does not drift. `lfoValue` becomes
one of several source values; `lfoTarget`/depth become the implicit routing (AC5).

### Source values
- **LFO 1:** `lfo.process()` (bipolar −1..+1 × depth), already per-voice, free-running phase per voice.
- **Envelope:** the same value `envelope.process()` returns for gain (`SynthVoice.cpp:197`). Decide unipolar (0..1) vs. bipolar; unipolar is the ADSR-as-mod norm. Note it advances once/sample already — read it, don't re-advance.
- **Velocity:** capture MIDI velocity (0..1) in `startNote`; constant across the note.

### Param → voice plumbing (`Parameters.h:372-467`)
`applyToVoice` already threads many refs (`mixSrcA/B`, `subOctave`, `adsrOn`, `mixModeOn`). Add the
slot array + `modMatrixOn` the same way; set them from `getRawParameterValue` (raw reads only). Mind the
signature length — group the slots behind a small helper or pass a pointer to the voice's slot array.

### Mod-ring generalization (`PluginEditor.cpp:417-452`, `Rack.cpp:422-425`, `ModuleFrame.cpp:428-436`)
Today: one `ModTarget activeT` from `lfoTarget`; `updateLiveFeed(active, activeT, value, ratio)`; a knob
lights iff `rk.target == activeTarget` (`ModuleFrame.cpp:436`). Change to a small fixed-capacity set of
`(ModTarget, amount)` and light iff the knob's `modTarget` is in the set. `ModTarget` enum lives at
`ModuleDescriptor.h:24-26` and mirrors `LFOTarget` (keep them in sync / append-only).

### Guardrails (from `project-context.md`)
- **APVTS single source of truth**; IDs only in `Parameters.h`, **append-only**, never renamed/renumbered.
- **RT-safety:** no alloc/lock in the callback; per-voice slot array is fixed-size, filled per block. Clamp defensively.
- **Signal-chain order is intentional** (osc mix → noise/karplus/wt/sub → wavefold → amplitude-LFO → filter → envelope → …). The matrix changes **what modulates** the existing stages, not their order. Amplitude modulation stays at its current point (`188-189`).
- **`.synthy` interop:** canonical enum strings = C# member names (no display spaces); missing field ⇒ default (Off/0, `modMatrixOn` ⇒ true). No `kFormatVersion` bump (append-only). C# owes matching fields — log in `deferred-work.md`.
- **Pitch model:** transpose ratio is applied per block then restored; frequency modulation multiplies the factor as today — keep it inside that model.

### Scope discipline
- **In scope (8.1):** engine + 3 sources + 7 existing targets + N slots + MOD MATRIX module + ring generalization + persistence. This is the foundation and must prove **stacking**.
- **Out of scope (follow-on 8.x, cheap once the engine exists):** LFO 2–4 (source append + module dup), Macros 1–4 + A/B morph, Per-Voice-Random/Drift source, Evolution module, new targets (Pan, FM-Amount, FX mixes), drag-&-drop matrix UI. See `docs/JASS_Ideen_Merge.md` §2–§3.

### Testing standards
No unit-test framework — verify by build + ear ([[feedback_ui_verification]]): (a) default patch byte/audibly
identical; (b) a single slot LFO→Cutoff reproduces the old LFO-target sweep; (c) LFO→Cutoff **+** Envelope→Cutoff
both audibly act (the new capability); (d) Velocity→Amplitude gives dynamics; (e) rings light on all modulated knobs.

### References
- [Source: _bmad-output/planning-artifacts/epics.md#Epic 8 / Story 8.1] (added this session)
- [Source: docs/JASS_Ideen_Merge.md §0–§2] — the "movement layer" rationale; matrix as the enabler.
- [Source: Source/Audio/SynthVoice.cpp:95-141,188-197] — apply block to replace + base-value capture.
- [Source: Source/Audio/Parameters.h:112-118,372-467] — LFO IDs + `applyToVoice`.
- [Source: Source/DSP/LFO.h:8-12] — `LFOWaveform`/`LFOTarget` (append-only target vocabulary to reuse).
- [Source: Source/UI/PluginEditor.cpp:417-452,740-743] — ring routing + LFO module descriptor.
- [Source: Source/UI/rack/ModuleFrame.cpp:428-436] — single-target ring matcher to generalize.
- [Source: Source/Audio/PresetIO.h:17-18,201-205,363-367] — LFO persistence pattern to mirror.
- [Source: _bmad-output/implementation-artifacts/5-1-selectable-mix-sources.md] — precedent for an append-only DSP change.

## Resolved Design Decisions (Michael, 2026-07-14)

All five forks decided before dev-story — implement exactly these:

1. **LFO built-in TARGET → KEEP as an implicit routing.** `lfoOn`/`lfoTarget`/`lfoDepth` stay on the LFO
   module and feed one implicit routing into the accumulating engine; matrix slots add explicit routings
   on top. Zero regression, old presets untouched. (Deprecating the LFO's own target is a possible later
   story, not now.)
2. **Slot count = 4.** Four `{Source, Target, Amount}` slots (12 appended params + `modMatrixOn`). More
   slots later are append-only.
3. **Source set v1 = LFO 1 + Envelope + Velocity.** Envelope is mandatory to demonstrate stacking on the
   same target; Velocity adds dynamics cheaply.
4. **RANDOM = EXCLUDE the matrix in v1** (slots + `modMatrixOn` untouched by RANDOM), like the
   `arpOn`/`glideOn` input-surfaces. Bounded inclusion can come later.
5. **MOD MATRIX = a rack module** in the MODULATION zone with slot rows (Source combo · Target combo ·
   bipolar Amount knob) + the uniform header/enable/info/reset + mod rings — a new taller size-class
   table entry is fine if needed (AD-2 single-table rule). Not a pop-out panel.

## Dev Agent Record

### Agent Model Used

claude-opus-4-8[1m]

### Debug Log References

### Completion Notes List

### File List
