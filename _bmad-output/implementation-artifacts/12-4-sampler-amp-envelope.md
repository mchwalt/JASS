# Story 12.4: SAMPLER amp envelope (per-zone release)

Status: draft (captured 2026-08-04, from the piano listening sessions)

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

- Metallic clicking on SOME SplendidGrand keys — CONFIRMED source-material defect: `FF A2.flac`
  (serves MIDI 57) is a bad recording; nothing documented upstream. Worked around locally by
  deleting that region and widening the A#2 zone (lokey 57) in the flattened sfz — the generic
  lesson for the help text: a broken zone in any imported set is fixed by editing the .sfz.
- **Fast playing still sounds UNNATURAL on piano (user, 2026-08-04, after the declick fixes):**
  the clicks are gone, but a steal/retrigger still CUTS the previous ring abruptly — a real
  piano lets the string keep ringing under a re-strike. Belongs in this story's design pass:
  (a) SAMPLER release ramp (the core scope) already softens steals; (b) evaluate same-note
  overlap (JUCE reuses the voice playing the same note — letting the old note ring in its own
  voice needs a Synthesiser-level decision) and steal-victim choice (prefer quietest voice).

## References

- [Source: _bmad-output/implementation-artifacts/12-2-sampler-multisampling.md] — sfz parser
  (where ampeg_release would be read), Entry/zone plumbing
- [Source: _bmad-output/implementation-artifacts/12-3-pitch-time-decoupling.md] — SamplePlayer
  structure (where the release ramp would live), declick precedent
- [Source: Source/DSP/SamplePlayer.h, Source/DSP/SampleMapping.h]
