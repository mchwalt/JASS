# Glossary — Begriffe & Abkürzungen (JASS)

Globales Nachschlagewerk für Abkürzungen und Fachbegriffe, die in diesem Projekt
(Code, Doku, BMAD-Artefakte, Commits) auftauchen. Neue Begriffe bitte hier ergänzen.

---

## BMAD / Planung & Prozess

| Begriff | Bedeutung |
|---------|-----------|
| **BMAD** | „BMad Method" — das agentische Planungs-/Entwicklungs-Framework (Skills wie `create-story`, `dev-story`, `retrospective`). Global installiert, pro Projekt per Junction. |
| **PRD** | *Product Requirements Document* — beschreibt, **was** das Produkt können soll (Features/Anforderungen), nicht **wie**. Liegt unter `_bmad-output/planning-artifacts/prds/`. |
| **FR** | *Functional Requirement* — funktionale Anforderung (durchnummeriert, stabile IDs: FR1, FR2, …). |
| **NFR** | *Non-Functional Requirement* — nicht-funktionale Anforderung (Wartbarkeit, Performance, Format-Kompatibilität, …). |
| **AD** | *Architecture Decision* — Architektur-Entscheidung in der „Architecture Spine" (AD-1 … AD-9). |
| **Spine** | Die schlanke Architektur-Kernachse (`ARCHITECTURE-SPINE.md`): Invarianten, aus denen alles Weitere konsistent abgeleitet wird. |
| **Epic** | Größeres, in sich geschlossenes Arbeitspaket, das in mehrere Stories zerfällt (E1, E2, E3, …). |
| **Story** | Kleinste umsetzbare Arbeitseinheit mit vollem Kontext (z. B. `2-4-universal-module-enablers.md`). Status: `draft` → `ready-for-dev` → `review` → `done`. |
| **AC** | *Acceptance Criteria* — Abnahmekriterien einer Story. |
| **memlog** | Append-only Entscheidungs-/Audit-Log eines PRD-Laufs (`.memlog.md`). Wird nur über das Skript geschrieben. |
| **correct-course** | BMAD-Skill, um größere Änderungen mitten im Sprint sauber in PRD/Architektur/Epics zurückzuschreiben (statt undokumentiertem Drift). |
| **retrospective** | BMAD-Skill für den Rückblick nach einem Epic (Lessons + Action Items). |
| **deferred-work** | Datei mit zurückgestellten Review-Punkten / Tech-Debt (`implementation-artifacts/deferred-work.md`). |

## JUCE / Audio / Plugin

| Begriff | Bedeutung |
|---------|-----------|
| **JUCE** | C++-Framework für Audio-Anwendungen und Plugins (als Git-Submodule eingebunden). |
| **APVTS** | `juce::AudioProcessorValueTreeState` — zentraler Parameter-Baum; verbindet Parameter mit UI-Controls (Attachments) und Persistenz. Einziger erlaubter Cross-Modul-Kanal (AD-9). |
| **Attachment** | JUCE-Bindeglied zwischen einem Control und einem APVTS-Parameter (`SliderAttachment`, `ComboBoxAttachment`, `ButtonAttachment`). Mappt bei Combos **per Index** (Quelle des Combo-Index-Bugs). |
| **DSP** | *Digital Signal Processing* — die eigentliche Audio-Signalverarbeitung. |
| **RT** | *Real-Time* — Audio-Thread-Regeln: keine Allokation/Locks auf dem Audio-Thread; UI↔Audio nur über `std::atomic` + APVTS. |
| **Standalone** | Die eigenständige App (`JASS.exe`, via `JASS_Standalone`). |
| **VST3** | Plugin-Format für DAWs (z. B. REAPER). Installationspfad: `C:\Program Files\Common Files\VST3\JASS.vst3`. |
| **DAW** | *Digital Audio Workstation* — Host für Plugins (hier: REAPER). |
| **processBlock** | JUCE-Callback, in dem pro Audio-Block die Samples erzeugt/verarbeitet werden. |

## JASS Rack-Framework (UI-Redesign)

