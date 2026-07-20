# JASS – Ideen-Merge: „Best of Both Worlds"

Stand: 2026-07-14. Fusion aus **`Feature_Ideas.md`** (unser bisheriger Plan) und
**`JASS_Ideen_chatGPT.txt`** (externer Ideenbrief).
Legende: Aufwand ★ (wenig) … ★★★★★ (viel) · Coolness ★ … ★★★★★

---

## 0. Leitidee (worauf sich beide Quellen einig sind)

> **Der Unterschied zu Serum/Vital/Pigments liegt selten in der Anzahl der Oszillatoren,
> sondern im Zusammenspiel aus _Modulation_, _Drift_ und _Makrosteuerung_.**
> Ein Patch, der sich über Minuten subtil verändert, wirkt hochwertiger als ein statischer
> Klang mit vielen Effekten.

Der ChatGPT-Brief priorisiert bewusst so:

| Bereich | Einfluss auf den Klang |
|---|---:|
| Modulationsmatrix | ⭐⭐⭐⭐⭐ |
| Effekte | ⭐⭐⭐⭐⭐ |
| Filter | ⭐⭐⭐⭐⭐ |
| Hüllkurven · Wavetables · Voice-Engine · Drift/Humanize · LFOs | ⭐⭐⭐⭐ |

**Konsequenz für JASS:** Die Engine-Bausteine stehen weitgehend. Der Hebel liegt jetzt auf
der **„Leben & Bewegung"-Schicht** (Drift, Per-Voice-Zufall, Evolution) plus zwei Meta-Systemen
(**Mod-Matrix**, **Makros**). Das gibt JASS eine eigene klangliche Identität statt „noch ein WT-Synth".

---

## 1. Abgleich: Was ChatGPT vorschlägt vs. was JASS schon hat

Damit wir **nichts doppelt bauen**. ✅ = in JASS vorhanden, 🟡 = teilweise, ⬜ = offen.

| ChatGPT-Vorschlag | Status in JASS | Anmerkung |
|---|:--:|---|
| 2 Wavetable-OSC + FM-OSC + Sub | ✅ | OSC1/2 (WT + FM OSC1→OSC2 + RingMod + **Self-FM**), Sub-OSC |
| Noise mit mehreren Farben (White/Pink/Brown/Blue) | 🟡 | aktuell **White + Pink** → Brown+Blue ergänzen (billig) |
| Formant-/Vokal-Filter | ✅ | Modul FORMANT (2026-07-13) |
| Phaser / Flanger | ✅ | Modul PHASER (2026-07-13) |
| Chorus · Delay · Reverb · Sättigung/Distortion | ✅ | komplette FX-Kette vorhanden |
| Tempo-Sync (LFO & Delay an BPM) | ✅ | 2026-07-13 |
| Portamento/Glide (mono + poly) | ✅ | 2026-07-14 |
| Zwei frei zuweisbare Hüllkurven | 🟡 | ENVELOPE vorhanden; „frei zuweisbar" = Mod-Matrix (siehe §2) |
| **4 LFOs (auch One-Shot + Random-Modus)** | 🟡 | ein LFO mit erweiterten Zielen; mehr LFOs + S&H/One-Shot offen |
| **Modulationsmatrix (Quelle→Ziel, Drag&Drop)** | ⬜ | größter Hebel, siehe §2 |
| **Makro-System (4–8 Regler)** | ⬜ | siehe §2 |
| **Per-Voice-Zufall / Analog Drift / „Living Osc"** | ⬜ | Signatur-Feature, siehe §2 |
| **„Evolution"-Modul (10–60 s Lebenszyklus)** | ⬜ | Alleinstellungsmerkmal, siehe §2 |
| Wavetable-**Morphing** über mehrere WTs | 🟡 | WT-Position-Morph in _einer_ Bank; A→B→C-Morph offen |
| MPE (per-note Pitch/Timbre/Pressure) | ⬜ | siehe §3 |
| Microtonale Skalen (19/31-TET, arab./ind.) | ⬜ | siehe §3 |
| Granular-Synthese | ⬜ | schon im Backlog (★★★★/★★★★★) |
| Spektral/FFT-Sounds | ⬜ | anspruchsvoll, siehe §3 |
| Kompressor | ⬜ | fehlt in der FX-Kette |
| Convolution-Reverb / Shimmer | ⬜ | Shimmer = Pitch-Shift-Feedback im Reverb |
| Oversampling für FM/Distortion | ⬜ | Anti-Aliasing-Qualität |
| **WT-Bibliothek (AKWF/CC0 + Metadaten)** | ⬜ | siehe §4 |
| **WT-Generator (Regler → neue Welle)** | ⬜ | siehe §4 |
| Preset-Generator aus Regeln / Preset-Bank | 🟡 | RANDOM vorhanden; kuratierte Bank + „musikalischer" Generator offen |

**Fazit:** ChatGPT bestätigt unsere zuletzt gebaute Richtung (Formant, Phaser, Glide, Tempo-Sync
waren goldrichtig) und zeigt exakt die verbleibenden Lücken auf.

