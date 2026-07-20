# JASS – Just Another Simple Synthesizer

A polyphonic software synthesizer written in **C++20 / [JUCE 8](https://juce.com/)**,
with an interface styled as a **19″ rack**. It runs as a **standalone** app, which
is the main focus.

> **VST3:** a VST3 build is produced as well, but it has so far only been tested
> **very superficially in vPlayer 4 Lite** — treat plugin support as experimental.

## Why this project

JASS is about building a **simple synthesizer with an intuitive interface** — one
where you get **quick first wins with good-sounding results** instead of getting
lost in menus. The goal is a **playful way into synthesis and sound processing**:
tweak a few knobs, hear something musical right away, and from there explore how
the pieces fit together — and invent your own solutions and signal paths, also
**off the beaten track**, beyond the usual textbook layouts.

Everything you need is on one screen: sources, modulation, and processing laid
out as a rack you can rearrange, with per-module and per-zone **enable / reset /
info** so nothing is hidden and every control explains itself.

![JASS Rack](docs/screenshots/rack.png)

## Features

**Sound sources**
- **3 oscillators** — sine / sawtooth / square / triangle, each with enable,
  freq, amp, **per-oscillator unison** (1–7 voices, detune) and **self-FM** (feedback)
- **Cross-mod / mix modes** — additive, ring modulation, FM between selectable
  sources (SRC/DEST)
- **Sub oscillator** — sine/square, −1/−2 octave, tracks OSC 1 pitch
- **Wavetable oscillator** — 6 built-in banks (Basic, Digital, Harmonic, Vocal,
  PWM, Spectral) with example WAVs, position morph, **WAV import**, own unison
- **Noise** — white / pink / brown / blue
- **Karplus-Strong** — plucked string, played from the keyboard
- **On-screen keyboard** — playable with mouse & computer keys

**Envelopes & modulation**
- **ADSR** + **pitch envelope**
- **4 LFOs** (sine/triangle/square/saw), tempo sync
- **Modulation matrix** — 6 slots, free source→target wiring with amount;
  routing auto-enables the source and the target module
- **Poly glide** (mono/legato/poly), **arpeggiator** (up/down/up-down/random)

**Processing & effects**
- **Biquad filter** (lowpass/highpass, resonance) + **formant filter**
- **Distortion** (soft/hard clip, foldback), **wavefolding**, **bitcrusher**
- **Compressor**, **phaser/flanger**
- **Delay** (tempo sync), **chorus**, **reverb**
- **Pseudo-stereo** master stage (on by default)

**Playing & workflow**
- **19″ rack UI** with zones; every module and zone has **enable / reset / info**
  (context help, EN/DE)
- Show/hide and reorder modules (persisted), **randomize** & **reset**
- **Oscilloscope + spectrum analyzer** (FFT)
- **3D spinning JASS logo** in the header (toggle off via right-click)
- **Presets** in the `.jass` format + an auto-saved live state; demo presets and
  example wavetables ship embedded and seed on first run

Use the **MODULES** button to show/hide modules per zone and reorder them by
drag & drop (the layout is saved; "Reset layout" restores the factory arrangement):

<img src="docs/screenshots/Modules.png" alt="MODULES panel" width="360">

## Build & run

**Prerequisites:** Visual Studio 2022 (C++ Desktop workload), CMake, Git.
JUCE is a **Git submodule** (only the commit pointer lives in this repo).

```powershell
# 1. Clone with the JUCE submodule
git clone --recurse-submodules <repo-url> JASS
cd JASS
#   (if already cloned without --recurse-submodules:)
git submodule update --init --recursive

# 2. Configure (generates build/ via CMake)
cmake -B build -G "Visual Studio 17 2022"

# 3. Build the standalone (Release)
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
    build\JASS_Standalone.vcxproj /p:Configuration=Release /m
```

**Output:**
- Standalone: `build\JASS_artefacts\Release\Standalone\JASS.exe`
- VST3: `build\JASS_artefacts\Release\VST3\JASS.vst3` (target `JASS_VST3.vcxproj`)

> Note: MSBuild slash arguments (`/m`) get mangled in Git Bash — run the build
> from **PowerShell**.

## Project layout

```
Source/
├─ PluginProcessor.*      Audio processor, APVTS, live state, app-data setup
├─ Modules/               Declarative module specs (one module = one place):
│                         <Name>Specs.h generate APVTS params + rack descriptor
├─ Audio/
│  ├─ Parameters.h        parameter IDs + applyToVoice()
│  ├─ PresetIO.h          .jass import/export (nested v4)
│  └─ SynthVoice.*        one voice (oscillators, generators, effects)
├─ DSP/                   Oscillator, Noise, KarplusStrong, WavetableBank,
│                         BiquadFilter, LFO, ADSR, Effects, ModMatrix …
└─ UI/                    rack editor + knob/display components
```

Architecture concept: **[`docs/Modul_Architektur_Konzept.md`](docs/Modul_Architektur_Konzept.md)**.

## Docs

- **[`docs/JASS_Preset_Format.md`](docs/JASS_Preset_Format.md)** — `.jass` preset format (nested v4)
- **[`docs/Modul_Architektur_Konzept.md`](docs/Modul_Architektur_Konzept.md)** — declarative module-spec architecture
- **[`docs/Glossary.md`](docs/Glossary.md)** — glossary
- `docs/notes/` — internal ideas, research & cheat-sheet notes (not official docs)

## Versioning

JASS uses **CalVer** — `YYYY.MM.MICRO` (e.g. `2026.07.0`) — shown in the header
subtitle and the right-click title info menu. Changes land on `main` only via pull
requests, and each merge bumps the version. See **[`CHANGELOG.md`](CHANGELOG.md)**.

The app version is independent of the preset **`FormatVersion`** (an integer schema
contract — see [`docs/JASS_Preset_Format.md`](docs/JASS_Preset_Format.md)).

## License

JASS is released under the **[GNU GPL v3](LICENSE)**.

JASS embeds **JUCE** as a submodule. JUCE is dual-licensed (**GPLv3 or a
commercial JUCE licence**); the GPLv3 choice covers free JUCE use. Distributing
JASS under other terms requires an appropriate JUCE licence.
