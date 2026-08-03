# Story 5.1: Selectable MIX MODE sources (A/B among OSC 1/2/3)

Status: done

<!-- Note: Validation is optional. Run validate-create-story for quality check before dev-story. -->

## Story

As a JASS sound designer,
I want to choose which two oscillators the MIX MODE (RingMod / FM) couples, instead of a fixed OSC1↔OSC2,
so that I can ring-mod / FM any pair (1-2, 1-3, 2-3) — consistent with the now freely-arrangeable modules.

## Acceptance Criteria

1. **Two source selectors.** MIX MODE exposes **Source A** and **Source B** combos, each choosing OSC 1 / OSC 2 / OSC 3. Defaults: A = OSC 1, B = OSC 2 (identical to today's behaviour).
2. **FM uses the selection.** In FM, A is the **modulator**, B is the **carrier** (B's frequency modulated by A); the remaining OSC is summed in plainly — same math as today, just generalized (`carrier + other`).
3. **RingMod uses the selection.** In RingMod, output = `oscA × oscB × 2 + other` (same math as today, generalized).
4. **Additive unchanged**; when MIX MODE is disabled (`mixModeOn` off) the OSCs are summed plainly regardless of A/B.
5. **Per-sample integrity.** Every oscillator advances **exactly once per sample** in every branch (no pitch/phase drift). If A == B, fall back to plain additive for that block (no double-advance).
6. **Back-compat + append-only.** New params `mixSrcA`/`mixSrcB` are **append-only** (defaults 0/1). A preset without them loads with the default 1↔2 coupling (missing ⇒ default). No `kFormatVersion` change required for this (append-only field); C# interop deprioritized.
7. **Dim predicate follows selection.** MIX MODE's lit/dimmed state = the two **selected** OSCs are both enabled (generalizes the old `osc1 && osc2`).
8. **RT-safe; clean build**; verify in the app: set A/B to different pairs, hear RingMod/FM on that pair; the third OSC still sounds; default patch unchanged.

## Tasks / Subtasks

- [ ] **Task 1 — Params (`Parameters.h`)**
  - [ ] Add ID constants `mixSrcA`, `mixSrcB`.
  - [ ] `createLayout`: two `AudioParameterChoice` `{ "OSC 1","OSC 2","OSC 3" }`, defaults 0 and 1 (append at the end — do not reorder existing params).
  - [ ] `applyToVoice`: set the voice's `mixSrcA`/`mixSrcB` ints from these params (add two `int&` params to the signature, like `subOctave`).
- [ ] **Task 2 — Voice (`SynthVoice.h/.cpp`)**
  - [ ] Add `int mixSrcA = 0, mixSrcB = 1;` + `int& getMixSrcARef()`, `int& getMixSrcBRef()`.
  - [ ] Generalize the mix block: `a=mixSrcA, b=mixSrcB` (clamp 0..2), `o = 3 - a - b` (the third OSC). FM: mod=osc[a], `fmOffset = mod * osc[a].getFrequency() * 2.0`, carrier=osc[b].nextSample(fmOffset), other=osc[o]; `mixedSample = carrier + other`. RingMod: `osc[a]*osc[b]*2 + osc[o]`. If `a==b` (or mixMode disabled), plain additive. **Each osc advanced exactly once.**
- [ ] **Task 3 — Processor wiring (`PluginProcessor.cpp`)**
  - [ ] Pass `voice->getMixSrcARef(), voice->getMixSrcBRef()` into the `Parameters::applyToVoice(...)` call.
- [ ] **Task 4 — UI (`PluginEditor.cpp buildSampleRack`)**
  - [ ] MIX MODE module: add two combos `Source A` / `Source B` (items `{ "OSC 1","OSC 2","OSC 3" }`, bound to `mixSrcA`/`mixSrcB`); size **XXS → S** (MODE + A + B = 3 controls).
  - [ ] Generalize `enabledWhen`: read `mixSrcA`/`mixSrcB` + the three `oscOn(i)` and return `oscOn[selA] && oscOn[selB]` (raw APVTS reads only, AD-9).
- [ ] **Task 5 — Persistence (`PresetIO.h`)**
  - [ ] `toVar`: write `"MixSrcA"`/`"MixSrcB"` (choice names via `rawChoice` over `{ "OSC 1","OSC 2","OSC 3" }`, or int). `applyVar`: read with missing ⇒ current default (`setChoice`/`setRaw`). Append-only.
- [ ] **Task 6 — Verify** (clean build + running app; default patch identical; pick 1-3 / 2-3 pairs and confirm the coupling moves; third OSC audible).

## Dev Notes

### Current DSP (the thing being generalized) — `Source/Audio/SynthVoice.cpp` ~110-131
```cpp
if (mixModeOn && mixMode == MixMode::FM) {          // OSC1 modulates OSC2, OSC3 additive
    float modulator = oscillators[0].nextSample();
    double fmOffset = modulator * oscillators[0].getFrequency() * 2.0;
    float carrier   = oscillators[1].nextSample(fmOffset);
    float s2        = oscillators[2].nextSample();
    mixedSample = carrier + s2;
}
else if (mixModeOn && mixMode == MixMode::RingMod) { // OSC1 × OSC2 + OSC3
    float s0 = oscillators[0].nextSample();
    float s1 = oscillators[1].nextSample();
    float s2 = oscillators[2].nextSample();
    mixedSample = s0 * s1 * 2.0f + s2;
}
else { for (auto& osc : oscillators) mixedSample += osc.nextSample(); }
```
Generalize by replacing the fixed indices 0/1/2 with `a`, `b`, `o = 3 - a - b`. With defaults a=0,b=1 ⇒ o=2 ⇒ **byte-identical** to today. `o = 3 - a - b` yields the missing index for any distinct a,b ∈ {0,1,2}. Guard `a != b` (else plain additive) so no oscillator is advanced twice.

### Param → voice plumbing
`PluginProcessor.cpp:254` calls `Parameters::applyToVoice(apvts, …, voice->getMixMode(), voice->getSubOsc(), voice->getSubOctaveRef(), voice->getAdsrOnRef(), voice->getMixModeOnRef())`. `applyToVoice` (Parameters.h ~296) sets each from `getRawParameterValue`. Add `int& mixSrcA, int& mixSrcB` to the signature + the two setters, and pass the new voice refs at the call site.

### MIX MODE module today — `buildSampleRack` (PluginEditor.cpp)
`mix.sizeClass = XXS; mix.enableParam = mixModeOn; mix.body = { Combo(mixMode,"MODE",{Additive,RingMod,FM}) }; mix.enabledWhen = [o1,o2]{ return o1&&o2; };`. Add the two combos, bump to **S**, and rewrite `enabledWhen` to read the selected sources.

### Guardrails
- **First real audio/DSP change in this project** (the redesign was UI-only). Keep it surgical: only the OSC-mix block + two append-only params. No change to the signal chain, filters, effects, or the noise/karplus/wavetable/sub path.
- **RT-safety (NFR2):** no allocation/locking in the audio callback; the mix indices are plain ints read per block. Clamp `a`,`b` to 0..2 defensively.
- **Append-only (NFR3-spirit):** append the two params at the end of `createLayout`; never reorder/rename existing IDs. Missing preset fields ⇒ defaults (1↔2). Format stays loadable by older builds.
- **Default equivalence:** with A=0/B=1 the audio must be identical to before (regression check).

### Testing standards
No unit tests — verify by ear + build: default patch unchanged; switch A/B to 1-3 and 2-3 and confirm the RingMod/FM coupling follows; the un-selected OSC still sounds.

### References
- [Source: _bmad-output/planning-artifacts/epics.md#Epic 5 / Story 5.1]
- [Source: PRD FR21]
- [Source: Source/Audio/SynthVoice.cpp] — mix block to generalize.
- [Source: Source/Audio/Parameters.h] — `createLayout` + `applyToVoice`.
- [Source: Source/UI/PluginEditor.cpp#buildSampleRack] — MIX MODE descriptor.

## Dev Agent Record

### Agent Model Used

claude-opus-4-8[1m]

### Debug Log References

- Clean incremental Release build (x64); `SynthVoice.cpp`, `PluginProcessor.cpp`, `PluginEditor.cpp` recompiled.

### Completion Notes List

- **Params:** `mixSrcA`/`mixSrcB` `AudioParameterChoice {OSC 1,2,3}` appended at end of `createLayout` (defaults 0/1). `applyToVoice` gained `int& mixSrcA, int& mixSrcB` and sets them from the params.
- **Voice:** `SynthVoice` holds `int mixSrcA=0, mixSrcB=1` (+ refs). Mix block generalized: `a=jlimit(0,2,mixSrcA)`, `b=...B`, `o=3-a-b`. FM: osc[a] modulates osc[b] (carrier), osc[o] added → `carrier+other`. RingMod: `osc[a]*osc[b]*2 + osc[o]`. `a==b` or mixMode-off ⇒ plain additive. Every oscillator advanced exactly once per branch (defaults 0/1/2 ⇒ byte-identical to before).
- **Processor:** passes `voice->getMixSrcARef()/BRef()` into `applyToVoice`.
- **UI:** MIX MODE module XXS→**S**, body now `MODE + SRC A + SRC B` combos. `enabledWhen` reads `mixSrcA/B` + the 3 `oscOn` (via a captured `std::array` of raw pointers) → lit when both selected sources are enabled.
- **Persistence:** append-only `.synthy` fields `MixSrcA`/`MixSrcB` (choice names via `kMixSrc`); missing ⇒ default 0/1 (older presets keep the 1↔2 coupling). No format-version change needed (append-only).
- **First real DSP change**, kept surgical (only the OSC-mix block + 2 params). Signal chain / effects / noise-karplus-wavetable-sub path untouched. C# interop deprioritized (per user).
- **Option B + rename → "CROSS MOD" (user decision 2026-07-11).** "Additive" is no longer a MODE — the module's MODE combo is `{RingMod, FM}` only, and "no coupling" = the module **disabled** (enable toggle off ⇒ plain additive sum), matching the Filter/LFO enable+trimmed-combo pattern (fixes the A/B-are-meaningless-in-Additive problem). `MixMode` enum trimmed to `{RingMod, FM}`; `mixMode` param choices `{RingMod, FM}`; `mixModeOn` default **false** (additive default). `.synthy` keeps `Additive/RingMod/FM` via `choiceOrOff` (on-disk unchanged; old presets round-trip; a Story-2.4-era explicit `MixModeOn=false` is honoured on read). Module renamed **MIX MODE → CROSS MOD** (display title only; internal `id="mixmode"` + param IDs unchanged ⇒ RackLayout + preset keys stable).
- **A==B prevented (user decision 2026-07-11).** SRC A and SRC B are kept distinct: a `parameterChanged` listener on `mixSrcA`/`mixSrcB` (processor is now an `APVTS::Listener`, registered in both standalone + plugin, reentrancy-guarded) bumps the OTHER selector to a free OSC when they'd collide. MIX MODE stays "couple two different oscillators"; the DSP `a==b` additive fallback remains as a safety net but is effectively unreachable. **Self-FM/feedback-FM deferred** to a dedicated feature with its own feedback-amount control (`docs/Feature_Ideas.md` backlog) — no similar module exists today. Also logged there: per-module online-help popup.

### File List

- `Source/Audio/Parameters.h` (`mixSrcA`/`mixSrcB` IDs + params + `applyToVoice` signature/body)
- `Source/Audio/SynthVoice.h` (members + refs)
- `Source/Audio/SynthVoice.cpp` (generalized FM/RingMod mix block)
- `Source/PluginProcessor.cpp` (pass new refs to `applyToVoice`)
- `Source/UI/PluginEditor.cpp` (MIX MODE 2 combos, size S, generalized `enabledWhen`; `<array>`)
- `Source/Audio/PresetIO.h` (`MixSrcA`/`MixSrcB` fields + `kMixSrc`)
