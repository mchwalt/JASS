# DemoPresets

Version-controlled example presets that showcase JASS features. Unlike the user's
working presets (which live in `%AppData%\Roaming\JASS\Presets`), these ship with
the repository so they can be shared and re-seeded.

To try one now, copy the `.jass` file into `%AppData%\Roaming\JASS\Presets\` and
pick it from the preset menu in the app. They are also pre-assigned to the PRESETS
quick-access bank on **F1–F5** (in the order listed below).

- **Matrix Demo.jass** (F1) — shows the modulation matrix (Story 8.1): the LFO, the
  Envelope and Velocity all drive the filter Cutoff at once (stacking). Play the
  keyboard with varying strength to hear Velocity and the filter envelope; the drone
  shows the LFO wah.
- **Matrix Demo 2.jass** (F2) — a multi-LFO evolving pad: LFO 1 → Cutoff (slow wah),
  LFO 2 → Resonance (very slow breathing peak) and LFO 3 → Pitch (fast, subtle
  vibrato) all run at once, plus Envelope → Cutoff for a per-note filter attack.
  Three independent LFOs move three different targets — hold a chord to hear them
  drift against each other.
- **FX Motion.jass** (F3) — an FX-heavy evolving pad where four LFOs each breathe a
  different effect: LFO 1 → Delay Mix, LFO 2 → Reverb Mix, LFO 3 → Chorus Depth,
  LFO 4 → Detune, plus LFO 1 → Cutoff. Hold a chord and let the wet effects drift.
- **Helikopter.jass** (F4) — LFO 1 → Amplitude at a fast rate: the classic chopping
  "rotor" tremolo. A one-routing demo of amplitude modulation.
- **Matrix Showcase.jass** (F5) — the full tour of the reworked matrix in one patch
  (8 slots, all source types): **per-oscillator** vibrato on OSC 2 only (LFO 4 →
  OSC 2 · FREQ) while OSC 1 stays steady, self-FM movement (LFO 1 → OSC 1 · FB),
  **global** slow detune drift (LFO 1 → Alle OSC · DETUNE), a **stacked** filter
  opening (Envelope + Velocity → FILTER · CUTOFF), and evolving space via the new FX
  targets (LFO 2 → REVERB · ROOM, LFO 3 → CHORUS · RATE, LFO 2 → DELAY · MIX). A lush
  pad that keeps moving on a held chord.

These ship embedded in the binary and are seeded into the user's Presets folder on
first run if missing (`PresetIO::seedDemoPresets`), so every user gets them.
