# Glossary — Begriffe & Abkürzungen (JASS)

Globales Nachschlagewerk für Abkürzungen und Fachbegriffe, die in diesem Projekt
(Code, Doku, BMAD-Artefakte, Commits) auftauchen. Neue Begriffe bitte hier ergänzen.

Die Entwickler-Dokus ([`ARCHITECTURE.md`](ARCHITECTURE.md),
[`MODULE_SYSTEM.md`](MODULE_SYSTEM.md), [`DEVELOPER_GUIDE.md`](DEVELOPER_GUIDE.md))
verlinken auf die Anker dieser Datei (z. B. `Glossary.md#apvts`). Jeder Eintrag
trägt dafür einen `<a id="…">`-Anker — beim Ergänzen bitte beibehalten.

---

## JUCE / Audio / Plugin

| Begriff | Bedeutung |
|---------|-----------|
| <a id="juce" name="juce"></a>**JUCE** | C++-Framework für Audio-Anwendungen und Plugins (als Git-Submodule eingebunden, aktuell 9.0.0). |
| <a id="apvts" name="apvts"></a>**APVTS** | `juce::AudioProcessorValueTreeState` — zentraler Parameter-Baum und **einzige Quelle der Wahrheit** für alle Parameter; verbindet Parameter mit UI-Controls (Attachments) und Persistenz. Einziger erlaubter Cross-Modul-Kanal (AD-9). |
| <a id="attachment" name="attachment"></a>**Attachment** | JUCE-Bindeglied zwischen einem Control und einem APVTS-Parameter (`SliderAttachment`, `ComboBoxAttachment`, `ButtonAttachment`). Mappt bei Combos **per Index** (Quelle der Combo-Index-Bug-Klasse). |
| <a id="dsp" name="dsp"></a>**DSP** | *Digital Signal Processing* — die eigentliche Audio-Signalverarbeitung (`Source/DSP/`). |
| <a id="rt" name="rt"></a>**RT / RT-Safety** | *Real-Time* — Audio-Thread-Regeln: keine Allokation/Locks/Logs auf dem Audio-Thread; UI↔Audio nur über `std::atomic` + APVTS. |
| <a id="audio-thread" name="audio-thread"></a>**Audio-Thread / Message-Thread** | Die zwei relevanten Threads: der Audio-Thread rendert `processBlock` (RT-Regeln!), der Message-Thread bedient UI, Timer und Preset-I/O. |
| <a id="voice" name="voice"></a>**Voice** | Eine Synthesizer-Stimme (`SynthVoice`, 8 Stück) mit eigener kompletter Generator- + Effektkette. |
| <a id="standalone" name="standalone"></a>**Standalone** | Die eigenständige App (`JASS.exe`, Target `JASS_Standalone`) — der Haupt-Auslieferungsweg. |
| <a id="vst3" name="vst3"></a>**VST3** | Plugin-Format für DAWs (z. B. REAPER). Installationspfad: `C:\Program Files\Common Files\VST3\JASS.vst3` (kein Auto-Install — manuell kopieren). |
| <a id="daw" name="daw"></a>**DAW** | *Digital Audio Workstation* — Host für Plugins (Referenz-Host hier: REAPER). |
| <a id="processblock" name="processblock"></a>**processBlock** | JUCE-Callback, in dem pro Audio-Block die Samples erzeugt/verarbeitet werden — inkl. Bus-Effekten und globalem Modulations-Pfad. |
| <a id="tu" name="tu"></a>**TU** | *Translation Unit* (Übersetzungseinheit) — eine `.cpp` samt aller inkludierten Header, wie der Compiler sie sieht. |
| <a id="submodule" name="submodule"></a>**Submodule** | Git-Mechanismus, mit dem JUCE eingebunden ist: das Haupt-Repo speichert nur einen Commit-Zeiger, nicht die JUCE-Dateien. |

## Build, Versionierung & Release

