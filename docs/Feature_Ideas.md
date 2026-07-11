# Synthy – Feature Ideas & Roadmap

Stand: 2026-06-04. Legende: Aufwand ★ (wenig) … ★★★★★ (viel) · Coolness ★ … ★★★★★

> **Fokus:** Es wird nur noch die **C++ (JUCE)** App weiterentwickelt; die C# App ist eingefroren.
> Das gemeinsame `.synthy`-Preset-Format wird weiter gepflegt.

---

## ✅ Neu in C++ (seit C#-Einfrieren)

| Feature | Was es macht |
|---|---|
| **Sub-Oszillator** | Sine/Square, −1/−2 Oktav, folgt OSC-1-Tonhöhe → fetter Bass |
| **Bitcrusher** | Lo-Fi: Bit-Tiefe (BITS) + Samplerate (RATE) reduzieren, Mix |
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

1. **Tempo-Sync** – LFO & Delay an BPM koppeln (★★ / ★★★★)
2. ~~Live-Modulations-Ringe~~ – ✅ umgesetzt (C++): dezenter cyan-Bogen um die LFO-Ziel-Knöpfe
3. ~~Arpeggiator~~ – ✅ umgesetzt (C++): Up/Down/UpDown/Random, Rate/Octaves/Gate, spielt gehaltene Akkorde

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
| ~~Bitcrusher~~ | ✅ umgesetzt (C++) – BITS + RATE + MIX | – | – |
| **Convolution-Reverb** | echte Impulsantworten laden (Kathedrale, Platte) | ★★★★ | ★★★★ |
| ~~Stereo-Width / Pseudo-Stereo~~ | ✅ umgesetzt (C++) – Master-Stufe WIDTH+TIME, mono-kompatibel (Lauridsen-Comb). Siehe „Echtes Stereo" unten | – | – |
| **EQ (3-Band)** | Bass/Mid/Treble | ★★ | ★★★ |

## ✨ Workflow & UX

| Feature | Was es macht | Aufwand | Coolness |
|---|---|---|---|
| ~~Randomize-Button~~ | ✅ umgesetzt (beide Apps) | – | – |
| **Macro-Knöpfe + Preset-Morph** | ein Knopf steuert viele Parameter; A/B überblenden | ★★★ | ★★★★★ |
| **Live-Modulations-Anzeige** | animierte Ringe zeigen LFO/Env-Bewegung | ★★★ | ★★★★★ |
| **Tempo-Sync** | LFO & Delay an BPM koppeln (1/4, 1/8, Triolen) | ★★ | ★★★★ |
| **Step-Sequencer** | eigenes Pattern-Modul | ★★★ | ★★★★ |
| **Rack-Customization (Show/Hide + Drag&Drop)** | Module/Zonen wegklappen; Module per Drag&Drop zwischen Zonen bewegen; visuell innerhalb einer Zone umsortieren (siehe Detail unten) | ★★★★ | ★★★★★ |
| ~~Arpeggiator~~ | ✅ umgesetzt (C++) – Up/Down/UpDown/Random, Rate/Octaves/Gate | – | – |
| **WAV-Export / Recording** | aufnehmen, was man spielt | ★★ | ★★★ |
| **MIDI-Learn** | Knöpfe an Hardware-Controller binden | ★★★ | ★★★★ |

---

## 🎧 Echtes Stereo (Schritt B – größerer Umbau, ★★★★)

**Ausgangslage (Stand heute):** Die ganze Synth-Engine ist **mono**. Jede `SynthVoice`
rechnet einen einzigen `mixedSample` (`SynthVoice.cpp` `renderNextBlock`) und schreibt ihn
identisch in alle Ausgangskanäle (`outputBuffer.addSample(channel, …, mixedSample)`). Der
Processor summiert alle 8 Voices zu einem mono-identischen L/R-Buffer. **Schritt A**
(Pseudo-Stereo, ✅ umgesetzt) erzeugt Breite *am Ende* der Kette über eine Master-Stufe
(`DSP/StereoWidth.h`, aufgerufen in `processBlock` nach `renderNextBlock`) – ohne die Engine
anzufassen. Das deckt ~80 % des wahrnehmbaren Effekts für ~20 % Aufwand.

**Was „echtes" Stereo (Pan pro Oszillator, Unison-Spread) erfordern würde:**
- Sobald ein Erzeuger **nicht** mittig sitzt, sind L und R **unterschiedliche Signale** →
  ab diesem Punkt muss die **gesamte Signalkette pro Kanal doppelt** laufen.
- `mixedSample` (Skalar) müsste zu **`float[2]` (L/R)** werden.
- **Jeder stateful per-Voice-Effekt braucht Zustand pro Kanal**, sonst Phasen-/Knackser-Bugs:
  `BiquadFilter` (z1/z2), `ChorusEffect`, `DelayEffect`, `ReverbEffect`,
  `BitcrusherEffect` (held/counter). → praktisch alle DSP-Klassen anfassen.
