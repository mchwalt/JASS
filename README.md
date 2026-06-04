# Synthy

Ein Software-Synthesizer, **zweimal gebaut** — einmal in **C# (WPF/NAudio)** und einmal in
**C++ (JUCE)**. Beide teilen denselben DSP-Kern und ein **gemeinsames Preset-Format**, sodass
sich derselbe Patch in beiden Apps laden lässt. Ziel des Projekts: dieselbe Synthese in zwei
Sprachen/Frameworks vergleichen — und über einen geteilten „LiveState" den Klang bei
identischen Einstellungen direkt gegenüberstellen.

> Bei gleichem Algorithmus klingen beide identisch. Der Mehrwert der C++-Variante ist der
> **VST3-Export für REAPER & Co.** sowie Echtzeitsicherheit (kein GC).

## Die zwei Implementierungen

| | C# Synthy | C++ Synthy |
|---|---|---|
| Stack | .NET 9, WPF, NAudio | C++20, JUCE 8, CMake/MSVC |
| Ausgabe | Standalone (WPF-Fenster) | Standalone **+ VST3** |
| Stimmen | globale Engine (monophon-artig) | polyphon, 8 Voices |
| Ordner | `D:\Projects\C#\Synthesizer\Synthy` | dieses Repo |

## Features (in beiden vorhanden)

- **3 Oszillatoren** — Sine / Sawtooth / Square / Triangle, je mit Enable, Freq, Amp
- **Per-Oszillator-Unison** — 1–7 Stimmen, Detune (0–1 = ±1 Halbton)
- **Mix-Modi** — Additive, Ring-Modulation, FM (OSC1 → OSC2)
- **Wavetable-Oszillator** — 6 Built-in Banks (Basic, Digital, Harmonic, Vocal, PWM, Spectral),
  Position-Morph, **WAV-Import**, eigenes Unison
- **Noise** — White + Pink (Voss-McCartney)
- **Karplus-Strong** — gezupfte Saite (Freq, Amp, Damping, Stretch)
- **ADSR-Hüllkurve**
- **LFO** — Sine/Triangle/Square/Saw → Frequency / Amplitude / Filter-Cutoff
- **Biquad-Filter** — Off / Lowpass / Highpass, Resonanz
- **Distortion** — Off / SoftClip / HardClip / Foldback, Drive, Mix
- **Wavefolding** — West-Coast Sinus-Wavefolder (pre-filter), Drive / Symmetry / Mix
- **Effekte** — Delay, Chorus, Reverb
- **Visualisierung** — Oszilloskop + Spektrum-Analyzer (FFT)
- **Randomize** — würfelt per Knopfdruck einen zufälligen, hörbaren Patch
- **Presets** im gemeinsamen `.synthy`-Format + geteilter LiveState

## Gemeinsames Preset-Format & LiveState

Beide Apps lesen/schreiben dasselbe JSON-Format (`*.synthy`) im selben Verzeichnis:

```
%AppData%\Roaming\Synthy\
├─ Presets\*.synthy      ← benannte Presets (Save/Load)
└─ LiveState.synthy      ← aktueller Zustand, auto-geladen/-gespeichert
```

Der **LiveState** wird beim Start geladen und bei jeder Änderung (debounced ~1,5 s) sowie beim
Beenden gespeichert. So kannst du C# Synthy schließen, C++ Synthy öffnen — und arbeitest mit
denselben Einstellungen weiter (ideal für A/B-Klangvergleiche bei identischen Parametern).

Details & vollständiges Schema: **[`Synthy_Preset_Format.md`](Synthy_Preset_Format.md)**.

## Bauen & Starten

### C++ (JUCE)

Voraussetzungen: Visual Studio 2022 (C++-Desktop-Workload), JUCE als Git-Submodule.

```powershell
# Submodule holen (einmalig nach dem Klonen)
git submodule update --init --recursive

# Konfigurieren (erzeugt build/ via CMake) und bauen
cmake -B build -G "Visual Studio 17 2022"
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
    build\Synthy_Standalone.vcxproj /p:Configuration=Release /m
```

Ausgabe:
- Standalone: `build\Synthy_artefacts\Release\Standalone\Synthy.exe`
- VST3: `build\Synthy_artefacts\Release\VST3\Synthy.vst3` (Target `Synthy_VST3.vcxproj`)

> Hinweis: In der Git-Bash werden MSBuild-Slash-Argumente (`/m`) verstümmelt — den Build
> **über PowerShell** ausführen.

### C# (WPF)

```powershell
cd D:\Projects\C#\Synthesizer\Synthy
dotnet run
```

## Projektstruktur (C++)

```
Source/
├─ PluginProcessor.*      Audio-Processor, APVTS, Auto-Play, LiveState
├─ Audio/
│  ├─ Parameters.h        alle Parameter + applyToVoice()
│  ├─ PresetIO.h          .synthy Import/Export (gemeinsames Format)
│  ├─ SynthVoice.*        eine Stimme (Oszillatoren, Generatoren, Effekte)
│  └─ SynthSound.*
├─ DSP/                   Oscillator, NoiseGenerator, KarplusStrong,
│                         WavetableBank/-Oscillator, BiquadFilter, LFO,
│                         AdsrEnvelope, Effects
└─ UI/                    PluginEditor + Knob/Display-Komponenten
```

## Weitere Doku

- **[`Feature_Ideas.md`](Feature_Ideas.md)** — Roadmap: erledigt & geplant, inkl. neuer Ideen
- **[`Synthy_Preset_Format.md`](Synthy_Preset_Format.md)** — Spezifikation des `.synthy`-Formats
- **[`Synth_Cheatsheet.md`](Synth_Cheatsheet.md)** — Referenz zu Surge XT / Helm / Odin 2 (Recherche)
- **[`CPP_Synth_Entwicklung_Uebersicht.md`](CPP_Synth_Entwicklung_Uebersicht.md)** — Recherche zu C++-Frameworks
- **[`REAPER_Keybindings_Cheatsheet.md`](REAPER_Keybindings_Cheatsheet.md)** — REAPER-Shortcuts fürs Plugin-Testing