| Begriff | Bedeutung |
|---------|-----------|
| <a id="calver" name="calver"></a>**CalVer** | *Calendar Versioning* — App-Version `YYYY.MM.MICRO` (z. B. `2026.07.15`). Quelle: `JASS_CALVER` in `CMakeLists.txt`; die CI setzt sie pro Release, ohne einen Bump zu committen. |
| <a id="micro" name="micro"></a>**MICRO** | Dritte CalVer-Stelle = Zähler der Releases im laufenden Monat (CI: Anzahl vorhandener `vYYYY.MM.*`-Tags). |
| <a id="ci" name="ci"></a>**CI** | *Continuous Integration* — die GitHub-Actions-Pipeline (`.github/workflows/release.yml`): baut bei jedem Merge nach `main` Windows- + Linux-Artefakte und veröffentlicht ein Release. Kein PR-Build! |
| <a id="formatversion" name="formatversion"></a>**FormatVersion** | Integer-Schema-Vertrag des Preset-Formats (`PresetIO::kFormatVersion`, aktuell **6**) — unabhängig von der CalVer-App-Version. Bump nur bei *Bedeutungs*-Änderungen; neue Felder laufen über missing⇒default. |
| <a id="append-only" name="append-only"></a>**append-only** | Kompatibilitäts-Grundregel des Projekts: Parameter-IDs, Choice-/Enum-Reihenfolgen, Katalog-Einträge und Preset-Felder werden nie umbenannt, umsortiert oder gelöscht — Neues wird hinten angehängt. |
| <a id="odr" name="odr"></a>**ODR / ABI-Falle** | *One Definition Rule* — wächst ein Header-Struct (z. B. Mod-Slot-Arrays), mischt ein **inkrementeller** Build TUs mit alter und neuer Struct-Größe → Heap-Korruption/Absturz beim Start. Gegenmittel: Clean Rebuild. |
| <a id="clean-rebuild" name="clean-rebuild"></a>**Clean Rebuild** | MSBuild mit `/t:Rebuild` — Pflicht nach Header-Struct-Größenänderungen und empfohlen nach `*Specs.h`-/Resource-Änderungen. |
| <a id="binary-data" name="binary-data"></a>**Binary Data** | Per `juce_add_binary_data` in die EXE einkompilierte Assets (Hilfetexte EN/DE, Demo-Presets, Wavetables, Samples) — je Resource-Familie ein eigenes Target mit eigenem Namespace. |
| <a id="seeding" name="seeding"></a>**Seeding** | Idempotentes Ausrollen der eingebetteten Assets nach `%AppData%\JASS` beim ersten Start — vorhandene (auch vom User geänderte) Dateien werden nie überschrieben. |

## Modul-Spec-System

