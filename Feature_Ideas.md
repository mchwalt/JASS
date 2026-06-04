# Synthy – Feature Ideas & Roadmap

Stand: 2026-05-26. Legende: Aufwand ★ (wenig) … ★★★★★ (viel) · Coolness ★ … ★★★★★

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

1. **Live-Modulations-Ringe** – animierte Ringe um Knöpfe (Vital-Style), sieht edel aus (★★★ / ★★★★★)
2. **Sub-Oszillator** – dedizierter Bass mit einem Klick (★ / ★★★★)
3. **Bitcrusher** – Bit-/Samplerate-Reduktion → Lo-Fi (★ / ★★★★)

---

## 🔊 Neue Klang-Engines

| Feature | Was es macht | Aufwand | Coolness |
|---|---|---|---|
| ~~Wavefolding~~ | ✅ umgesetzt (beide Apps) – Sinus-Folder, Drive/Symmetry/Mix, pre-filter | – | – |
| **Granular-Synthese** | Sample in Körner zerlegen, streuen/pitchen → Clouds, Texturen | ★★★★ | ★★★★★ |
| **Formant-/Vokal-Filter** | morpht A-E-I-O-U → „sprechender" Synth (Helm/Odin haben das) | ★★★ | ★★★★★ |
| **Modal-Synthese** | Karplus-Erweiterung für Glocken/Mallets (Resonatoren) | ★★★ | ★★★★ |
| **Sub-Oszillator** | dedizierter −1/−2 Oktav Sine/Square → fetter Bass | ★ | ★★★★ |

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

## 🔗 Cross-Projekt (das Zwei-Sprachen-Setup)

- ✅ **Gemeinsames Preset-Format** – erledigt (`.synthy` + LiveState)
- ✅ **C++ auf C#-Stand gebracht** – Engine-Parität erreicht (MixMode, per-OSC-Unison, Distortion-Typen)
- **Offen:** neue Features konsequent in *beiden* Apps spiegeln, damit Presets verlustfrei bleiben