- **Rechenlast ~×2** pro Voice (× 8 Voices). Für ein Lernprojekt vertretbar, aber kein Trivial-Edit.
- Neue Params: Pan pro OSC (−1..+1), Unison-Stereo-Spread (verteilt Detune-Voices übers Panorama).
- **Aufwand realistisch ★★★★** (nicht ★★ wie Pseudo-Stereo). Eigenes größeres Vorhaben;
  Schritt A bleibt danach als globaler WIDTH-Regler nützlich.
- Der separate Eintrag **„Stereo-Panning – pro Oszillator L/R"** unten = genau dieser Schritt B.

---

## 🧩 Rack-Customization: Show/Hide + Drag&Drop (Post-Rack-Umbau, ★★★★)

Sammel-Vorhaben (2026-07-01), am besten als eigenes **„Epic 4: Rack-Customization"** *nach* dem
laufenden Rack-Umbau (Epics 1–3) — es baut genau auf dem Rack-Layout-Modell (AD-2) auf.

**Teil 1 — Module & Zonen ein-/ausblenden.** Selten genutzte Module und ganze Zonen
(GEN/MOD/PROC/MASTER BUS) wegklappen → Rack entrümpeln, Platz sparen. Damit entfällt auch das
Platz-Argument gegen zusätzliche Zonen (z.B. eine VISUALIZATION-Zone für Scope/Spectrum).
Architektonisch billig: `visible`-Flag pro Modul + „im Layout überspringen".

**Teil 2 — Drag&Drop + Verschieben.** Module per Drag&Drop **zwischen** Zonen bewegen **und**
visuell **innerhalb** einer Zone umsortieren. Voraussetzung dafür sind Enabler, die man schon
**während der Migration billig mit-einbaut** (später teuer nachzurüsten — siehe Spine „Deferred"):
- **Stabile Modul-ID** je Deskriptor → gespeichertes Custom-Layout wieder zuordenbar.
- **Explizite (Default-)Zone/Gruppe am Modul** (heute nur am Call-Site `Rack::addModule(zone,…)`),
  **getrennt** von `typeTag` (Identität/Farbe) — Reverb nach GENERATORS ziehen macht es *nicht*
  zum Generator.
- **Rack-Layout als geordnetes Daten-Modell** (`id → {zone, position}`) statt Einfüge-Reihenfolge;
  Verschieben = Edit an diesem Modell.

**Wie persistieren/schalten (User-Präferenz, gilt für Show/Hide *und* Layout):**
1. **Bevorzugt: über Standard-Synthy-Parameter** (APVTS je Modul). Nutzt bestehende Infrastruktur
   (Attachment, Persistenz, `.synthy`). **Interop-safe**, da rückwärtskompatibel (fehlende Felder =
   Defaults, siehe Cross-Projekt), append-only, kein `kFormatVersion`-Bump. Nachteil: View-/Layout-
   State erscheint als automatisierbare DAW-Parameter → in eigene Param-Gruppe, klar getrennt vom Klang.
2. **Fallback: separate Rack-Config** (eigene UI-Prefs-Datei / Host-State-Blob, **nicht** `.synthy`),
   wenn man View-/Layout-State bewusst aus dem Klang-Preset heraushalten will.

**Offene Design-Fragen:** (a) Fenster fix + Rack packt nach, oder Fenster schrumpft beim Ausblenden?
(b) Sichtbarkeit/Layout pro Preset oder global? (c) Affordance: Collapse-Button am Zonen-Header,
Hide-Toggle + Drag-Handle pro Modul.

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

---

## 🔜 Backlog (2026-07-11)

- **Feedback-FM / Self-FM** (★★★ / ★★★★) – ein Oszillator moduliert seine eigene Frequenz (DX-Style, hellere/sägezahnartige Timbres). Braucht einen **eigenen Feedback-Amount-Regler** (nicht als A==B-Nebeneffekt von MIX MODE, wegen Stabilität/Lautheit). **Kein ähnliches Modul vorhanden** (die bestehenden „feedback"-Stellen sind Delay/Reverb/Chorus + Karplus-Loop). MIX MODE koppelt bewusst nur zwei *verschiedene* OSCs.
- **Online-Hilfe pro Modul** (★★ / ★★★) → **BMAD Epic 6 / Story 6.1 (`ready-for-dev`, 2026-07-11).** Design 2026-07-11 überarbeitet: **kein** Hover-Auto-Popup mehr. Stattdessen ein **Info-Icon („i" im Kreis) im Modul-Header**; nur Klick öffnet ein **verschiebbares** Panel mit Kurzbeschreibung, das ausschließlich über **„✕" oben rechts oder ESC** geschlossen wird. Pro Modul ein optionaler Hilfetext (`ModuleDescriptor::help`, sprachkodiert). **Mehrsprachig (Start EN + DE)**, Umschaltung per Combobox im JASS-Header. Details: `_bmad-output/implementation-artifacts/6-1-per-module-online-help.md`.
- **Modul-Größen Feintuning** (★★ / ★★★) – eigene BMAD-Story beim nächsten Mal: einige Module sind noch **unnötig groß/breit**. Größenklassen pro Modul überprüfen und straffen (z.B. Kandidaten mit wenigen Controls auf kleinere Klasse), ohne die AD-3-Regel (Rotary-Mindestgröße) und „XXS nur für 1-Control" zu verletzen.