| Begriff | Bedeutung |
|---------|-----------|
| <a id="spec" name="spec"></a>**Spec / `<Name>Specs.h`** | Die deklarative Ein-Ort-Definition eines Moduls (`Source/Modules/`): aus ihr werden APVTS-Parameter, Rack-Descriptor und `.jass`-Persistenz **generiert**. Handgeschrieben bleiben DSP und `applyToVoice`-Verdrahtung. |
| <a id="paramspec" name="paramspec"></a>**ParamSpec** | Die audio-sichere Beschreibung eines Parameters (ID, persistKey, UI-Label, Kind, Range, Default, Choices, modTarget, …) — Feldreferenz in [`MODULE_SYSTEM.md`](MODULE_SYSTEM.md#2-paramspec-audio-safe-half--sourcemodulesparamspech). |
| <a id="modulespec" name="modulespec"></a>**ModuleSpec** | Die UI-Hälfte der Modul-Deklaration (id, title, persistObject, zone, size, enableParamId, params, …); `makeModuleDescriptor` erzeugt daraus den Rack-Descriptor. |
| <a id="registry" name="registry"></a>**Registry** | `ModuleRegistry` + `AllModules.h`: `Modules::all()` ist die geordnete Liste aller Specs — ihre Reihenfolge **ist** die APVTS-Parameter-Reihenfolge (append-only!). |
| <a id="persistkey" name="persistkey"></a>**persistObject / persistKey** | JSON-Schlüssel eines Moduls bzw. eines Feldes in der `.jass`-Datei (z. B. `"Filter"` / `"Cutoff"`). |
| <a id="indexisvalue" name="indexisvalue"></a>**indexIsValue** | Combo-Modus, der den Item-Index direkt als Parameterwert schreibt und das `ComboBoxAttachment` umgeht — nötig bei Combos mit variabler Item-Anzahl (SAMPLER-SET, MOD-MATRIX-PARAM). |
| <a id="helpid" name="helpid"></a>**helpId** | Hilfe-Slug eines Moduls; erlaubt geteilte Hilfetexte (LFO 1–4 → `lfo.md`, OSC 1–3 → `osc1.md`). Leer ⇒ Modul-`id`. |

## JASS Rack-Framework (UI)

| Begriff | Bedeutung |
|---------|-----------|
| <a id="rack" name="rack"></a>**Rack** | Das einheitliche 19″-Rack-Layout — besitzt **alle** Modul-Platzierung auf dem **24-Spalten-Grid** × Rack-Unit-Zeilen (114 px) (`UI/rack/Rack.{h,cpp}`, AD-2). |
| <a id="zone" name="zone"></a>**Zone** | Gruppierung im Rack mit eigenem Header (Enable/Reset/Info): **MASTER BUS**, **GENERATORS**, **MODULATION**, **PROCESSING**, **VISUALIZATION**, **INPUT**. |
| <a id="moduleframe" name="moduleframe"></a>**ModuleFrame** | Generischer Modul-Rahmen (Header + Body); rendert ein Modul aus seinem Deskriptor und besitzt alle APVTS-Attachments (AD-6). |
| <a id="moduledescriptor" name="moduledescriptor"></a>**ModuleDescriptor** | Deklarative Beschreibung eines Moduls (Größenklasse, Titel, Typ-Tag, Enable-Param, Body-Elemente). Trägt eine stabile `id` (= Layout-Schlüssel). |
| <a id="bodyelement" name="bodyelement"></a>**BodyElement** | Ein Bestandteil des Modul-Bodys: `Knob`, `Combo`, `Toggle`, `Action`, `FileAction`, `Caption`, `Display`. |
| <a id="caption" name="caption"></a>**Caption** | Statisches Text-Element im Body (heißt so wegen Namenskollision mit `juce::Label`). |
| <a id="sizeclass" name="sizeclass"></a>**SizeClass / Größenklasse** | Grid-Footprint eines Moduls, benannt `W{Spalten}H{Reihen}` auf dem 24er-Raster (z. B. `W4H1`, `W24H2`). Eine Daten-Tabelle (`sizeClassSpec`) definiert alle. |
| <a id="mod-ring" name="mod-ring"></a>**Mod-Ring** | Live-Modulations-Ring um einen Knob, der die aktuelle Modulation (Quelle: Mod-Matrix) anzeigt; deklarativ über `modTarget` (AD-8). |
| <a id="typetag" name="typetag"></a>**typeTag** | Identitäts-/Farb-Tag eines Moduls (Generator/Modulator/Processor). |
| <a id="enableparam" name="enableparam"></a>**enableParam / enabledWhen** | Zwei Quellen für den Enable-Zustand eines Moduls: echter Bool-Parameter (Header-Toggle) bzw. abgeleitetes Prädikat (liest nur APVTS). Bei beidem gilt UND. |
| <a id="activewhen" name="activewhen"></a>**activeWhen** | Per-Knob-Relevanz-Prädikat: dimmt einen einzelnen Knopf (z. B. PAN in Mono-Modi), ohne das Modul zu deaktivieren. |
| <a id="auto-fit" name="auto-fit"></a>**Auto-Fit** | Fensterbreite fix 1520 px; die Höhe folgt automatisch dem sichtbaren Rack, auf kleinen Displays wird die ganze UI herunterskaliert (AD-12). |

## Preset & State

| Begriff | Bedeutung |
|---------|-----------|
| <a id="jass-format" name="jass-format"></a>**`.jass`** | Das Preset-Format: JSON, **ein verschachteltes Objekt pro Modul** (spec-getrieben), FormatVersion 6. Laden = erst Werksreset, dann Datei-Werte darüber (missing ⇒ Werks-Default). Spez: [`JASS_Preset_Format.md`](JASS_Preset_Format.md). |
| <a id="livestate" name="livestate"></a>**LiveState** | `%AppData%\JASS\LiveState.jass` — automatisch gespeicherter Arbeitszustand der Standalone-App (debounced ~1,5 s), beim Start geladen. Im DAW-Betrieb ungenutzt (der Host besitzt den State). |
| <a id="racklayout" name="racklayout"></a>**RackLayout** | Persistiertes Rack-Layout (Sichtbarkeit, Reihenfolge, Zone, L/R-Ausrichtung) — als JSON-String-Property auf dem APVTS-State, gespiegelt ins Preset-Feld `"RackLayout"` (nur wenn nicht Werks-Default). |
| <a id="preset-bank" name="preset-bank"></a>**Preset-Bank** | Die F1–F12-Schnellzugriffe im PRESETS-Modul (Einzeldruck = laden, Doppeldruck = belegen); global in `%AppData%\JASS\PresetBanks.json`, nicht Teil eines Presets. |
| <a id="appdata" name="appdata"></a>**AppData** | `%AppData%\Roaming\JASS\` — alle Laufzeitdaten (Presets, LiveState, Wavetables, Samples, Settings); Tabelle in [`DEVELOPER_GUIDE.md`](DEVELOPER_GUIDE.md#7-runtime-configuration-surfaces). |
| <a id="migration" name="migration"></a>**Migration** | Automatisches Hochziehen älterer Presets auf die aktuelle FormatVersion (mit Backup nach `PresetsBackup\`), plus der einmalige AppData-Umzug `Synthy` → `JASS`. |

## Synthesizer-Begriffe (Module & Engine)

| Begriff | Bedeutung |
|---------|-----------|
| <a id="osc" name="osc"></a>**OSC** | *Oscillator* — Grund-Klangerzeuger (OSC 1–3: Sine/Saw/Square/Triangle, Unison, Self-FM). |
| <a id="unison" name="unison"></a>**Unison / Detune** | Mehrfach-Stimmen pro Oszillator (1–7) mit Verstimmung — macht den Klang breiter/fetter. |
| <a id="self-fm" name="self-fm"></a>**Self-FM (FB)** | Feedback-Regler pro OSC: der Oszillator moduliert seine eigene Phase mit dem vorigen Ausgang — Sinus morpht Richtung Sägezahn, oben chaotisch. |
| <a id="sub" name="sub"></a>**Sub** | Sub-Oszillator (−1/−2 Oktaven, folgt OSC 1). |
| <a id="noise" name="noise"></a>**Noise** | Rausch-Generator (White/Pink/Brown/Blue). |
| <a id="karplus" name="karplus"></a>**Karplus / Pluck** | Karplus-Strong-Saitensynthese; **PLUCK** löst das Anzupfen aus (Button/Leertaste), gespielt über die Klaviatur. |
| <a id="wavetable" name="wavetable"></a>**Wavetable** | Klangerzeugung aus einer Wellenform-Tabelle mit Positions-Morph; **LOAD WAV** importiert eigene Tabellen. |
| <a id="sampler" name="sampler"></a>**SAMPLER** | Spielt eigene Aufnahmen (WAV/AIFF, ≤60 s) als Klangquelle durch die ganze Kette: ROOT-Transponierung, START/END, One-Shot/Loop/Reverse, SPEED; Stereo-Dateien als zwei platzierte Teilquellen; Presets referenzieren Samples per **Name**. |
| <a id="crossmod" name="crossmod"></a>**CROSS MOD** | Kreuzmodulation zweier wählbarer OSCs (SRC → DEST): **RingMod** oder **FM**; Modul aus = additive Mischung. (Hieß früher „Mix Mode".) |
| <a id="adsr" name="adsr"></a>**ADSR** | Hüllkurve *Attack–Decay–Sustain–Release* — formt die Lautstärke über die Zeit; zugleich Mod-Matrix-Quelle. |
| <a id="pitchenv" name="pitchenv"></a>**Pitch-Env** | Pitch-Hüllkurve — biegt die Tonhöhe beim Notenstart (osc/wavetable/sub). |
| <a id="lfo" name="lfo"></a>**LFO** | *Low-Frequency Oscillator* — langsamer Oszillator (4 Stück, tempo-syncbar). Moduliert **ausschließlich** über die Mod-Matrix (kein eingebautes Ziel mehr). |
| <a id="modmatrix" name="modmatrix"></a>**Mod-Matrix** | 8 Routing-Slots **Quelle → MODUL → PARAM** mit bipolarem Amount; Quellen: LFO 1–4, Envelope, Velocity. Slots stapeln (mehrere Quellen auf ein Ziel summieren); aktive Slots schalten Quelle + Zielmodul automatisch ein. |
| <a id="lfotarget" name="lfotarget"></a>**LFOTarget / ModDest** | Die zwei Katalog-Ebenen der Matrix-Ziele: `LFOTarget` = flaches DSP-Vokabular (`ModTargets.h`), `ModDest` = zweistufiger UI-Katalog MODUL → PARAM (`ModMatrixCatalog.h`). Beide append-only. |
| <a id="velocity" name="velocity"></a>**Velocity** | Anschlagstärke der gespielten Note — Mod-Matrix-Quelle (pro Voice in `startNote` erfasst). |
| <a id="velocity-layer" name="velocity-layer"></a>**Velocity-Layer** | Mehrfach-Aufnahmen derselben Taste bei verschiedenen Anschlagstärken in einer Sample-Bibliothek — hart angeschlagene Saiten klingen nicht nur lauter, sondern auch obertonreicher. Die `.sfz` wählt per `lovel`/`hivel` die Schicht zur gespielten Velocity; JASS importiert bewusst nur **eine** Schicht (die lauteste, `hivel`-Ranking). |
| <a id="dynamics" name="dynamics"></a>**pp / mp / mf / ff** | Die klassischen musikalischen **Dynamikstufen** (italienisch): *pianissimo* (sehr leise), *mezzopiano* (halb leise), *mezzoforte* (halb laut), *fortissimo* (sehr laut). Sample-Bibliotheken benennen ihre [Velocity-Layer](#velocity-layer) danach (z. B. `FF A2.flac` = Ton A2, sehr laut angeschlagen). |
| <a id="arp" name="arp"></a>**Arpeggiator (Arp)** | Zerlegt einen gehaltenen Akkord in eine rhythmische Notenfolge (Up/Down/UpDown/Random) — per MIDI-Umschreiben im `processBlock`. |
| <a id="glide" name="glide"></a>**Glide (Portamento)** | Gleitende Tonhöhe zwischen Noten (Mono/Legato/Poly), läuft nach dem Arp und glidet dadurch auch Arp-Schritte. |
| <a id="drone" name="drone"></a>**Drone / Auto-Play** | Automatische Dauernote (C4 auf MIDI-Kanal 16), solange ein Generator aktiv ist und keine Taste gehalten wird — weicht beim Spielen, kehrt bei Generator-Aktivierung zurück. |
| <a id="filter" name="filter"></a>**Filter** | Biquad-Frequenzfilter (Lowpass/Highpass, Cutoff/Resonanz). |
| <a id="formant" name="formant"></a>**Formant** | Formant-Filter — formt Vokal-Charakter (VOWEL/RESO/MIX); braucht obertonreiches Eingangsmaterial. |
| <a id="distortion" name="distortion"></a>**Distortion** | Verzerrung (Soft Clip / Hard Clip / Foldback). |
| <a id="wavefold" name="wavefold"></a>**Wavefold / Bitcrush** | Wellenfaltung (Sinus-Folder, pre-Filter) bzw. Bit-/Sample-Rate-Reduktion (Lo-Fi). |
| <a id="phaser" name="phaser"></a>**Phaser/Flanger** | Allpass- bzw. Kurz-Delay-Modulationseffekt (RATE/DEPTH/FB/MIX). |
| <a id="compressor" name="compressor"></a>**Compressor** | Dynamik-Kompressor auf dem Summen-Bus (Threshold/Ratio/Attack/Release/Makeup). |
| <a id="chorus" name="chorus"></a>**Chorus / Delay / Reverb** | Modulations-, Verzögerungs- und Hall-Effekte — **pro Voice** instanziert, keine Bus-Sends. |
| <a id="scope" name="scope"></a>**Scope / Oscilloscope** | Wellenform-Anzeige (stereo, L/R nebeneinander), abgegriffen am **finalen Output**. |
| <a id="spectrum" name="spectrum"></a>**Spectrum** | Frequenzspektrum-Anzeige (1024er-FFT, zwei Kurven L/R), gleicher Abgriffpunkt. |

## Spatialisierung (STEREO-Ausgangsstufe)

| Begriff | Bedeutung |
|---------|-----------|
| <a id="pan" name="pan"></a>**PAN** | Per-Generator-Panorama-Regler (equal-power); auch Mod-Matrix-Ziel (Auto-Panning). |
| <a id="outputmode" name="outputmode"></a>**Output-Mode** | Die 5 Modi der STEREO-Stufe: Mono, **Pseudo-Stereo** (Default), Stereo-Pan, Binaural, Kunstkopf (HRTF). |
| <a id="pseudo-stereo" name="pseudo-stereo"></a>**Pseudo-Stereo** | Stereo-Verbreiterung aus dem Mono-Signal (Lauridsen-Komplementär-Comb, Width/Time) — mono-kompatibel per Konstruktion. |
| <a id="channelstrip" name="channelstrip"></a>**ChannelStrip** | Kanal-agnostischer Voice-Bus: die **komplette Effektkette einmal pro Ausgangskanal** — ein links gepannter Generator verhallt auch links (`DSP/ChannelStrip.h`). |
| <a id="binaural" name="binaural"></a>**Binaural (parametrisch)** | Kopfhörer-3D ohne Messdaten: ITD + ILD + Kopfschatten-Lowpass pro Generator (`DSP/BinauralPanner.h`). |
| <a id="kunstkopf" name="kunstkopf"></a>**Kunstkopf (HRTF)** | Echte Außer-Kopf-Ortung: Faltung jedes Generators mit gemessenen KEMAR-Impulsantworten für seinen PAN-Azimut (`DSP/HrtfPanner.h`, 128 Taps, statisch, RT-safe). |
| <a id="hrtf" name="hrtf"></a>**HRTF** | *Head-Related Transfer Function* — richtungsabhängige Filterwirkung von Kopf/Ohrmuschel, die das Gehör zur Ortung nutzt. |
| <a id="hrir" name="hrir"></a>**HRIR** | *Head-Related Impulse Response* — die HRTF als Impulsantwort im Zeitbereich (das, womit gefaltet wird). |
| <a id="kemar" name="kemar"></a>**KEMAR** | Kunstkopf-Messpuppe des MIT Media Lab; deren HRTF-Messungen (1994, attribution-only) sind als `DSP/KemarHrir.h` eingebettet — offline entzerrt und normiert (`tools/gen_kemar_hrir.py`). |
| <a id="itd" name="itd"></a>**ITD** | *Interaural Time Difference* — Laufzeitunterschied zwischen den Ohren (≤ ~0,9 ms), wichtigster Richtungs-Cue unterhalb ~1,5 kHz. |
| <a id="ild" name="ild"></a>**ILD** | *Interaural Level Difference* — Pegelunterschied zwischen den Ohren (Kopfabschattung), Richtungs-Cue oberhalb ~1,5 kHz. |
| <a id="room" name="room"></a>**ROOM** | Kunstkopf-Externalisierung: binaurale Early-Reflection-Stufe auf dem Bus (6 nicht-harmonische Taps 8–24 ms über laterale KEMAR-Ohren), 5-Rastungen-Makro, Mitte = ohr-kalibriertes Optimum (`DSP/BinauralRoom.h`). |

## BMAD / Planung & Prozess

| Begriff | Bedeutung |
|---------|-----------|
| <a id="bmad" name="bmad"></a>**BMAD** | „BMad Method" — das agentische Planungs-/Entwicklungs-Framework (Skills wie `create-story`, `dev-story`, `retrospective`). Global installiert, pro Projekt per Junction. |
| <a id="prd" name="prd"></a>**PRD** | *Product Requirements Document* — beschreibt, **was** das Produkt können soll (Features/Anforderungen), nicht **wie**. Liegt unter `_bmad-output/planning-artifacts/prds/`. |
| <a id="fr" name="fr"></a>**FR** | *Functional Requirement* — funktionale Anforderung (durchnummeriert, stabile IDs: FR1, FR2, …). |
| <a id="nfr" name="nfr"></a>**NFR** | *Non-Functional Requirement* — nicht-funktionale Anforderung (Wartbarkeit, Performance, Format-Kompatibilität, …). |
| <a id="ad" name="ad"></a>**AD** | *Architecture Decision* — Architektur-Entscheidung in der „Architecture Spine" (**AD-1 … AD-12**; Kurzfassung in [`ARCHITECTURE.md`](ARCHITECTURE.md#9-design-decision-record)). |
| <a id="spine" name="spine"></a>**Spine** | Die schlanke Architektur-Kernachse (`ARCHITECTURE-SPINE.md` in `_bmad-output/`): Invarianten, aus denen alles Weitere konsistent abgeleitet wird. |
| <a id="epic" name="epic"></a>**Epic** | Größeres, in sich geschlossenes Arbeitspaket, das in mehrere Stories zerfällt (E1, E2, E3, …). |
| <a id="story" name="story"></a>**Story** | Kleinste umsetzbare Arbeitseinheit mit vollem Kontext (z. B. `12-1-sampler-module.md`). Status: `draft` → `ready-for-dev` → `review` → `done`. |
| <a id="ac" name="ac"></a>**AC** | *Acceptance Criteria* — Abnahmekriterien einer Story. |
| <a id="memlog" name="memlog"></a>**memlog** | Append-only Entscheidungs-/Audit-Log eines PRD-Laufs (`.memlog.md`). Wird nur über das Skript geschrieben. |
| <a id="correct-course" name="correct-course"></a>**correct-course** | BMAD-Skill, um größere Änderungen mitten im Sprint sauber in PRD/Architektur/Epics zurückzuschreiben (statt undokumentiertem Drift). |
| <a id="retrospective" name="retrospective"></a>**retrospective** | BMAD-Skill für den Rückblick nach einem Epic (Lessons + Action Items). |
| <a id="deferred-work" name="deferred-work"></a>**deferred-work** | Datei mit zurückgestellten Review-Punkten / Tech-Debt (`implementation-artifacts/deferred-work.md`). |
