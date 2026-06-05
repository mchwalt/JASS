# Synthy `.synthy` Preset Format (shared C# ↔ C++)

Both Synthy apps (C# WPF and C++ JUCE) read and write the **same** preset format,
so a patch saved in one loads in the other.

- **Format:** JSON, UTF-8, indented. File extension `.synthy`.
- **Canonical:** the C# `Preset` class is the reference schema. The C++ side
  (`Source/Audio/PresetIO.h`) maps its APVTS parameters to/from these exact field
  names and enum strings.
- **Enum values** are the C# enum member names (PascalCase, no spaces), e.g.
  `"SoftClip"`, `"FilterCutoff"`, `"RingMod"`.

## Shared locations (identical for both apps)

| Purpose | Path |
|---|---|
| Root | `%AppData%\Roaming\Synthy\` |
| Named presets | `%AppData%\Roaming\Synthy\Presets\*.synthy` |
| **Live state** | `%AppData%\Roaming\Synthy\LiveState.synthy` |

### LiveState (the cross-app bridge)
Both apps **auto-load** `LiveState.synthy` on startup and **auto-save** it on every
change (debounced ~1.5 s) and on exit. Switching between the C# and C++ **standalone**
apps therefore preserves the current settings — useful for A/B comparing the actual
sound at identical parameters.

- C++: only active in **standalone** mode (`wrapperType_Standalone`). As a VST3 in a
  host (e.g. REAPER), the host owns project state and LiveState is left untouched.
- On the C++ standalone, LiveState is re-applied *after* the JUCE wrapper restores its
  own saved state, so the shared LiveState wins on startup.

## Schema

```jsonc
{
  "FormatVersion": 1,
  "Name": "Init",
  "Modified": false,          // C++ LiveState only: true = unsaved working state (header shows "Current State")
  "Oscillators": [            // exactly 3
    {
      "Enabled": false,
      "Waveform": "Sine",      // Sine | Sawtooth | Square | Triangle
      "Frequency": 261.63,     // Hz
      "Amplitude": 0.5,        // 0..1
      "UnisonVoices": 1,       // 1..7
      "UnisonDetune": 0.0      // 0..1  (= ±1 semitone at 1.0)
    } /* x3 */
  ],
  "MasterVolume": 0.7,         // 0..1
  "MixMode": "Additive",       // Additive | RingMod | FM
  "Attack": 0.01, "Decay": 0.1, "Sustain": 0.7, "Release": 0.3,  // seconds / 0..1 sustain
  "FilterType": "Off",         // Off | Lowpass | Highpass
  "FilterCutoff": 5000, "FilterResonance": 0.707,
  "DistortionType": "Off",     // Off | SoftClip | HardClip | Foldback
  "DistortionDrive": 0.5,      // 0..1  (mapped to gain 1x..20x)
  "DistortionMix": 1.0,
  "WavefoldEnabled": false,    // West-Coast sine wavefolder (pre-filter)
  "WavefoldDrive": 0.3,        // 0..1  (mapped to fold gain 1x..8x)
  "WavefoldSymmetry": 0.0,     // -1..1 (asymmetry → even harmonics)
  "WavefoldMix": 1.0,
  "BitcrushEnabled": false,    // lo-fi bitcrusher (after distortion)
  "BitcrushBits": 8,           // 1..16 bit depth
  "BitcrushRate": 1,           // 1..50 sample-rate reduction factor
  "BitcrushMix": 1.0,          // 0..1
  "SubEnabled": false,         // sub-oscillator (tracks OSC 1 pitch, octave(s) down)
  "SubWaveform": "Sine",       // Sine | Square
  "SubOctave": -1,             // -1 or -2 octaves
  "SubLevel": 0.5,             // 0..1
  "StereoEnabled": false,      // pseudo-stereo master stage (mono engine -> wide stereo)
  "StereoWidth": 0.5,          // 0..1  (0 = mono, higher = wider)
  "StereoTime": 12.0,          // 1..15 ms comb/Haas delay
  "LfoWaveform": "Sine",       // Sine | Triangle | Square | Sawtooth
  "LfoTarget": "Off",          // Off | Frequency | Amplitude | FilterCutoff
  "LfoRate": 2.0, "LfoDepth": 0.5,
  "DelayEnabled": false, "DelayTime": 0.3, "DelayFeedback": 0.4, "DelayMix": 0.3,
  "ChorusEnabled": false, "ChorusRate": 1.5, "ChorusDepth": 0.005, "ChorusMix": 0.5,
  "ReverbEnabled": false, "ReverbRoomSize": 0.7, "ReverbDamping": 0.5, "ReverbMix": 0.3,
  "KarplusEnabled": false, "KarplusFrequency": 261.63, "KarplusAmplitude": 0.5,
  "KarplusDamping": 0.5, "KarplusStretch": 0.0,
  "NoiseType": "Off",          // Off | White | Pink
  "NoiseAmplitude": 0.3,
  "WavetableEnabled": false,
  "WavetableBankIndex": 0,     // 0..5 built-in, 6+ = loaded WAVs (per-app, see note)
  "WavetablePosition": 0.0, "WavetableFrequency": 261.63, "WavetableAmplitude": 0.5,
  "WavetableUnisonVoices": 1, "WavetableUnisonDetune": 0.0
}
```

## Notes & known limits

- **Built-in wavetable banks** 0..5 are identical in both apps and in the same order
  (Basic, Digital, Harmonic, Vocal, PWM, Spectral), so `WavetableBankIndex` 0..5 maps
  cleanly. Indices ≥ 6 reference user-loaded WAVs, which are per-app — a WAV bank index
  from one app won't resolve to the same waveform in the other.
- **Quantization:** values snap to each parameter's step on the C++ side (e.g.
  `MasterVolume` step 0.01), so a round-trip may differ by less than one step.
- **Missing fields** are tolerated on both sides. On the C++ side a preset load first resets
  every parameter to its **default**, then applies the file — so a field the preset omits (e.g.
  a feature added after the preset was saved) lands on its default instead of inheriting the
  previously loaded patch's value. (LiveState always contains all current fields, so its
  round-trip is unaffected.) Newer
  fields (`Wavefold*`, `Bitcrush*`, `Sub*`, `Stereo*`) are added by the actively-developed **C++** app; the frozen C#
  app simply ignores them (and they keep their defaults when it writes a preset).
- Note: `Sub*` and the auto-play behaviour are C++ additions. Auto-play is no longer a stored
  field — it is automatic runtime behaviour (drone steps aside when you play, returns when a
  generator is re-enabled).
- C++ implementation: `Source/Audio/PresetIO.h`. C#: `Audio/PresetManager.cs` +
  `MainViewModel.BuildPreset()/ApplyPreset()`.
