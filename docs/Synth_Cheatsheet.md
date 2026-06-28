# Synthesizer Cheatsheet: Surge XT, Helm, Odin 2

---

## Surge XT v1.3.4

### Architektur
- **2 Scenes** (A/B) – jeweils unabhängig mit eigenen Oszillatoren, Filtern, Envelopes
- **3 Oszillatoren** pro Scene + Ring Mod (1x2, 2x3) + Noise
- **2 Filter** pro Scene (seriell, parallel, stereo, ring, wide)
- **12 LFOs** (6 Voice + 6 Scene)
- **8 Macros** (zuweisbar auf MIDI CC)
- **16 Effekt-Slots** (Insert, Send, Global)

### Oszillator-Typen
| Typ | Beschreibung |
|---|---|
| Classic | Traditionelle Wellenformen |
| Modern | Erweiterte Wellenform-Generierung |
| Wavetable | Wavetable-Synthese |
| Window | Windowed Wavetable |
| Sine | Reine Sinuswelle |
| FM2 / FM3 | FM-Synthese mit 2/3 Operatoren |
| String | Physical Modeling |
| Twist | Morphing Waveforms |
| Alias | Aliased Waveforms |
| S&H Noise | Sample-and-Hold Noise |
| Audio Input | Externes Audio verarbeiten |

### Filter-Typen
- Lowpass, Highpass, Bandpass (jeweils mehrere Subtypes)
- Konfigurationen: Serial, Dual, Stereo, Ring, Wide

### Modulationsquellen
- 2x ADSR (Filter EG + Amp EG)
- 12 LFOs (Sine, Triangle, Square, Saw, Noise, Envelope, Step Sequencer, MSEG, Formula/Lua)
- MIDI: Velocity, Aftertouch, Pitch Bend, Mod Wheel, MPE
- Random, Alternate, Keytrack

### Effekte
| Kategorie | Effekte |
|---|---|
| Insert | EQ, Graphic EQ, Exciter, Resonator, Waveshaper, Distortion, Tape, CHOW, Neuron, Bonsai, Combulator |
| Send | Chorus, Ensemble, Flanger, Phaser, Rotary, Freq Shifter, Delay, Reverb (3 Typen), Spring Reverb, Tremolo |
| Utility | Ring Mod, Vocoder, Nimbus, Treemonster, Airwindows, Conditioner, Mid-Side |

### Play Modes
Poly, Mono, Mono ST (Single Trigger), Mono FP (Fingered Portamento), Latch

### Shortcuts
| Shortcut | Aktion |
|---|---|
| `Ctrl+Z` / `Ctrl+Y` | Undo / Redo |
| `Ctrl+S` | Patch speichern |
| `Alt+K` | Virtuelle Tastatur öffnen |
| `Alt+O` | Oszilloskop öffnen |
| `F1` | Kontexthilfe für Parameter |
| `Tab` | Routing-Modus umschalten |
| `Doppelklick` | Parameter auf Default zurücksetzen |
| `Shift+Drag` | Feinsteuerung (Slider) |
| `Ctrl+Drag` | Schrittweise Steuerung (Slider) |
| `Rechtsklick` | Kontextmenü |
| `Esc` | Eingabe abbrechen |

---

## Helm v0.9.0

### Architektur
- **2 Oszillatoren** + 1 Sub-Oszillator + Noise
- **1 Filter** mit mehreren Typen
- **2 ADSR Envelopes** (Filter + Amp)
- **2 LFOs**
- **Polyphon** (bis zu 32 Stimmen)
- **Modulationsmatrix** mit visueller Darstellung

### Oszillator-Typen
| Typ | Beschreibung |
|---|---|
| Saw | Sägezahn |
| Square / Pulse | Rechteck mit PWM |
| Triangle | Dreieck |
| Sine | Sinus |
| Noise | Rauschen |

- Cross-Modulation zwischen Osc 1 und Osc 2
- Sub-Oszillator (1/2 Oktave tiefer, Saw/Square/Sine)

