# JASS `.jass` Preset Format (nested, v7)

JASS stores patches as **JSON** files with the extension `.jass`. The format is
**nested** (one object per module) and versioned; the current version is
**`FormatVersion: 7`** (defined as `PresetIO::kFormatVersion`).

- **Format:** JSON, UTF‑8. File extension `.jass`.
- **Canonical source:** each module declares its own parameters and JSON shape in
  `Source/Modules/<Name>Specs.h`; the serializer walks those specs
  (`Modules::writeState` / `readState`), so the spec files — not this document —
  are the single source of truth for field names. This doc describes the
  *structure*.
- **Serializer:** `Source/Audio/PresetIO.h`.
- **Terms & abbreviations:** see the [Glossary](Glossary.md) (e.g.
  [APVTS](Glossary.md#apvts), [LiveState](Glossary.md#livestate),
  [FormatVersion](Glossary.md#formatversion), [Seeding](Glossary.md#seeding)).

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
  "FormatVersion": 6,
  "Name": "Matrix Demo",
  "Modified": false,        // LiveState only: true = unsaved working state (header shows "Current State")

  // One nested object per module (spec-driven; field names come from Source/Modules/*Specs.h).
  // Examples — the actual set follows the installed modules:
  "Filter":     { "Enabled": true,  "Type": "Lowpass", "Cutoff": 500.0, "Resonance": 2.5 },
  "Compressor": { "Enabled": false, "Threshold": -18.0, "Ratio": 2.0, "Attack": 10.0, "Release": 120.0, "Makeup": 0.0 },
  "Stereo":     { "Enabled": true,  "Width": 0.5, "Time": 12.0 },
  "Master":     { "On": true, "Volume": 0.5, "Tempo": 130.0 },
  "Sub":        { "Enabled": false, "Waveform": "Sine", "Octave": "-1 Oct", "Amp": 0.5 },   // "Amp" was "Level" until 2026-08; the old key is still read (legacyPersistKey fallback)
  "Noise":      { "Enabled": false, "Type": "White", "Amount": 0.5 },
  "Formant":    { "Enabled": false, "Vowel": 0.0, "Resonance": 0.5, "Mix": 1.0 },
  "Distortion": { "Enabled": false, "Type": "SoftClip", "Drive": 0.5, "Mix": 1.0 },
  "Wavefold":   { "Enabled": false, "Drive": 0.3, "Symmetry": 0.0, "Mix": 1.0 },
  "Bitcrush":   { "Enabled": false, "Bits": 8.0, "Rate": 1.0, "Mix": 1.0 },
  // … Oscillators, Wavetable, LFOs (indexed 1..4), ModMatrix (8 slots),
  //   ADSR, PitchEnv, Glide, Arp, Delay, Chorus, Reverb, Karplus, CrossMod, etc.

  // Optional. Only present when the rack layout differs from the factory default.
  "RackLayout": { /* nested, human-readable module positions/visibility */ }
}
```

Field renames never break old files: a `ParamSpec` may carry a `legacyPersistKey`, and the
reader falls back to it when the current key is missing. Writes always use the current key.
So far (all 2026-08): `Sub.Level` → `Sub.Amp`, `Sampler.Level` → `Sampler.Amp`,
`Perc.Level1..4` → `Perc.Amp1..4`.

### Fields that are not parameters

Three fields sit next to the spec-driven ones because what they carry has no knob:

| Field | Meaning |
|---|---|
| `Sampler.File` | Name of the selected sample **set**. `Sampler.Set` is a session-local index into whatever is installed, so the name is what actually survives a move to another machine; it is re-resolved on load, and fetched in the background if the set is not loaded yet. |
| `Perc.File` | The same for PERC's drum **kit** (`Perc.Kit` is the index; 0 means "no kit"). A preset carrying only the index would point at whatever set happens to sit there — for a drum pattern, usually a piano. |
| `StepSeq.LatchRoot` | MIDI note the STEP SEQ figure is **latched** to, or the field is absent. Since the latch outlives the key that started it, a patch can be saved while a figure is running — and this is what makes it come back running, on the same note. **Absent ⇒ the patch loads silent** *and* clears whatever the previous patch left running. |
| `StepSeq.Steps` | **v7 (story 15.2):** the 32 steps as an **array of note objects** — `{ "On": true, "Note": 46, "Name": "Bb1", "Accent": true }` — replacing the flat `Pitch1`/`Step1`/… keys (still read from older files). `Note` is the **absolute MIDI note and canonical**; `Name` is generated for the reader's eyes and ignored on load, so the two can never diverge; `Accent` is omitted when plain. Absolute pitch is resolved against `LatchRoot` (or C3 = 48 when absent), and the engine keeps root-relative offsets internally — the figure still transposes with the played key. |

None of them is automatable, and none appears in the modulation matrix. (The steps' underlying
on/off/accent/pitch values ARE parameters and do take part in RANDOM.)

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
  rule handles that. Version history: `v3` = nested per module (flat before);
  `v4` folded the LFO's built‑in target into modulation‑matrix slots; `v5` split
  the matrix DEST into MODULE + PARAM combos; `v6` sorted the matrix
  module/param catalog A→Z (the persisted PARAM index was remapped); `v7` moved
  the STEP SEQ steps into the `StepSeq.Steps` array of note objects (see above).
  This integer contract is independent of the app's CalVer version.
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
