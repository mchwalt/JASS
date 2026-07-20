# JASS `.jass` Preset Format (nested, v4)

JASS stores patches as **JSON** files with the extension `.jass`. The format is
**nested** (one object per module) and versioned; the current version is
**`FormatVersion: 4`**.

- **Format:** JSON, UTF‑8. File extension `.jass`.
- **Canonical source:** each module declares its own parameters and JSON shape in
  `Source/Modules/<Name>Specs.h`; the serializer walks those specs
  (`Modules::writeState` / `readState`), so the spec files — not this document —
  are the single source of truth for field names. This doc describes the
  *structure*.
- **Serializer:** `Source/Audio/PresetIO.h`.

## Locations (`%AppData%\Roaming\JASS\`)

| Purpose | Path |
|---|---|
| Root | `%AppData%\Roaming\JASS\` |
| Named presets | `%AppData%\Roaming\JASS\Presets\*.jass` |
| **Live state** | `%AppData%\Roaming\JASS\LiveState.jass` |
| Loaded WAV wavetables | `%AppData%\Roaming\JASS\Wavetables\*.wav` |

### LiveState
The standalone app **auto‑loads** `LiveState.jass` on startup and **auto‑saves**
it on every change (debounced ~1.5 s) and on exit, so you resume exactly where
you left off. As a **VST3** in a host (e.g. REAPER) the host owns project state
and LiveState is left untouched.

### Auto‑migration (one‑time)
On first launch JASS checks for the legacy `%AppData%\Synthy` folder and, if the
new `%AppData%\JASS` folder does not yet exist, copies presets/LiveState across
(`migrateLegacyAppData`). Old flat `.synthy` files are converted to the current
nested format on load.

## Structure

```jsonc
{
  "FormatVersion": 4,
  "Name": "Matrix Demo",
  "Modified": false,        // LiveState only: true = unsaved working state (header shows "Current State")

  // One nested object per module (spec-driven; field names come from Source/Modules/*Specs.h).
  // Examples — the actual set follows the installed modules:
  "Filter":     { "Enabled": true,  "Type": "Lowpass", "Cutoff": 500.0, "Resonance": 2.5 },
  "Compressor": { "Enabled": false, "Threshold": -18.0, "Ratio": 2.0, "Attack": 10.0, "Release": 120.0, "Makeup": 0.0 },
  "Stereo":     { "Enabled": true,  "Width": 0.5, "Time": 12.0 },
  "Master":     { "On": true, "Volume": 0.5, "Tempo": 130.0 },
  "Sub":        { "Enabled": false, "Waveform": "Sine", "Octave": "-1 Oct", "Level": 0.5 },
  "Noise":      { "Enabled": false, "Type": "White", "Amount": 0.5 },
  "Formant":    { "Enabled": false, "Vowel": 0.0, "Resonance": 0.5, "Mix": 1.0 },
  "Distortion": { "Enabled": false, "Type": "SoftClip", "Drive": 0.5, "Mix": 1.0 },
  "Wavefold":   { "Enabled": false, "Drive": 0.3, "Symmetry": 0.0, "Mix": 1.0 },
  "Bitcrush":   { "Enabled": false, "Bits": 8.0, "Rate": 1.0, "Mix": 1.0 },
  // … Oscillators, Wavetable, LFOs (indexed 1..4), ModMatrix (6 slots),
  //   ADSR, PitchEnv, Glide, Arp, Delay, Chorus, Reverb, Karplus, CrossMod, etc.

  // Optional. Only present when the rack layout differs from the factory default.
  "RackLayout": { /* nested, human-readable module positions/visibility */ }
}
```

## Load semantics

- **A preset is a complete snapshot.** On load, JASS first **factory‑resets every
  parameter to its default** (and clears the rack layout), then layers the file's
  values on top.
- **Missing fields ⇒ factory default.** A field the file omits (e.g. a feature
  added after the preset was saved) lands on its default — it does *not* inherit
  the previously loaded patch's value. LiveState always contains all current
  fields, so its round‑trip is unaffected.
- **`FormatVersion`** is reserved for *value* migrations (a field whose meaning
  changed). Merely adding new fields needs no version bump — the missing⇒default
  rule handles that. `v4` folded the LFO's built‑in target into modulation‑matrix
  slots. This integer contract is independent of the app's CalVer version.
- **Migration & backups.** Presets older than the current `FormatVersion` are
  upgraded to the current format — both by the startup batch pass (`convertOldPresets`)
  and when you open an older file via the LOAD dialog. **Before rewriting, the original
  is copied to `%AppData%\JASS\PresetsBackup\`.** The direct LOAD path also **fails
  loudly** (a visible message) on a corrupt / non‑JASS file instead of silently
  resetting to defaults, and it tells you when a file was migrated (from which version).
- **Quantization:** values snap to each parameter's step, so a round‑trip may
  differ by less than one step.
- **Enum fields** are stored as the choice string (e.g. `"Lowpass"`,
  `"SoftClip"`), matched by name back to the parameter's choice list.

## Notes

- **Built‑in wavetable banks** ship embedded and seed into
  `%AppData%\JASS\Wavetables\` on first run. `WavetableBankIndex` 0..5 are the
  built‑ins; higher indices reference user‑loaded WAVs.
- **Demo presets** (`DemoPresets/*.jass`) are embedded and auto‑seed into the
  Presets folder on first run.
