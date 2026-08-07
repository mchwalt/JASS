# Story 12.4: SAMPLER amp envelope (per-zone release)

Status: **done** — implemented 2026-08-04, user listening test passed 2026-08-07 (fast playing /
re-strike, tail with the ENVELOPE module off, REL fallback on the folder sets; earlier that week:
comb-filter tone gone, Salamander tail, A3/A4 quirk accepted). **Sustain pedal (scope item 4) was
NOT verified and will not be** — the user has no pedal. Nothing was built for it either: JUCE
holds the note-offs while CC64 is down, so the release ramp simply starts at pedal-up.

<!-- User insight while playing the Iowa/Splendid pianos: "vermutlich braucht man für Piano
     separate ADSR … die eine Standard-ADSR-Hüllkurve taugt für diesen komplexen Klang nicht."
     Confirmed by the source material itself: Splendid's regions carry ampeg_release= values
     that our minimal parser deliberately ignores. -->

## Story

As a player,
I want the SAMPLER to fade released notes with its OWN release time (per zone where the .sfz
provides one),
so that sampled instruments ring out naturally without fighting the global ADSR — whose settings
apply to every generator at once and to sounds whose attack/decay is already baked into the
recording.

## Scope sketch (to be contexted before dev)

1. **What a piano actually needs from an envelope:** attack/decay/sustain live in the RECORDING;
   only RELEASE (fade on note-off) is genuinely the sampler's job. So this is NOT a second full
   ADSR — one per-voice release ramp inside `SamplePlayer`, applied after note-off, independent
   of the global ADSR (which stays what it is for the synth generators).
2. **Sources of the release time:** per-zone `ampeg_release=` from the .sfz (opcode already in
   the files we import — currently ignored), falling back to a new SAMPLER **REL** knob
   (append-only param) for folder/single-sample sets.
3. **Interaction with the global ADSR:** decide + document. Simplest defensible rule: the
   sampler voice uses max(sampler release, ADSR release) — or the SAMPLER release simply rides
   inside the existing voice gain like the baked-in envelope does. Needs a short design pass.
4. **Sustain pedal:** already works (JUCE handles CC64 — held notes ignore their note-off);
   verify with a MIDI pedal and mention it in the help text. Una corda / sympathetic resonance /
   release samples are explicitly OUT of scope (that way lies the sister-project ghost of 12.1).
5. Append-only params, FormatVersion 6, help EN/DE, CHANGELOG.

## Open investigation (parked)

- Metallic tone on SplendidGrand A3/A4 (MIDI 57/69) — **SOLVED 2026-08-04, and every earlier
  diagnosis was wrong.** Elimination chain (user's ear + computed measurements, scratchpad
  stoerton*.py + flacdec.py, a bit-exact pure-Python FLAC decoder): source files clean (Audacity)
  → resampling 44.1→48k clean (full-spectrum Hermite-vs-sinc, no images >6 dB; user also
  reproduced at 44.1k host) → AppData copies hash-identical → JUCE FLAC decode exonerated (WAV
  test set from our own decoder sounded identical) → **culprit: the ±0.5 sub-source spread in
  the gain-based STEREO-PAN mode.** Equal-power at ±0.5 leaks 38% of the opposite mic channel
  into each ear; the coherent sum combs the recording key-dependently — measured ripple matched
  the user's per-key perception exactly (Splendid A3/A4: 2.9 dB RMS, ±6 dB peaks on the loudest
  partials; control C4: 1.1 dB; Salamander: 1.7 dB "only slight"). Fix in applyToVoice: stereo
  sets spread ±1.0 (hard L/R, PAN = balance) in gain modes; Binaural/Kunstkopf keep ±0.5 (ITD/
  HRIR decorrelate — no coherent comb). Mono/Pseudo-Stereo still mono-sum by design (documented).
  LESSON for future sound bugs: verify the SIGNAL PATH end-to-end before blaming material —
  and a "measurement" that searches only at predicted frequencies can acquit a guilty party
  (piano partials are inharmonic; the first Hermite check missed sideband search windows).