---

## 2. Die „Best-of-Both"-Kern-Roadmap (höchste Priorität)

Diese vier bilden zusammen JASS' eigene Identität — die „Bewegungs-Schicht", die beide Quellen
als das Entscheidende benennen. Reihenfolge = empfohlene Umsetzung, weil sie aufeinander aufbauen.

### 2.1 Modulationsmatrix ★★★ / ★★★★★  — *das Fundament*
Beliebige **Quelle → Ziel** mit Amount. Quellen: LFO(s), ENVELOPE, S&H/Random, Velocity,
Keytrack, **Per-Voice-Random** (§2.3), Makros (§2.2), später MPE-Timbre/Pressure.
Ziele: alle klangrelevanten Params (WT-Pos, FM, Cutoff, Res, Pan, Pitch, FX-Mix …).
- JASS hat das Muster schon halb: **LFO-Ziele + Mod-Ringe** existieren — die Matrix
  verallgemeinert das nur zu „n Quellen × m Ziele".
- Umsetzung: schlanke Matrix (feste Quellen-/Ziel-Enums, additive Summe pro Ziel), UI als Modul
  „MOD MATRIX" mit Slot-Liste (Quelle · Ziel · Amount). Kein Drag&Drop nötig für v1 — Comboboxen genügen.
- **Interop:** append-only Params (`modSlotNSource/Target/Amount`), `.synthy` back-compat.
- **Warum zuerst:** macht §2.2/§2.3/§2.4 zu „nur noch Quellen an die Matrix hängen".

### 2.2 Makro-System (4–8 Regler) + A/B-Morph ★★★ / ★★★★★
Ein Makro steuert **viele** Ziele gleichzeitig (über die Matrix). Plus **Macro-Morph**:
ein Regler blendet zwischen bis zu 4 gespeicherten Klangzuständen über (Vital-„Macro", Pigments-„Macro").
- Baut direkt auf der Matrix auf (Makro = Quelle).
- Preset-Morph (A/B überblenden) war schon im alten Plan — hier vereint.