### Filter
- Lowpass 12/24 dB
- Highpass 12/24 dB
- Bandpass 12/24 dB
- Allpass
- Comb Filter
- Filter Drive / Saturation

### Modulation
- 2x ADSR Envelopes (visuell editierbar)
- 2x LFOs (Sine, Triangle, Square, Saw Up, Saw Down, S&H)
- Velocity, Aftertouch, Mod Wheel, Pitch Bend
- Step Sequencer (16 Steps)
- Visuelle Mod-Verbindungen in der UI

### Effekte
| Effekt | Details |
|---|---|
| Distortion | Soft Clip, Hard Clip, Fold Back |
| Stutter | Rhythmisches Gating |
| Delay | Tempo-sync, Feedback, Stereo |
| Reverb | Size, Damping, Mix |
| Formant Filter | Vowel-basierte Filterung |

### Bedienung
- Alle Knobs per Drag steuerbar
- Modulationsverbindungen per Drag von Quelle auf Ziel
- Patch Browser oben in der UI
- Rechtsklick auf Knobs für MIDI Learn

---

## Odin 2 v2.4.0

### Architektur
- **3 Oszillatoren** (frei wählbarer Typ pro Slot)
- **2 Filter** (seriell/parallel konfigurierbar)
- **3 ADSR Envelopes**
- **4 LFOs**
- **Semi-modulare Routing-Matrix**
- **24 Stimmen** polyphon
- **5 Effekte**

### Oszillator-Typen
| Typ | Beschreibung |
|---|---|
| Analog | Klassische Wellenformen (Saw, Square, Tri, Sine) |
| Wavetable | Wavetable mit Morph |
| Multi | Mehrere Wellenformen gleichzeitig |
| Vector | Vektor-Synthese |
| Chiptune | 8-bit Retro-Sounds |
| FM | FM-Synthese |
| PM (Phase Mod) | Phasenmodulation |
| Noise | Rauschgenerator |
| Wavedraw | Eigene Wellenformen zeichnen |

### Filter-Typen
| Typ | Beschreibung |
|---|---|
| SEM 12 | Oberheim SEM Emulation |
| Moog Ladder | Moog Ladder Filter Emulation |
| Korg 35 | Korg MS-20 Filter Emulation |
| Diode Ladder | Dioden-Ladder Filter |
| Formant | Vokal-/Formant-Filter |
| Comb | Kammfilter |
| Ring Mod | Ringmodulator |

- Jeweils LP, HP, BP Varianten verfügbar

### Modulation
- 3x ADSR Envelopes
- 4x LFOs (Sine, Saw, Triangle, Square, S&H, Noise)
- XY-Pad
- Modulations-Matrix mit flexiblem Routing
- MIDI: Velocity, Aftertouch, Mod Wheel

### Effekte
| Effekt | Beschreibung |
|---|---|
| Delay | Stereo Delay mit Sync |
| Phaser | Klassischer Phaser |
| Flanger | Flanging-Effekt |
| Chorus | Stereo Chorus |
| Distortion | Verzerrung / Waveshaping |

### Besonderheiten
- Semi-modulare Architektur: flexibles Signal-Routing zwischen allen Modulen
- Filter-Emulationen legendärer Hardware (Moog, Korg, Oberheim)
- Wavedraw-Oszillator zum Zeichnen eigener Wellenformen
- Chiptune-Oszillator für Retro-Sounds

---

## Vergleich auf einen Blick

| Feature | Surge XT | Helm | Odin 2 |
|---|---|---|---|
| Oszillatoren | 3 (11 Typen) | 2 + Sub | 3 (9 Typen) |
| Filter | 2 (multi-config) | 1 (6 Typen) | 2 (7 Emulationen) |
| LFOs | 12 | 2 | 4 |
| Envelopes | 2 ADSR + MSEG | 2 ADSR | 3 ADSR |
| Effekte | 16 Slots | 5 | 5 |
| Scenes/Layers | 2 Scenes | - | - |
| Stimmen | Konfigurierbar | 32 | 24 |
| Komplexität | Hoch | Niedrig | Mittel |
| Lernkurve | Steil | Einfach | Moderat |