- **Fast playing still sounds UNNATURAL on piano (user, 2026-08-04, after the declick fixes):**
  the clicks are gone, but a steal/retrigger still CUTS the previous ring abruptly — a real
  piano lets the string keep ringing under a re-strike. Belongs in this story's design pass:
  (a) SAMPLER release ramp (the core scope) already softens steals; (b) evaluate same-note
  overlap (JUCE reuses the voice playing the same note — letting the old note ring in its own
  voice needs a Synthesiser-level decision) and steal-victim choice (prefer quietest voice).

## Implementation record (2026-08-04)

Design decisions taken (scope items 1–4 above):

1. **Ramp, not a second ADSR:** one per-voice exponential release ramp inside `SamplePlayer`
   (`gateOff()` → `relGain *= relCoef` per output sample; time-to-−60 dB reading of
   `ampeg_release`, floor −80 dB then the voice's sampler stops reading). Applied in BOTH modes
   by folding `relGain` into the output gain; in stretch mode it multiplies the engine OUTPUT
   (the input feed keeps walking), and reaching the floor also cancels a running drain.
2. **Sources:** per-zone `ampeg_release` (parser + `Entry` + `SampleZone.releaseSeconds`;
   values ≤ 0 or unparsable ⇒ unset, capped at 30 s) wins; new append-only param
   `samplerRelease` ("REL", 0–8 s, skew 0.5, **default 0 = OFF**) is the fallback. Default OFF
   keeps every old preset bit-identical (missing ⇒ default).
3. **Global ADSR interaction — decided:** the ramp rides INSIDE the voice gain; the global ADSR
   keeps multiplying on top and its release is the audible CEILING. No max()-magic, no per-voice
   envelope mutation (globals must not bend for wish scenarios). Documented in help EN/DE:
   sampled instruments want A 0 / D 0 / S max / R ≥ the longest fade. Consequence accepted:
   released voices stay allocated until the ADSR goes Idle — but JUCE's steal policy prefers
   released voices, and those are now ALREADY fading, so steals land on decaying low-level
   material (the fast-playing "hard cut" complaint dies with the same mechanism).
   **Amendment (same day, user report):** with the ENVELOPE module OFF the 10 ms bypass gate
   cut the tail at note-off — the gate is now HELD OPEN while `sampler.isRingingOut()` and
   closes once the fade is spent (`samplerTailHold` in SynthVoice; checked per block). So the
   SIMPLEST piano setup is ENVELOPE OFF — the sampler governs its whole tail itself. Trade-off
   (documented): other generators in an ADSR-off preset sustain under the tail — irrelevant in
   practice, a sampled instrument is normally the only generator then.
4. **Sustain pedal:** nothing to do — JUCE holds note-offs while CC64 is down, so `gateOff()`
   simply arrives at pedal-up. Mentioned in the help text.

Same-note overlap (open investigation item b): JUCE's `noteOn` already tail-offs the old
same-note voice and starts a fresh one — with the ramp the old strike now rings down under the
new one, which IS the natural re-strike behaviour. No Synthesiser-level change needed.
Steal-victim choice: unchanged (JUCE prefers released voices — now the right victims); revisit
only if the user still hears churn (Story 13.1 territory).

Touched: `SampleMapping.h` (parser + Entry), `SampleBank.h` (zone field), `SamplePlayer.h`
(ramp), `SynthVoice.cpp` (gateOff wiring), `SamplerSpecs.h` + `Parameters.h` (param),
`PluginEditor.cpp` (REL knob), help EN/DE, CHANGELOG.

## References

- [Source: _bmad-output/implementation-artifacts/12-2-sampler-multisampling.md] — sfz parser
  (where ampeg_release would be read), Entry/zone plumbing
- [Source: _bmad-output/implementation-artifacts/12-3-pitch-time-decoupling.md] — SamplePlayer
  structure (where the release ramp would live), declick precedent
- [Source: Source/DSP/SamplePlayer.h, Source/DSP/SampleMapping.h]