### 2.3 Per-Voice-Zufall / Analog Drift / „Living Oscillator" ★★★ / ★★★★★  — *Signatur*
Jede Stimme bekommt **eigene kleine Zufallswerte** (Cutoff ±%, Startphase, FM-Index, Pitch-Drift),
plus einen **langsam driftenden** Anteil pro Voice („Living Osc": Waveshape/Phase/PWM wandern um 1–3 %).
- Macht Akkorde und Unison sofort lebendig und „analog" statt digital-steril.
- Zwei Regler genügen als Einstieg: **HUMANIZE** (Streuung fix pro Note) + **DRIFT** (langsame Bewegung).
- Technisch: pro `SynthVoice` ein RNG-Seed + langsamer interner LFO; als Matrix-Quelle „Voice-Random"
  verfügbar machen ⇒ Nutzer kann es auf beliebige Ziele legen.
- Deckt zugleich ChatGPTs „Broken Synth" ab (kontrolliertes Chaos in stärkerer Dosis).

### 2.4 „Evolution"-Modul ★★★★ / ★★★★★  — *Alleinstellungsmerkmal*
ChatGPTs Favorit und die Idee, „die man so kaum sieht": Nach dem Anschlag entwickeln sich
WT-Position, FM-Index, Filter und Stereobreite **über 10–60 s** organisch weiter; kleine
Zufallsabweichungen sorgen dafür, dass es sich **nie exakt wiederholt**. Der Nutzer stellt nur
**Intensität** und **Dauer** ein.
- Umsetzung als sehr langsame, per-Voice leicht randomisierte Multi-Ziel-Hüllkurve/LFO-Kombi —
  wieder eine **Matrix-Quelle** „Evolution" mit 2 Reglern (Time, Amount) + Ziel-Auswahl.
- Realisiert direkt die Preset-Ideen „Harmonic Bloom" und „Liquid Motion" aus dem Brief.

> **Roter Faden:** 2.1 baut die Autobahn, 2.2/2.3/2.4 sind Auffahrten. Jede neue Quelle
> (Makro, Voice-Random, Evolution) ist danach billig, weil die Verrohrung einmal steht.

---

## 3. Klang-Erweiterungen (mittlere Priorität)

| Feature | Was / Merge-Notiz | Aufwand | Cool |
|---|---|:--:|:--:|
| **Mehr LFOs + One-Shot/S&H** | 2.–4. LFO, Modi Random/Sample&Hold/One-Shot → Quellen für die Matrix | ★★★ | ★★★★ |
| **WT-Morphing A→B→C** | über mehrere Bänke/Wellen morphen (heute nur Position in _einer_ Bank) → „Morphing Leads" | ★★★★ | ★★★★★ |
| **Noise: Brown + Blue** | zwei Farben ergänzen (White/Pink vorhanden) — sehr billig, „Ice Wind"-Preset | ★ | ★★ |
| **Kompressor** | fehlt komplett in der FX-Kette; Glue/Punch | ★★ | ★★★ |
| **Shimmer-/Convolution-Reverb** | Shimmer = Pitch-Shift im Reverb-Feedback („Aurora Glass"); Convolution = IR laden | ★★★★ | ★★★★ |
| **Oversampling (FM/Distortion)** | Anti-Aliasing → sauberere aggressive Sounds | ★★★ | ★★★ |
| **Granular-Synthese** | Sample → Körner (Clouds/Texturen), Grain-Size/Position/Pitch modulierbar | ★★★★ | ★★★★★ |
| **Microtonale Skalen** | 19/31-TET, arab./ind. Skalen, Scala-`.scl`-Import → sofort außergewöhnlich | ★★★ | ★★★★ |
| **MPE** | per-note Pitch/Timbre/Pressure als Matrix-Quellen (ROLI/Osmose) | ★★★★ | ★★★★ |
| **Spektral/FFT** | Formanten verschieben, Spektrum morphen — anspruchsvoll, langfristig | ★★★★★ | ★★★★★ |

---

## 4. Wavetable-Ökosystem (eigener Strang)

ChatGPT betont: Wavetables sind „nur" 20–30 % des Klangs, aber eine **kuratierte, durchsuchbare
Bibliothek** hebt den Workflow. JASS hat schon 6 Built-in-Banks + WAV-Import — hier die Ausbaustufen:

1. **AKWF-Kern (CC0)** – kuratierte ~200–512 Wellen aus den Adventure-Kid-Waveforms, kategorisiert
   (Analog/Harmonic/Digital/Vocal/Metal/Motion/Aggressive/Experimental). Lizenz: Public Domain, sauber. ★★
2. **Eigenes Binärformat beim ersten Start** – WAV → `float`-Frames (2048 Samples), schnelles Laden,
   direkter Speicherzugriff fürs Morphing. ★★★
3. **Metadaten pro Welle** (Brightness/Harmonics/Metallic/Vocal/Analog …) automatisch berechnet
   (Spektralschwerpunkt, Obertonzahl, Zero-Crossing-Rate, Spectral Flatness) → **durchsuchbare Bibliothek**. ★★★
4. **WT-Generator** – Nutzer stellt Grundform + Helligkeit/Metallisch/Formant/Obertöne/Symmetrie →
   JASS erzeugt in Echtzeit eine neue Welle (additiv/FFT), speicher- und morphbar. **Echtes
   Alleinstellungsmerkmal.** ★★★★ / ★★★★★
5. **Preset-Generator „mit Verstand"** – statt reinem RANDOM: aus musikalischen Regeln + WT-Metadaten
   gezielt „warmer Analog-Bass" oder „heller metallischer Pad" würfeln. ★★★ / ★★★★

---

## 5. Konkrete Preset-Bank als Zielbild (Motivation für §2–§4)

Der Brief liefert 10 fertige Klangrezepte. Sie sind zugleich die **Akzeptanz-Tests** für die
Features oben — wenn JASS diese sauber kann, sitzt die Bewegungs-Schicht:

| Preset | Braucht v. a. |
|---|---|
| Aurora Glass, Liquid Motion, Frozen Choir | mehrere langsame LFOs, WT-Morph, Shimmer-Reverb, §2.4 Evolution |
| Analog Ghost, „Living Osc" | §2.3 Per-Voice-Drift/Humanize |
| Harmonic Bloom | §2.4 Evolution (Multi-Ziel über 8–12 s) |
| Crystal Bell, Digital Rain | FM (✅) + metallische WTs + Env Decay + Delay-Sync (✅) |
| Neon Bass, Cyber Lead | Unison (✅) + FM + Filter-Env + Sättigung (✅) + Makro (§2.2) |
| Ice Wind | Noise-Farben (§3) + langsame Filterbewegung + 100 % Reverb |

**Empfehlung:** Nach §2 eine kleine **kuratierte Werks-Preset-Bank** (20–40 Sounds, Kategorien
Pad/Lead/Bass/Pluck/Keys/FX/Cinematic/Ambient) anlegen — sie verkauft die neuen Features besser als
jede Feature-Liste.

---

## 6. Empfohlene Reihenfolge (verschmolzen)

1. **Mod-Matrix (2.1)** — Fundament, macht alles Weitere billig.
2. **Per-Voice-Drift/Humanize (2.3)** — sofort hörbarer „premium/analog"-Effekt, kleiner Aufwand.
3. **Makros + Morph (2.2)** — Workflow-Sprung, Live-Tauglichkeit.
4. **Evolution-Modul (2.4)** — das Alleinstellungsmerkmal.
5. **Kleine Werks-Preset-Bank** — validiert 1–4, macht sie sichtbar.
6. Dann nach Lust: mehr LFOs, WT-Morphing A→B→C, Kompressor/Shimmer, Noise-Farben, Granular,
   Microtonal, WT-Bibliothek/Generator, MPE, Spektral.

> Alles append-only planen (Interop mit `.synthy` + C#-Rückwärtskompatibilität, fehlende Felder =
> Default), View-/Meta-State getrennt vom Klang halten (wie beim Rack-Layout).
