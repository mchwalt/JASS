# DemoPresets

Version-controlled example presets that showcase JASS features. Unlike the user's
working presets (which live in `%AppData%\Roaming\JASS\Presets`), these ship with
the repository so they can be shared and re-seeded.

To try one now, copy the `.jass` file into `%AppData%\Roaming\JASS\Presets\` and
pick it from the preset menu in the app.

- **Matrix Demo.jass** — shows the modulation matrix (Story 8.1): the LFO, the
  Envelope and Velocity all drive the filter Cutoff at once (stacking). Play the
  keyboard with varying strength to hear Velocity and the filter envelope; the drone
  shows the LFO wah.
- **Matrix Demo 2.jass** — a multi-LFO evolving pad: LFO 1 → Cutoff (slow wah),
  LFO 2 → Resonance (very slow breathing peak) and LFO 3 → Pitch (fast, subtle
  vibrato) all run at once, plus Envelope → Cutoff for a per-note filter attack.
  Three independent LFOs move three different targets — hold a chord to hear them
  drift against each other.

These ship embedded in the binary and are seeded into the user's Presets folder on
first run if missing (`PresetIO::seedDemoPresets`), so every user gets them.
