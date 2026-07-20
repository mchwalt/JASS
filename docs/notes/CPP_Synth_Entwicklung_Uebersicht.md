# C++ Synthesizer-Entwicklung – Umfassende Übersicht (Stand: März 2026)

> Recherche zu Frameworks, Libraries und Tools für die Entwicklung eines Software-Synthesizers in C++ unter Windows mit Visual Studio 2022/2026.

---

## Inhaltsverzeichnis
1. [Audio-Plugin-Frameworks](#1-audio-plugin-frameworks)
2. [DSP-Bibliotheken](#2-dsp-bibliotheken)
3. [Audio-I/O-Bibliotheken](#3-audio-io-bibliotheken)
4. [MIDI-Bibliotheken](#4-midi-bibliotheken)
5. [GUI-Bibliotheken](#5-gui-bibliotheken)
6. [Standalone vs. Plugin](#6-standalone-vs-plugin--ansatz)
7. [Open-Source-Referenzprojekte](#7-open-source-referenzprojekte)
8. [Empfehlung für den Einstieg](#8-empfehlung-für-den-einstieg)

---

## 1. Audio-Plugin-Frameworks

### 1.1 JUCE (Version 8.x)

**Beschreibung:** Das meistverwendete Framework für Audio-Plugin- und -Anwendungsentwicklung weltweit. JUCE ist ein umfassendes C++-Framework, das alles mitbringt – von Audio-Engine über GUI bis hin zu Plugin-Wrappern.

| Eigenschaft | Details |
|---|---|
| **Lizenz** | Dual: AGPLv3 (Open Source) oder Kommerziell. **JUCE Personal** = kostenlos bis $50.000 Umsatz/Jahr. Darüber: Indie ($40/Monat) oder Pro ($130/Monat) |
| **Plugin-Formate** | VST2, VST3, AU, AUv3, AAX, LV2, Standalone |
| **Plattformen** | Windows, macOS, Linux, iOS, Android |
| **Visual Studio** | Exzellente Unterstützung – generiert VS-Projektdateien direkt, CMake-Support |
| **Aktiv gepflegt** | Sehr aktiv – JUCE 8 (2024), aktuell v8.0.9. Gehört zu PACE/Acceleware |
| **Community** | Sehr groß – offizielles Forum, Discord, ADC-Konferenz, YouTube-Tutorials |
| **Lernkurve** | Mittel – viel Abstraktion, aber umfangreiches Framework. Guter Einstiegskurs seit Jan. 2026 |
| **Anfänger?** | **Ja** – Bester Einstieg für Plugin-Entwicklung. Kostenloser Kurs verfügbar |

**Vorteile:**
- All-in-One: Audio, GUI, Plugin-Wrapper, MIDI, Netzwerk, etc.
- Riesige Community und hervorragende Dokumentation
- JUCE 8: Direct2D-Renderer (Windows GPU-beschleunigt), WebView-UI, Animation-Framework
- Industriestandard – wird von den meisten kommerziellen Plugin-Herstellern verwendet
- CMake-basierter Build-Prozess
- Offizieller kostenloser Kurs (Jan. 2026) in Zusammenarbeit mit WolfSound Academy

**Nachteile:**
- Großes Framework – man lernt "JUCE" und nicht unbedingt die Grundlagen
- AGPLv3 bei Open Source = starke Copyleft-Pflicht
- Kommerziell bei > $50k Umsatz kostenpflichtig
- Abstraktionsebene kann das Verständnis der Grundlagen verdecken

---

### 1.2 iPlug2

**Beschreibung:** Leichtgewichtiges C++-Framework für Audio-Plugins. Nachfolger von WDL-OL/iPlug. Besonders für kleinere Projekte und schnelles Prototyping geeignet.

| Eigenschaft | Details |
|---|---|
| **Lizenz** | zlib-ähnlich (sehr liberal) – frei für kommerzielle und Closed-Source-Projekte |
| **Plugin-Formate** | CLAP, VST2, VST3, AUv2, AUv3, AAX, Web Audio Module (WAM) |
| **Plattformen** | Windows (ab Win 8), macOS, iOS, visionOS, Web (via Emscripten) |
| **Visual Studio** | Gute Unterstützung |
| **Aktiv gepflegt** | Aktiv – empfohlener Einstieg über iPlug2OOS (Out-of-Source) Repo |
| **Community** | Kleiner als JUCE, aber engagiert. iPlug2-Forum und GitHub |
| **Lernkurve** | Niedrig bis mittel – deutlich schlanker als JUCE |
| **Anfänger?** | **Bedingt** – guter Einstieg, aber weniger Lernressourcen als JUCE |

**Vorteile:**
- Sehr liberale Lizenz (keine Umsatzgrenzen, kein Copyleft)
- Schlanker als JUCE – weniger Overhead
- Unterstützt Web-Targets (WebAssembly via Emscripten)
- IGraphics-System mit NanoVG oder Skia als Rendering-Backend (GPU-beschleunigt)
- Kompiliert auch zu CLAP

**Nachteile:**
- Deutlich kleinere Community als JUCE
- Weniger Tutorials und Lernressourcen
- Keine so breite Industrieadoption

---

### 1.3 CLAP SDK

**Beschreibung:** CLAP (CLever Audio Plugin) ist ein offenes Plugin-Format (kein vollständiges Framework). Entwickelt 2022 von u-he und Bitwig als moderne Alternative zu VST3.

| Eigenschaft | Details |
|---|---|
| **Lizenz** | MIT – komplett frei, keine Registrierung oder Lizenzgebühren |
| **Plugin-Formate** | Nur CLAP (kein VST3/AU – dafür braucht man ein Framework) |
| **Plattformen** | Windows, macOS, Linux |
| **Visual Studio** | Ja, Header-Only SDK |
| **Aktiv gepflegt** | Aktiv – wachsende Adoption |
| **Community** | Wachsend – Discord, GitHub |
| **Lernkurve** | Hoch – Low-Level-API, C-basiertes ABI mit C++-Wrapper |
| **Anfänger?** | **Nein** – eher für Fortgeschrittene |

**Vorteile:**
- Völlig offenes Format – keine Lizenzkosten oder -bürokratie
- Per-Note-Automation und -Modulation (MIDI 2.0 kompatibel)
- Modernes Multithread-Design (Thread-Pool zwischen Host und Plugin)
- Schnelles Plugin-Scanning
- Erweiterbar durch Extension-System

**Nachteile:**
- Kein vollständiges Framework – man braucht eigene GUI, DSP etc.
- Noch nicht in allen DAWs unterstützt (aber wachsend: Bitwig, REAPER, MultitrackStudio u.a.)
- Für Anfänger zu Low-Level

**Hinweis:** CLAP kann über JUCE, iPlug2 oder DPF als Zielformat genutzt werden.

---

### 1.4 DPF (DISTRHO Plugin Framework)

**Beschreibung:** Leichtgewichtiges, Open-Source C++-Framework speziell für Plugin-Entwicklung mit Fokus auf Linux/Open-Source, aber auch Windows-kompatibel.

| Eigenschaft | Details |
|---|---|
| **Lizenz** | ISC (sehr permissiv, ähnlich MIT) |
| **Plugin-Formate** | LADSPA, DSSI, LV2, VST2, VST3, CLAP, JACK/Standalone |
| **Visual Studio** | Möglich, primär aber Makefile/CMake-basiert |
| **Aktiv gepflegt** | Aktiv |
| **Community** | Klein, Linux-fokussiert |
| **Lernkurve** | Niedrig bis mittel |
| **Anfänger?** | **Bedingt** – einfach, aber wenig Windows-spezifische Dokumentation |

---

### Vergleichsmatrix: Plugin-Frameworks

| Kriterium | JUCE | iPlug2 | CLAP SDK | DPF |
|---|---|---|---|---|
| Lizenz | AGPLv3 / Kommerziell | zlib (frei) | MIT (frei) | ISC (frei) |
| Vollständigkeit | All-in-One | Mittel | Nur Plugin-API | Mittel |
| GUI inkludiert | Ja (eigenes System) | Ja (IGraphics) | Nein | Ja (OpenGL/Cairo) |
| VST3-Support | Ja | Ja | Nein (nur CLAP) | Ja |
| Community-Größe | ★★★★★ | ★★★ | ★★★ | ★★ |
| Anfängerfreundlich | ★★★★ | ★★★ | ★ | ★★★ |
| Windows/VS-Support | ★★★★★ | ★★★★ | ★★★★ | ★★★ |

---

## 2. DSP-Bibliotheken

### 2.1 STK (Synthesis Toolkit in C++)

**Beschreibung:** Klassische Audio-DSP-Bibliothek von CCRMA (Stanford). Fokus auf algorithmische Synthese und Lehre.

| Eigenschaft | Details |
|---|---|
| **Lizenz** | MIT-ähnlich (frei) |
| **Schwerpunkt** | Physical Modeling, algorithmische Synthese, Wellenformen, Filter, Effekte |
| **Visual Studio** | Ja |
| **Aktiv gepflegt** | Mäßig – stabil, aber nicht häufig aktualisiert |
| **Lernkurve** | Niedrig – explizit für Lehre konzipiert |
| **Anfänger?** | **Ja** – ideal zum Lernen von DSP-Grundlagen |

**Enthält:** Oszillatoren, Filter, Hüllkurven, Physical-Modeling-Instrumente (Blas-, Streich-, Schlaginstrumente), Delay-Lines, Reverbs, FFT

---

### 2.2 Gamma

**Beschreibung:** Generische Sound-Synthese-Bibliothek von Lance Putnam (UC Santa Barbara). Eleganter, mathematisch orientierter Ansatz.

| Eigenschaft | Details |
|---|---|
| **Lizenz** | BSD-ähnlich (permissiv, frei) |
| **Schwerpunkt** | Generische Signal-Synthese und -Filterung, domänenunabhängig |
| **Visual Studio** | Ja |
| **Aktiv gepflegt** | Wenig aktiv – stabil, aber selten Updates |
| **Lernkurve** | Mittel – sauberes API-Design, aber akademisch |
| **Anfänger?** | **Bedingt** – gut für Lernende mit Mathe-Affinität |

**Besonderheit:** Arbeitet mit "Assignable Sampling Domains" – Algorithmen sind datentyp- und domänenunabhängig (Sound, Grafik, Frequenz, Zeit).

---

### 2.3 Maximilian

**Beschreibung:** Einfache, selbstständige C++-Bibliothek für Audio-Synthese und Signal-Processing. Keine externen Abhängigkeiten.

| Eigenschaft | Details |
|---|---|
| **Lizenz** | MIT (frei) |
| **Schwerpunkt** | Oszillatoren, Filter, Effekte, Granularsynthese, Sample-Playback, MIR |
| **Visual Studio** | Ja – kompiliert ohne Abhängigkeiten |
| **Aktiv gepflegt** | Mäßig aktiv |
| **Lernkurve** | **Sehr niedrig** – Einsteigerfreundlichste DSP-Lib |
| **Anfänger?** | **Ja** – ideal für absolute Einsteiger |

**Enthält:** Oszillatoren, Filter, Hüllkurven, Delay, Distortion, Chorus, Flanging, Granularsynthese, FFT/Spektralanalyse, Stereo/Quadraphonic/Ambisonic-Mixing, WAV/OGG-Lesen

---

### 2.4 FAUST

**Beschreibung:** Funktionale Programmiersprache für Signal-Processing, die nach C++, LLVM, WebAssembly u.a. kompiliert. Kein klassisches C++-Library, sondern ein DSP-Compiler.

| Eigenschaft | Details |
|---|---|
| **Lizenz** | GPLv2 (Compiler), generierter Code ist lizenzfrei nutzbar |
| **Schwerpunkt** | DSP-Algorithmen in funktionaler Syntax → generiert C++-Code |
| **Visual Studio** | Generierter C++-Code ist in VS verwendbar |
| **Aktiv gepflegt** | Sehr aktiv – GRAME (Frankreich) |
| **Lernkurve** | Mittel – eigene Sprache lernen, aber DSP wird sehr kompakt beschrieben |
| **Anfänger?** | **Bedingt** – gut für DSP-Prototyping, aber man muss die FAUST-Sprache lernen |

**Vorteile:**
- Extrem effiziente Code-Generierung
- Rapid Prototyping von DSP-Algorithmen
- Kann direkt Plugin-Code (VST, AU, etc.) generieren
- Online-IDE (Faust Playground) zum Experimentieren

**Nachteile:**
- Eigene Sprache – nicht direkt C++
- Integration in bestehende C++-Projekte erfordert Extra-Arbeit

---

### Vergleichsmatrix: DSP-Bibliotheken

| Kriterium | STK | Gamma | Maximilian | FAUST |
|---|---|---|---|---|
| Lizenz | MIT-ähnlich | BSD | MIT | GPLv2 (Compiler) |
| Einstieg | ★★★★★ | ★★★ | ★★★★★ | ★★★ |
| Feature-Umfang | ★★★★ | ★★★ | ★★★★ | ★★★★★ |
| Abhängigkeiten | Minimal | Minimal | Keine | Eigener Compiler |
| Für Synth-Bau | ★★★★ | ★★★★ | ★★★★ | ★★★★★ |

---

## 3. Audio-I/O-Bibliotheken (für Standalone-Apps)

### 3.1 RtAudio

| Eigenschaft | Details |
|---|---|
| **Beschreibung** | C++-Klassen für Echtzeit-Audio-I/O |
| **Lizenz** | MIT-ähnlich (frei) |
| **Backends (Windows)** | DirectSound, ASIO, WASAPI |
| **Integration** | Einzelne .cpp + .h Datei – direkt ins Projekt kopierbar |
| **Aktiv gepflegt** | Mäßig – stabil |
| **Lernkurve** | Niedrig |
| **Anfänger?** | **Ja** – sehr einfach zu verwenden |

### 3.2 PortAudio

| Eigenschaft | Details |
|---|---|
| **Beschreibung** | C-basierte, plattformübergreifende Audio-I/O-Bibliothek |
| **Lizenz** | MIT (frei) |
| **Backends (Windows)** | DirectSound, ASIO, WASAPI, MME |
| **Aktiv gepflegt** | Aktiv |
| **Lernkurve** | Niedrig |
| **Anfänger?** | **Ja** – weit verbreitet, viele Beispiele |

### 3.3 ASIO SDK (Steinberg)

| Eigenschaft | Details |
|---|---|
| **Beschreibung** | Steinbergs Low-Latency Audio-Treiber-API |
| **Lizenz** | Proprietär – kostenlos zum Download, aber Redistribution eingeschränkt |
| **Anmerkung** | Wird typischerweise über RtAudio/PortAudio genutzt, nicht direkt |
| **Anfänger?** | **Nein** – zu Low-Level für den Einstieg |

### 3.4 miniaudio

| Eigenschaft | Details |
|---|---|
| **Beschreibung** | Single-Header Audio-Playback/Capture-Bibliothek in C |
| **Lizenz** | MIT / Public Domain (Unlicense) |
| **Lernkurve** | Niedrig – Single-Header, keine Abhängigkeiten |
| **Anfänger?** | **Ja** – extrem einfache Integration |

### Empfehlung Audio-I/O
Für ein **Standalone-Projekt** sind RtAudio oder PortAudio die beste Wahl. Wenn du **JUCE oder iPlug2** verwendest, brauchst du keine separate Audio-I/O-Lib – das ist bereits integriert.

---

## 4. MIDI-Bibliotheken

### 4.1 RtMidi

| Eigenschaft | Details |
|---|---|
| **Beschreibung** | C++-Klassen für Echtzeit-MIDI-I/O |
| **Lizenz** | MIT-ähnlich (frei) |
| **Backends (Windows)** | Windows Multimedia Library, UWP |
| **Lernkurve** | Niedrig |
| **Anfänger?** | **Ja** |
| **Einschränkung** | Kein MIDI-2.0-Support, kein Timing (Nachrichten werden sofort gesendet) |

### 4.2 libremidi

| Eigenschaft | Details |
|---|---|
| **Beschreibung** | Moderner Fork/Rewrite von RtMidi in C++17/20 |
| **Lizenz** | BSD-2-Clause (frei) |
| **Besonderheiten** | MIDI 2.0 Support (experimentell), Hotplug-Support, Header-Only möglich, weniger Speicher-Allokationen |
| **Lernkurve** | Niedrig bis mittel |
| **Anfänger?** | **Ja** – modernere API als RtMidi |

### Empfehlung MIDI
- **Für neue Projekte:** libremidi (moderner, MIDI 2.0 ready)
- **Für einfachen Einstieg:** RtMidi (mehr Tutorials verfügbar)
- **Mit JUCE/iPlug2:** Eingebauter MIDI-Support – keine extra Lib nötig

---

## 5. GUI-Bibliotheken für Synth-UIs

### 5.1 JUCE GUI

- Eigenes, vollständiges GUI-System mit Component-Hierarchie
- Custom Look-and-Feel für individuelle Designs
- Seit JUCE 8: Direct2D auf Windows (GPU-beschleunigt), WebView-UI (HTML/CSS/JS für UI möglich), Animations-Framework
- **Vorteil:** Eng mit dem Plugin-Framework verzahnt
- **Nachteil:** JUCE-spezifisch, nicht außerhalb von JUCE nutzbar

### 5.2 IGraphics (iPlug2)

- Eigenes Grafik-System in iPlug2
- Backends: NanoVG (OpenGL) oder Skia (GPU-beschleunigt)
- HiDPI/Retina-Support, Skalierung
- Vektorbasiertes Rendering
- **Vorteil:** Leichtgewichtig, performant
- **Nachteil:** Nur innerhalb von iPlug2

### 5.3 Dear ImGui

- Immediate-Mode GUI-Bibliothek (kein Retained-Mode wie JUCE)
- **Lizenz:** MIT (frei)
- Extrem schnelles Prototyping
- Projekte wie `clap-imgui` und `ImSynth` zeigen Integration mit Audio
- DirectX12 auf Windows, Metal auf macOS
- **Vorteil:** Schnell zu lernen, ideal für Prototypen und Tools
- **Nachteil:** Nicht für polierte, kommerzielle Plugin-UIs geeignet. Kein natives Plugin-GUI-Embedding ohne Wrapper

### 5.4 VSTGUI (Steinberg)

- Steinbergs eigene GUI-Lib für VST-Plugins
- **Lizenz:** BSD-3-Clause (frei)
- Wird in manchen älteren Projekten verwendet
- **Nachteil:** Veraltet im Vergleich zu JUCE/iPlug2-GUIs

### 5.5 Pugl + OpenGL/Vulkan

- Minimale, plattformübergreifende Fenster-Bibliothek für OpenGL/Vulkan/Cairo
- Wird von einigen Plugin-Entwicklern für Custom-Rendering verwendet
- **Für Fortgeschrittene** – maximale Kontrolle, aber viel Eigenarbeit

### Empfehlung GUI
- **Anfänger:** JUCE GUI (alles integriert) oder Dear ImGui (für Prototypen)
- **Fortgeschritten:** IGraphics (iPlug2) oder Custom OpenGL/Vulkan mit Pugl

---

## 6. Standalone vs. Plugin – Ansatz

### Standalone-Anwendung

| Vorteile | Nachteile |
|---|---|
| Einfacherer Einstieg – kein Plugin-Host nötig | Kein Integration in DAWs |
| Direktes Audio-I/O (RtAudio/PortAudio) | MIDI-Routing muss selbst implementiert werden |
| Schnelleres Debugging (einfach starten, kein Plugin-Scan) | Kein Preset-Management durch Host |
| Volle Kontrolle über den Audio-Thread | Keine Automation durch DAW |
| Gut zum Lernen der DSP-Grundlagen | Muss eigenes Fenster-Management implementieren |

### Plugin (VST3/CLAP/AU)

| Vorteile | Nachteile |
|---|---|
| Integration in jede DAW (REAPER, Ableton, etc.) | Komplexerer Build-Prozess |
| MIDI wird vom Host bereitgestellt | Debugging schwieriger (Plugin im Host laden) |
| Automation, Presets, Session-Save durch Host | Framework-Abhängigkeit (JUCE, iPlug2 etc.) |
| Professioneller Einsatz möglich | Abstraktion verdeckt teilweise die Grundlagen |
| Multi-Instanz-fähig | Plugin-Format-Compliance (Validierung, Scanning) |

### Empfehlung für ein Lernprojekt

**Phase 1 – Grundlagen lernen (Standalone):**
- Starte mit einer Standalone-App (z.B. mit RtAudio + RtMidi oder Maximilian)
- Implementiere einen einfachen Oszillator, Filter, ADSR-Hüllkurve
- Lerne Audio-Buffers, Sample-Rate, Block-Processing
- Optional: Dear ImGui für schnelles UI-Prototyping

**Phase 2 – Plugin-Entwicklung:**
- Portiere das Gelernte in ein JUCE- oder iPlug2-Plugin
- Nutze das Framework für GUI und Plugin-Wrapper
- Teste in REAPER als VST3 oder CLAP

**Alternativer Direkteinstieg:**
- Starte direkt mit JUCE (das auch Standalone-Builds unterstützt)
- Der offizielle JUCE-Kurs (2026) ist ideal dafür

---

## 7. Open-Source-Referenzprojekte

### 7.1 Surge XT

| Eigenschaft | Details |
|---|---|
| **GitHub** | [surge-synthesizer/surge](https://github.com/surge-synthesizer/surge) |
| **Lizenz** | GPLv3 |
| **Technologie** | C++, JUCE (Plugin-Wrapper), eigene DSP-Engine mit SSE2 |
| **Synthese** | Subtraktiv, FM, Wavetable, Physical Modeling |
| **Lernwert** | Hoch – gut dokumentierte Architektur (`doc/Surge Architecture.md`), aber komplexer Codebase |
| **Geeignet für** | Fortgeschrittene – der Code nutzt SSE2-Intrinsics und ist hochoptimiert |

### 7.2 Vital / Vitalium

| Eigenschaft | Details |
|---|---|
| **GitHub** | [mtytel/vital](https://github.com/mtytel/vital) |
| **Lizenz** | GPLv3 (kommerziell lizenzierbar) |
| **Technologie** | C++, JUCE |
| **Synthese** | Spectral Warping Wavetable |
| **Lernwert** | Hoch – modernes UI, Drag-and-Drop-Modulation, animierte Visualisierung |
| **Geeignet für** | Fortgeschrittene |

### 7.3 Helm

| Eigenschaft | Details |
|---|---|
| **GitHub** | [mtytel/helm](https://github.com/mtytel/helm) |
| **Lizenz** | GPLv3 |
| **Technologie** | C++, JUCE |
| **Synthese** | Subtraktiv, polyphon |
| **Lernwert** | **Sehr hoch** – saubere, übersichtliche Architektur, ideal zum Studieren |
| **Geeignet für** | Anfänger bis Fortgeschrittene |

### 7.4 Dexed

| Eigenschaft | Details |
|---|---|
| **GitHub** | [asb2m10/dexed](https://github.com/asb2m10/dexed) |
| **Lizenz** | GPLv3 |
| **Technologie** | C++, JUCE |
| **Synthese** | FM-Synthese (DX7-Klon) |
| **Lernwert** | Hoch – zeigt FM-Synthese-Implementierung |
| **Geeignet für** | FM-Synthese-Interessierte |

### 7.5 Odin 2

| Eigenschaft | Details |
|---|---|
| **GitHub** | [TheWaveWarden/odin2](https://github.com/TheWaveWarden/odin2) |
| **Lizenz** | GPLv3 |
| **Technologie** | C++, JUCE |
| **Synthese** | Semi-modular, Subtraktiv, FM, Wavetable, Vector |
| **Lernwert** | Hoch – guter Mittelweg zwischen Komplexität und Lesbarkeit |
| **Geeignet für** | Anfänger bis Fortgeschrittene |

### 7.6 OB-Xd

| Eigenschaft | Details |
|---|---|
| **GitHub** | [reales/OB-Xd](https://github.com/reales/OB-Xd) |
| **Lizenz** | GPLv3 |
| **Technologie** | C++, JUCE |
| **Synthese** | Analog-Modeling (Oberheim OB-X Emulation) |
| **Lernwert** | Mittel bis Hoch – zeigt Virtual-Analog-Techniken |

---

## 8. Empfehlung für den Einstieg

### Empfohlener Stack für Anfänger

```
Framework:  JUCE 8 (kostenlos bis $50k, All-in-One)
IDE:        Visual Studio 2022/2026 Community Edition
DSP-Lernen: Maximilian (zum Verstehen) → dann JUCE-eigene DSP-Klassen
MIDI:       In JUCE integriert
GUI:        JUCE GUI-System
Test-DAW:   REAPER (bereits installiert)
```

### Alternativer Stack (maximale Freiheit, keine Lizenz-Einschränkungen)

```
Framework:  iPlug2 (zlib-Lizenz, komplett frei)
IDE:        Visual Studio 2022/2026 Community Edition
DSP:        STK oder Maximilian als Lernhilfe
MIDI:       In iPlug2 integriert
GUI:        IGraphics (in iPlug2 integriert)
Test-DAW:   REAPER
```

### Lernpfad-Vorschlag

1. **C++ Grundlagen auffrischen** (falls nötig)
2. **JUCE installieren und den offiziellen Kurs durcharbeiten** (kostenlos, 2026)
3. **Einfachen Synth bauen:** 1 Oszillator → Filter → ADSR → Audio-Output
4. **Helm-Quellcode studieren** (sauberste Architektur zum Lernen)
5. **Polyphonie hinzufügen** (Voice-Management)
6. **Modulation-System bauen** (LFOs, Envelopes → Parameter)
7. **GUI gestalten** (Knobs, Slider, Waveform-Display)
8. **Als VST3/CLAP in REAPER testen**
9. **Surge XT / Odin 2 Code studieren** für fortgeschrittene Techniken

---

## Quellen

- [JUCE Official Website](https://juce.com/)
- [JUCE GitHub Repository](https://github.com/juce-framework/JUCE)
- [JUCE Free Plugin Development Course (2026)](https://bedroomproducersblog.com/2026/01/19/juce-plugin-development-course/)
- [How To Learn Audio Plugin Development With JUCE in 2026](https://conference.audio.dev/how-to-learn-audio-plugin-development-with-juce-in-2026-for-free-jan-wilczek-tom-poole-adc)
- [iPlug2 Official Website](https://iplug2.github.io/)
- [iPlug2 GitHub Repository](https://github.com/iPlug2/iPlug2)
- [CLAP Audio Plugin API](https://github.com/free-audio/clap)
- [CLAP Overview (u-he)](https://u-he.com/community/clap/)
- [DPF – DISTRHO Plugin Framework](https://github.com/DISTRHO/DPF)
- [STK – Synthesis Toolkit in C++](https://ccrma.stanford.edu/software/stk/)
- [Gamma DSP Library](https://github.com/LancePutnam/Gamma)
- [Maximilian](https://github.com/micknoise/Maximilian)
- [FAUST Programming Language](https://faust.grame.fr/)
- [RtAudio](https://github.com/thestk/rtaudio)
- [PortAudio](https://portaudio.com/)
- [RtMidi](https://github.com/thestk/rtmidi)
- [libremidi](https://github.com/celtera/libremidi)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [clap-imgui Demo](https://github.com/free-audio/clap-saw-demo-imgui)
- [ImSynth (ImGui + PortAudio Synth)](https://github.com/syyePhenomenol/ImSynth)
- [Surge XT](https://github.com/surge-synthesizer/surge)
- [Vital](https://github.com/mtytel/vital)
- [Helm](https://github.com/mtytel/helm)
- [Dexed](https://github.com/asb2m10/dexed)
- [Odin 2](https://github.com/TheWaveWarden/odin2)
- [Audio Developer Conference](https://conference.audio.dev/)
- [Getting Started with Audio Programming (audiodev.blog)](https://audiodev.blog/newbie-resources/)
- [Awesome Music DSP](https://github.com/olilarkin/awesome-musicdsp)
- [CLAP vs VST3 (martinic.com)](https://www.martinic.com/en/blog/clap-audio-plugin-format)
