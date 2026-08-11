# Story 15.5: The figure keeps running

Status: **done** — raised by the maintainer 2026-08-11: "STEP SEQ läuft los mit der ersten gedrückten
Taste, solange, bis die nächste Taste gedrückt wird oder die Oktave erhöht oder erniedrigt wird. das
entbindet den Spieler, eine Taste dauernd gedrückt halten zu müssen." Extended the same day by
"Leertaste stoppt das Spiel" and, after loading `DAF Beat` and hearing only the drone, by "wäre es
möglich mit einem gelatchten C3 zu starten — wir machen es gleich richtig".

## Story

As someone playing over a sequenced figure,
I want the pattern to keep running once I have started it,
so that both my hands are free for everything else the instrument can do.

## Why

The figure ran only while a key was held — the sequencer read the lowest held note as its root every
block, and nothing held meant nothing playing. That is fine for auditioning a pattern and useless for
playing over one: a bass line that stops when you reach for a filter knob is not a bass line.

## Acceptance Criteria

1. **The first key latches.** The pattern keeps running after the key is released.
2. **A new key moves it** to that root, mid-pattern; it does not restart.
3. **The octave keys shift it** by ±12. There is no held key left for `retuneSoundingComputerKeys` to
   move, so the editor hands the shift to the latched root directly.
4. **SPACE stops it**, ranked below "SPACE is a rest while recording a figure" (15.4) and above the
   Karplus pluck, so it only claims the key when there is a figure to stop.
5. **Switching the module off stops it and forgets the root** — the module's switch stays the
   transport.
6. **The on-screen keyboard shows the note the pattern plays**, in the MODULATION colour, so it reads
   apart from the key the player is holding.
7. **A patch saved while a figure runs comes back running**, on the same note: new preset field
   `StepSeq.LatchRoot`. Absent ⇒ silent on load *and* clears whatever the previous patch was playing.
8. No new parameter, no `FormatVersion` bump (the format is append-only, missing ⇒ default).

## Dev Notes

- The latch is one atomic in the processor. `processBlock` still finds the lowest held note; a found
  note overwrites the latch, and the LATCH is what plays. Everything downstream (quantised entry
  against PERC, the release-on-nothing-playing path) keys off the latched value.
- **The sequencer's notes must stay out of `MidiKeyboardState`.** Putting them in would have lit the
  on-screen keyboard for free — and the root search reads that same state, so the pattern would have
  re-rooted itself on its own output, transposing away under the player's hands. The processor
  mirrors the sounding note into an atomic and `FillWidthKeyboard` paints it; nothing sounds from it.
- `StepSeq.LatchRoot` is **not** a parameter: no knob, no meaningful automation, and as a parameter it
  would show up in RANDOM and as a modulation target. It reaches PresetIO through hooks, the same way
  the background sample loader does (`PresetIO::seqLatchRoot` / `applySeqLatchRoot`, both cleared in
  the processor's destructor since they capture `this`).
- Starting a latch also ends the auto-play drone: the instrument is playing now, and a held C4 under a
  bass figure is exactly the confusion this removes — that drone was what the maintainer heard when
  `DAF Beat` seemed to do nothing.
- App STARTUP is deliberately exempt: restoring the last session stays silent, or JASS would begin
  playing the moment it is opened.
- Help texts `Resources/{EN,DE}/stepseq.md` and the README rewritten — "hold a note and the pattern
  runs" was no longer true.
