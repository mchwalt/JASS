# JASS – Just Another Simple Synthesizer

Ein polyphoner Software-Synthesizer in **C++20 / [JUCE 8](https://juce.com/)**,
mit einer als **19″-Rack** gestalteten Oberfläche. Läuft als **Standalone** und
als **VST3** (getestet in REAPER).

> Früher „Synthy". JASS war ursprünglich **zweimal** gebaut (C# WPF + C++ JUCE)
> und teilte ein Preset-Format. Die C#-App ist inzwischen **eingefroren** und die
> C#-Kompatibilität wurde **abgeworfen**: Presets sind jetzt `.jass` im Ordner
> `%AppData%\JASS` (einmalige Auto-Migration aus dem alten `Synthy`-Ordner beim
> ersten Start). Weiterentwicklung nur noch in dieser C++-App.

![JASS Rack](docs/screenshots/rack.png)

## Features

**Klangerzeugung**
- **3 Oszillatoren** — Sine / Sawtooth / Square / Triangle, je mit Enable, Freq,
  Amp, **Per-Oszillator-Unison** (1–7 Stimmen, Detune) und **Self-FM** (Feedback)
- **Cross-Mod / Mix-Modi** — Additive, Ring-Modulation, FM zwischen wählbaren
  Quellen (SRC/DEST)
- **Sub-Oszillator** — Sine/Square, −1/−2 Oktav, folgt OSC-1-Tonhöhe
- **Wavetable-Oszillator** — 6 Built-in Banks (Basic, Digital, Harmonic, Vocal,
  PWM, Spectral) mit Beispiel-WAVs, Position-Morph, **WAV-Import**, eigenes Unison
- **Noise** — White / Pink / Brown / Blue
- **Karplus-Strong** — gezupfte Saite, über die Klaviatur gespielt
- **On-Screen-Klaviatur** — spielbar per Maus & Computertasten

**Hüllkurven & Modulation**
- **ADSR** + **Pitch-Envelope**
- **4 LFOs** (Sine/Triangle/Square/Saw), Tempo-Sync
- **Modulation Matrix** — 6 Slots, freie Quelle→Ziel-Verdrahtung mit Amount;
  Routing aktiviert Quelle & Ziel-Modul automatisch
- **Poly-Glide** (Mono/Legato/Poll), **Arpeggiator** (Up/Down/UpDown/Random)

**Verarbeitung & Effekte**
- **Biquad-Filter** (Lowpass/Highpass, Resonanz) + **Formant-Filter**
- **Distortion** (SoftClip/HardClip/Foldback), **Wavefolding**, **Bitcrusher**
- **Kompressor**, **Phaser/Flanger**
- **Delay** (Tempo-Sync), **Chorus**, **Reverb**
- **Pseudo-Stereo** (Master-Stufe, werksseitig an)

**Bedienung**
- **19″-Rack-UI** mit Zonen; jedes Modul und jede Zone hat **Enable / Reset /
  Info** (Kontext-Hilfe EN/DE)
- Module ein-/ausblenden & neu anordnen (persistiert), **Randomize** & **Reset**
- **Oszilloskop + Spektrum-Analyzer** (FFT)
- **3D-rotierender JASS-Schriftzug** im Header (per Rechtsklick abschaltbar)
- **Presets** im `.jass`-Format + auto-gespeicherter LiveState; Demo-Presets &
  Beispiel-Wavetables sind eingebettet und werden beim ersten Start angelegt

## Bauen & Starten

**Voraussetzungen:** Visual Studio 2022 (C++-Desktop-Workload), CMake, Git.
JUCE ist als **Git-Submodule** eingebunden (nur der Commit-Pointer liegt im Repo).

```powershell
# 1. Klonen inkl. JUCE-Submodule
git clone --recurse-submodules <repo-url> JASS
cd JASS
#   (falls schon ohne --recurse-submodules geklont:)
git submodule update --init --recursive

# 2. Konfigurieren (erzeugt build/ via CMake)
cmake -B build -G "Visual Studio 17 2022"

# 3. Standalone bauen (Release)
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
    build\JASS_Standalone.vcxproj /p:Configuration=Release /m
```

**Ausgabe:**
- Standalone: `build\JASS_artefacts\Release\Standalone\JASS.exe`
- VST3: `build\JASS_artefacts\Release\VST3\JASS.vst3` (Target `JASS_VST3.vcxproj`)

> Hinweis: MSBuild-Slash-Argumente (`/m`) werden in der Git-Bash verstümmelt —
> den Build **über PowerShell** ausführen.

## Projektstruktur

```
Source/
├─ PluginProcessor.*      Audio-Processor, APVTS, LiveState, AppData-Migration
├─ Modules/               Deklarative Modul-Specs (ein Modul = ein Ort):
│                         <Name>Specs.h erzeugen APVTS-Params + Rack-Descriptor
├─ Audio/
│  ├─ Parameters.h        Parameter-IDs + applyToVoice()
│  ├─ PresetIO.h          .jass Import/Export (nested v4)
│  └─ SynthVoice.*        eine Stimme (Oszillatoren, Generatoren, Effekte)
├─ DSP/                   Oscillator, Noise, KarplusStrong, WavetableBank,
│                         BiquadFilter, LFO, ADSR, Effects, ModMatrix …
└─ UI/                    Rack-Editor + Knob-/Display-Komponenten
```

Architektur-Konzept: **[`docs/Modul_Architektur_Konzept.md`](docs/Modul_Architektur_Konzept.md)**.

## Doku

- **[`docs/JASS_Preset_Format.md`](docs/JASS_Preset_Format.md)** — `.jass`-Preset-Format (nested v4)
- **[`docs/Modul_Architektur_Konzept.md`](docs/Modul_Architektur_Konzept.md)** — deklarative Modul-Spec-Architektur
- **[`docs/Glossary.md`](docs/Glossary.md)** — Begriffe
- `docs/notes/` — interne Ideen-, Recherche- & Cheat-Sheet-Notizen (kein offizieller Doku-Stand)

## Lizenz

JASS steht unter der **[GNU GPL v3](LICENSE)**.

JASS bindet **JUCE** als Submodule ein. JUCE ist dual-lizenziert (**GPLv3 oder
kommerzielle JUCE-Lizenz**); die GPLv3-Wahl deckt die kostenlose JUCE-Nutzung ab.
Wer JASS unter anderen Bedingungen verbreiten will, benötigt eine passende
JUCE-Lizenz.
