# Synthy – Feature Ideas & Roadmap

Stand: 2026-06-04. Legende: Aufwand ★ (wenig) … ★★★★★ (viel) · Coolness ★ … ★★★★★

> **Fokus:** Es wird nur noch die **C++ (JUCE)** App weiterentwickelt; die C# App ist eingefroren.
> Das gemeinsame `.synthy`-Preset-Format wird weiter gepflegt.

---

## ✅ Neu in C++ (seit C#-Einfrieren)

| Feature | Was es macht |
|---|---|
| **Sub-Oszillator** | Sine/Square, −1/−2 Oktav, folgt OSC-1-Tonhöhe → fetter Bass |
| **On-Screen-Klaviatur** | spielbar per Maus & Computertasten (z/x = Oktave); löst ADSR pro Note aus |
| **Tonhöhen-Transponierung** | gespielte Note transponiert alle Erzeuger relativ zu C4 (FREQ-Knöpfe = Grundklang) |
| **FREQ-Regler-Anzeige** | aktive OSC-FREQ-Regler zeigen beim Spielen die gespielte Frequenz, danach zurück zur Basis |
| **Auto-Play automatisch** | Drone (eigener MIDI-Kanal) beim Aktivieren einer Quelle; verstummt beim Spielen, kehrt beim erneuten Aktivieren zurück |

---

## ✅ Bereits umgesetzt (in C# **und** C++)

| Feature | Was es macht |
|---|---|
| **Triangle-Wellenform** | 4. Grundwellenform |
| **Unison/Detune** | per-Oszillator, 1–7 Stimmen → fetter Sound |
| **Mix-Modi** | Additive · **Ring-Modulation** · **FM** (OSC1→OSC2) |
| **Wavetable-Synthese** | 6 Built-in Banks + Position-Morph + **WAV-Import** |
| **Noise Generator** | White + Pink (Voss-McCartney) |
| **Karplus-Strong** | gezupfte Saite (Physical Modeling), Damping/Stretch |
| **Distortion** | SoftClip / HardClip / **Foldback**, Drive, Mix |
| **Wavefolding** | West-Coast Sinus-Wavefolder (pre-filter), Drive / Symmetry / Mix |
| **Spectrum Analyzer** | FFT-Anzeige neben dem Oszilloskop |
| **ADSR · LFO · Biquad-Filter** | Standard-Modulation & Filterung |
| **Delay · Chorus · Reverb** | Effekt-Kette |
| **Randomize-Button** | würfelt einen zufälligen, hörbaren Patch (mit Sanity-Guards) |
| **Gemeinsames Preset-Format** | `.synthy` JSON + geteilter LiveState (App-übergreifend) |

---

## 🎯 Als Nächstes empfohlen

1. **Bitcrusher** – Bit-/Samplerate-Reduktion → Lo-Fi (★ / ★★★★)
2. **Live-Modulations-Ringe** – animierte Ringe um Knöpfe (Vital-Style), sieht edel aus (★★★ / ★★★★★)
3. **Tempo-Sync** – LFO & Delay an BPM koppeln (★★ / ★★★★)

---

## 🔊 Neue Klang-Engines

| Feature | Was es macht | Aufwand | Coolness |
|---|---|---|---|
| ~~Wavefolding~~ | ✅ umgesetzt (beide Apps) – Sinus-Folder, Drive/Symmetry/Mix, pre-filter | – | – |
| **Granular-Synthese** | Sample in Körner zerlegen, streuen/pitchen → Clouds, Texturen | ★★★★ | ★★★★★ |
| **Formant-/Vokal-Filter** | morpht A-E-I-O-U → „sprechender" Synth (Helm/Odin haben das) | ★★★ | ★★★★★ |
| **Modal-Synthese** | Karplus-Erweiterung für Glocken/Mallets (Resonatoren) | ★★★ | ★★★★ |
| ~~Sub-Oszillator~~ | ✅ umgesetzt (C++) – Sine/Square, −1/−2 Oktav | – | – |

## 🎛️ Neue Effekte

| Feature | Was es macht | Aufwand | Coolness |
|---|---|---|---|
| **Phaser / Flanger** | Allpass-Kette / modulierter Comb → Sweep-Sounds | ★★ | ★★★★ |
| **Bitcrusher** | Bit-/Samplerate-Reduktion → Lo-Fi, 8-bit | ★ | ★★★★ |
| **Convolution-Reverb** | echte Impulsantworten laden (Kathedrale, Platte) | ★★★★ | ★★★★ |
| **Stereo-Width / Mid-Side** | Breite stufenlos regeln, Haas-Effekt | ★★ | ★★★ |
| **EQ (3-Band)** | Bass/Mid/Treble | ★★ | ★★★ |

## ✨ Workflow & UX

| Feature | Was es macht | Aufwand | Coolness |
|---|---|---|---|
| ~~Randomize-Button~~ | ✅ umgesetzt (beide Apps) | – | – |
| **Macro-Knöpfe + Preset-Morph** | ein Knopf steuert viele Parameter; A/B überblenden | ★★★ | ★★★★★ |
| **Live-Modulations-Anzeige** | animierte Ringe zeigen LFO/Env-Bewegung | ★★★ | ★★★★★ |
| **Tempo-Sync** | LFO & Delay an BPM koppeln (1/4, 1/8, Triolen) | ★★ | ★★★★ |
| **Step-Sequencer** | eigenes Pattern-Modul | ★★★ | ★★★★ |
| **Arpeggiator** | automatische Notenfolgen (Auf/Ab/Random) | ★★★ | ★★★★★ |
| **WAV-Export / Recording** | aufnehmen, was man spielt | ★★ | ★★★ |
| **MIDI-Learn** | Knöpfe an Hardware-Controller binden | ★★★ | ★★★★ |

---

## 📋 Klassiker noch offen

- **Portamento/Glide** – Tonhöhe gleitet von Note zu Note (★★ / ★★★★)
- **Pitch-Envelope** – Tonhöhe verändert sich über Zeit (Laser, Kicks) (★★ / ★★★★)
- **Stereo-Panning** – pro Oszillator L/R platzieren (★★ / ★★★)
- **Modulation Matrix** – beliebige Quelle → beliebiges Ziel (★★★ / ★★★★★)
- **Voller Sampler** – WAV als Klangquelle (Wavetable-Import deckt schon Teile ab) (★★★★ / ★★★★)

---

## 🔗 Cross-Projekt

- ✅ **Gemeinsames Preset-Format** – `.synthy` + LiveState, wird in C++ weiter gepflegt
- **C# ist eingefroren** – Entwicklung läuft nur noch in C++ (kein Spiegeln mehr). Format bleibt rückwärtskompatibel (fehlende Felder = Defaults).