| Begriff | Bedeutung |
|---------|-----------|
| **Rack** | Das einheitliche 19″-Rack-Layout — besitzt **alle** Modul-Platzierung auf dem 12-Spalten-Grid (`Rack.h/.cpp`). |
| **Zone** | Gruppierung im Rack mit eigenem Zonen-Header: **MASTER BUS**, **GENERATORS**, **MODULATION**, **PROCESSING**. |
| **ModuleFrame** | Generischer Modul-Rahmen (Header + Body); rendert ein Modul aus seinem Deskriptor. Ersetzt die alten `OscillatorPanel`/`EffectPanel`/Inline-Panels. |
| **ModuleDescriptor** | Deklarative Beschreibung eines Moduls (Größenklasse, Titel, Typ-Tag, Enable-Param, Controls). Trägt eine stabile `id`. |
| **BodyElement** | Ein Bestandteil des Modul-Bodys (Knob, Combo, Toggle, Action-Button, File-Action, Caption, Display). |
| **Caption** | Statisches Text-Element im Body (heißt so wegen Namenskollision mit `juce::Label`). |
| **Größenklassen** | Spalten-Spannen auf dem 12-Spalten-Grid: **XXS** 1×1, **XS** 2×1, **S** 3×1, **M** 4×1, **L** 4×2, **XL** 6×2. XXS nur für Ein-Control-Module. |
| **Mod-Ring** | Live-Modulations-Ring um einen Knob, der die aktuelle Modulation (z. B. per LFO) anzeigt. |
| **typeTag** | Identitäts-/Farb-Tag eines Moduls (Generator/Modulator/Processor) für die Farbcodierung (FR6). |
| **enableParam / enabledWhen** | Zwei Quellen für den Enable-Zustand eines Moduls: echter Parameter (interaktiv) bzw. abgeleitetes Prädikat (nur Dimmen). Bei beidem gilt UND. |
| **`.synthy`** | Das On-Disk-Preset-Format (JSON). Bewusst unverändert/append-only; interop mit der (eingefrorenen) C#-Synthy-App. „Off" bleibt On-Disk-Marker via `PresetIO::choiceOrOff`. |

## Synthesizer-Begriffe (Module & Effekte)

| Begriff | Bedeutung |
|---------|-----------|
| **OSC** | *Oscillator* — Grund-Klangerzeuger (OSC 1–3). |
| **Sub** | Sub-Oszillator (tiefe Ergänzung). |
| **Noise** | Rausch-Generator. |
| **Karplus / Pluck** | Karplus-Strong-Saitensynthese; **PLUCK** löst das Anzupfen aus (Button / Leertaste). |
| **Wavetable** | Klangerzeugung aus einer Wellenform-Tabelle; **LOAD WAV** lädt eine eigene Wellenform. |
| **ADSR** | Hüllkurve *Attack–Decay–Sustain–Release* — formt Lautstärke über die Zeit. |
| **LFO** | *Low-Frequency Oscillator* — langsamer Oszillator zur Modulation anderer Parameter. |
| **Arpeggiator (Arp)** | Zerlegt einen Akkord in eine rhythmische Notenfolge. |
| **Filter** | Frequenzfilter (Cutoff/Resonanz); Cutoff ist Mod-Ring-Ziel. |
| **Distortion** | Verzerrung (Soft Clip / Hard Clip / Foldback). |
| **Wavefold / Bitcrush** | Wellenfaltung bzw. Bit-/Sample-Rate-Reduktion (Lo-Fi). |
| **Chorus / Delay / Reverb** | Modulations-, Verzögerungs- und Hall-Effekte. |
| **Mix Mode** | Verrechnungsart der Oszillatoren (additiv / gekoppelt). |
| **Pseudo-Stereo** | Stereo-Verbreiterung aus einem Mono-Signal (Width/Time). |
| **Scope / Oscilloscope** | Wellenform-Anzeige (XL-Display), mit Zoom (1/2/5/10/25 ms) + ms-Skala. |
| **Spectrum** | Frequenzspektrum-Anzeige (XL-Display), mit dB- + Log-Freq-Skala. |
