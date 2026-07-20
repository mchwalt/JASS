# Story 8.2: Mod-Matrix target expansion (per-voice FX & generator targets)

Status: in-review

<!-- Follow-on to Epic 8 (Modulation Matrix), which explicitly reserved "later stories add …
     new targets (Pan, FM-Amount, FX mixes) — each just another target on the matrix". -->

## Story

As a sound designer,
I want more **MOD MATRIX destinations** — the per-voice effects and a couple of generator params — so I can modulate delay, reverb, chorus, distortion, bitcrush, the sub level and oscillator detune, not just pitch/filter/wavetable.

## Context / Decision

- **All new targets are per-voice.** Investigation (2026-07-20) confirmed DELAY, REVERB, CHORUS, DISTORTION, BITCRUSH are processed **inside `SynthVoice::renderNextBlock`** (per voice), not as a global master stage — the processor's `applyToVoice` only *configures* each voice's effect objects. So these targets use the **existing per-voice mechanism** (same as FilterCutoff), no new global mod path needed. Truly-global params (Master Volume, Stereo Width) are excluded (they'd need a separate block-level path) and Pan does not exist (mono engine → pseudo-stereo).
- **Zone boundary clarified** in the online help (zone-modulation / zone-processing, EN+DE): audio-vs-control test; the MOD MATRIX is the bridge that lets MODULATION drive PROCESSING knobs. These new targets deliberately strengthen that bridge.

## Acceptance Criteria

1. **8 new DEST targets, append-only:** Delay Time, Delay Mix, Reverb Mix, Chorus Depth, Dist Drive, Bitcrush, Sub Level, Detune. Added at the END of the target vocabulary so existing preset/DAW indices never move.
2. **Kept in sync across the three lists (identical order):** `LFOTarget` enum (`Source/DSP/LFO.h`), `kLfoTarget` persist strings (`Source/Audio/PresetIO.h`), `tgt` display labels (`Source/Modules/ModMatrixSpecs.h`). `ModMatrixConfig::kNumTargets` bumped 8 → 16.
3. **Applied per-voice** in `SynthVoice`: each target modulates around a captured base value, clamped to the parameter's range, gated by `tActive` (unrouted targets untouched → no behaviour change), and restored after the block. Only audible when the owning effect/generator is enabled.
4. **Detune** modulates all three oscillators' unison detune together (needs a new `Oscillator::getDetuneAmount()` getter; base cents = `uniDetune × 100`).
5. **RANDOM unchanged** — still draws from the original 7 targets only (avoids wild delay/reverb sweeps on randomize).
6. **No preset regression** — old presets' target indices (1–7) still resolve; the hidden per-LFO `lfoTarget` (migration-only, 7 choices) never receives a new-target value.
7. **Zone help updated** (EN+DE) with the modulation-vs-processing distinction.
8. Verified by build + running app (no unit tests — [[feedback_ui_verification]]).

## Dev Notes

- Scales chosen per target (bipolar offset × scale, added to base, clamped): Delay Time ±0.2 s [0.01,2.0]; Delay/Reverb Mix, Dist Drive, Bitcrush Mix, Sub Level ±0.5 [0,1]; Chorus Depth ±0.01 [0,0.05]; Detune ±50 cents [0,100].
- Effect objects expose public fields (`delay.time`, `reverb.mix`, …) — set directly; no setters needed. Sub level = `subOsc` amplitude.
- Per-voice delay-time modulation warbles the delay line (chorus-like) — an accepted creative effect.
- Files: `Source/DSP/LFO.h`, `Source/DSP/ModMatrix.h`, `Source/DSP/Oscillator.h`, `Source/Audio/PresetIO.h`, `Source/Modules/ModMatrixSpecs.h`, `Source/Audio/SynthVoice.cpp`, `Resources/{EN,DE}/zone-{modulation,processing}.md`.
- Delivered on branch `develop` (no push/merge — author's call).

## Dev Agent Record

### Agent Model Used
claude-opus-4-8[1m] (2026-07-20)

### Completion Notes List
- 8 new per-voice targets implemented + applied in `SynthVoice` (tActive-gated, base-captured, restored).
- **Ring animation**: `modTarget` set on each target's knob spec (Delay TIME/MIX, Reverb MIX, Chorus DEPTH, Dist DRIVE, Bitcrush MIX, Sub LEVEL, OSC DETUNE → all 3 OSCs).
- **Auto-enable/disable** (symmetric, "with memory") extended to the FX targets via `matrixTargetEnableParam` + the `managed[]` list. **OscDetune deliberately ring-only** (no auto-toggle — it spans the core OSC 1-3).
- **Generic single source** `Source/DSP/ModTargets.h` (X-macro `JASS_MOD_TARGETS`): generates the `LFOTarget` enum, `kCount`, persist strings (PresetIO `kLfoTarget`), DEST labels (ModMatrixSpecs), and the enable-param map (PluginProcessor). `rack::ModTarget` is now a type alias for `LFOTarget`; `LiveModFeed` and `ModMatrixConfig::kNumTargets` derive from `kCount`. Adding a target = ONE macro line + one object-specific apply block in SynthVoice.
- **Demo preset** `DemoPresets/FX Motion.jass` — pad routing 4 LFOs → Delay Mix, Reverb Mix, Chorus Depth, Detune (+ Cutoff). Embedded + seeds on first run (verified seeded to AppData).
- Clean rebuild green; demo seeded. Running-app audio/ring verification pending with the user.

Delivered on branch `develop` (no push/merge — author's call).

### File List
- `Source/DSP/LFO.h`, `Source/DSP/ModMatrix.h`, `Source/DSP/Oscillator.h`
- `Source/Audio/PresetIO.h`, `Source/Audio/SynthVoice.cpp`
- `Source/Modules/ModMatrixSpecs.h`
- `Resources/EN/zone-modulation.md`, `Resources/DE/zone-modulation.md`
- `Resources/EN/zone-processing.md`, `Resources/DE/zone-processing.md`
