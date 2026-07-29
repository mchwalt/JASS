# Story 10.6: Stereo displays — OSCILLOSCOPE + SPECTRUM tap the bus AFTER the master stages

Status: draft

<!-- Raised 2026-07-29 while ear-testing Story 10.4: the user expected the ROOM effect to show in
     the displays and (correctly) diagnosed why it cannot — the capture point predates the whole
     master-bus section. -->

## Story

As a user shaping the master bus,
I want the OSCILLOSCOPE and SPECTRUM to show the **stereo signal I actually hear**, so that the
master-bus stages (compressor, widener, Binaural/Kunstkopf, ROOM reflections) are visible, not
invisible, in the displays.

## Context

`WaveformCapture` is written in `processBlock` BEFORE the master-bus section (deliberate, Story 2.3:
"the scope shows the dry mono mix"). Everything after it — COMPRESSOR, Pseudo-Stereo widener,
Binaural/Kunstkopf rendering and the Story-10.4 ROOM stage — bypasses both displays. With the
engine now genuinely stereo at the output (Epic 10), a mono pre-bus tap no longer represents what
comes out of the headphones.

## Scope (to be refined before dev)

1. **Stereo capture, post-bus:** `WaveformCapture` (or a second instance) carries L+R, written after
   the master-bus stages. Decide: pre or post master VOLUME (leaning pre-volume, so the display does
   not collapse when the user pulls the volume down; note the old capture is pre-compressor too).
2. **OSCILLOSCOPE:** two traces (L/R, colour-separated — mind the red-green-colorblind rule: use
   blue/neutral pair, not red vs green). In the 1-channel modes (Mono/Pseudo-Stereo-off) L==R —
   draw a single trace to avoid doubled lines.
3. **SPECTRUM:** decide between two curves or the L/R **power sum** (power sum is honest — a plain
   L+R sample sum would show phantom cancellations the ear does not hear).
4. **RT safety:** capture stays lock-free/alloc-free (Epic 11 rules); WaveformCapture is sized once.
5. Regression: displays in Mono mode should look the same as today (modulo the tap point moving
   behind the compressor — decide whether that change is wanted or the compressor stays outside).

## Open decisions (user)

- Tap point: post-ROOM/pre-master-volume vs. true final output.
- SPECTRUM: two curves vs. power sum.
- Whether the old dry-mono view is worth keeping as a toggle (probably not — simplicity first).

## Dev Agent Record

_Not started — story recorded 2026-07-29 at the user's request during the 10.4 ear test._
