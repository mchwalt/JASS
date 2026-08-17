# JASS — Feature Ideas

Last groomed: 2026-08-17. Legend: effort ★ (small) … ★★★★★ (large) · coolness ★ … ★★★★★

This file carries **open ideas only**. Everything shipped lives in
[`CHANGELOG.md`](../../CHANGELOG.md); the roadmap thread is in
[`JASS_Ideen_Merge.md`](../JASS_Ideen_Merge.md). Ideas that were tried or evaluated
and dropped are recorded at the bottom — with the reason — so they don't come back
by accident.

---

## Modulation

| Idea | What it does | Effort | Coolness |
|---|---|---|---|
| **Chaos mod source (Lorenz attractor)** | Three coupled differential equations, integrated per block — a mod source that never repeats but never degenerates into random noise. A new append-only `ModSource` next to LFO 1–4. Aim it at **timbre** (cutoff, wavefold drive, wavetable position), not pitch — story 14.1 taught that pitch dirt across chord notes reads as out-of-tune, not as analog. Pairs naturally with the LFO shapes below as one "LFO expansion" story. | ★★ | ★★★★ |
| **LFO shapes: Sample & Hold + One-Shot** | Two classic shapes the four LFOs still lack (current waves: Sine/Triangle/Square/Saw). S&H for stepped random motion, One-Shot for envelope-like single sweeps. | ★★ | ★★★ |
| **Feedback-FM / Self-FM** | An oscillator modulates its own frequency (DX-style — brighter, saw-like timbres). Needs its **own feedback-amount knob**; CROSS MOD deliberately couples two *different* OSCs, and self-feedback needs its own stability/loudness handling. | ★★★ | ★★★★ |

## Sound generation

| Idea | What it does | Effort | Coolness |
|---|---|---|---|
| **Granular synthesis** | Split a sample into grains, scatter/pitch them → clouds and textures. The Epic-12 sampler already holds the raw material (zones, samples in RAM). Two design notes worth keeping (Gemini, 2026-08): grain length as a matrix target (5 ms grains read as metallic crush, 300 ms as a psychedelic echo), and **per-grain pitch quantized to a scale** relative to the played note — the quantization is what turns randomness into music instead of dirt. | ★★★★ | ★★★★★ |
| **Modal synthesis** | Resonator banks for bells and mallets — the Karplus-Strong extension. | ★★★ | ★★★★ |
| **Sympathetic string resonance** | A bank of tuned waveguide resonators on the master bus (where PERC already sits), keyed by the held notes, fed a little of the piano's output — the strings of a real piano ringing along. Honest caveat: the effect shines with a sustain pedal, and there is none on this desk (CC64 is dropped for good) — only currently held notes would resonate, audible with chords but far less spectacular. | ★★★ | ★★ |
| **Wavetable morphing A→B→C** | Morph across up to three banks instead of one bank's position axis. Roadmap candidate. | ★★★ | ★★★★ |
| **SFZ `#include` / `#define`** | Parser support for the two preprocessor opcodes larger SFZ libraries use. | ★★ | ★★ |

## Effects

| Idea | What it does | Effort | Coolness |
|---|---|---|---|
| **Shimmer reverb** | Pitch-shifted feedback path in the reverb → octave-up halo. Roadmap candidate. | ★★★ | ★★★★ |
| **Convolution reverb** | Load real impulse responses (cathedral, plate). The Kunstkopf mode already does HRIR convolution, so part of the machinery exists. | ★★★★ | ★★★★ |
| **EQ (3-band)** | Bass/Mid/Treble on the master bus. | ★★ | ★★★ |

## Workflow & UX

| Idea | What it does | Effort | Coolness |
|---|---|---|---|
| **WAV export / recording** | Record what you play. | ★★ | ★★★ |
| **MIDI learn** | Bind knobs to a hardware controller. | ★★★ | ★★★★ |
| **Rack drag & drop** | Move modules between zones, reorder within a zone. Show/hide shipped long ago (MODULES panel), and the redesign delivered the enablers (stable module ids, explicit zone per `ModuleSpec`) — what's left is the layout-as-data model and the drag UI. | ★★★★ | ★★★ |
| **Macro knobs + preset morph** | One knob drives many parameters; blend A/B. **Deferred by the maintainer (2026-08)** — don't re-pitch unprompted. | ★★★ | ★★★★★ |
| **Evolution module** | Slowly mutating patches. Only worth pursuing if it targets **timbre**, not pitch (same 14.1 lesson as above). | ★★★★ | ★★★ |

---

## Declined — tried or evaluated, and dropped

- **Per-voice humanize/drift (story 14.1).** Built, heard, rejected: the audible window is
  narrow (±8 ct inaudible, ±25 ct dirty), and detune between voices only sounds "analog"
  on the *same* note — which is what UNISON DETUNE is for. Across chord notes it reads as
  dirt. Code parked on `feat/voice-humanize-drift` (no PR). If ever revisited: per-voice
  timbre/level variation, never pitch.
- **Sustain pedal (CC64).** No pedal on this desk, and JUCE's standalone holds note-offs
  itself. Dropped for good — do not bring it back.
- **"Wave-scraping" — sampler output as audio-rate phase modulator of the wavetable**
  (Gemini, 2026-08). Architecturally local (both generators live in the voice, an FM path
  exists), but uncontrollable in practice: every sample breaks differently, nothing about
  it is preset-able.
- **Audio-rate formant FM** (Gemini, 2026-08). The formant filter itself shipped 2026-07;
  modulating the formant frequencies at audio rate would need a direct engine path (the
  matrix runs at block rate) — engine surgery for a niche growl sound.
- **"Vektor-Mischkreuz" as a stereo stepping stone** (Gemini, 2026-08). Recommended
  building a vector-mixing crossfade that JASS never had planned, to solve a stereo
  problem Epic 10 already solved differently (per-generator PAN, five output modes,
  Kunstkopf HRTF).

## Meinung Gemini

Das ist eine unglaublich gut gepflegte, scharfkantig analysierte Ideen-Roadmap! 
Es ist faszinierend zu sehen, wie konsequent Claude Code und du aus echten Praxistests (wie Story 14.1) lernen: Tonhöhen-Fluktuationen über Akkorde hinweg klingen nach Schmutz, nicht nach Analog-Vibe. Das rigorose Aussortieren von unberechenbaren Ideen („Wave-Scraping“) oder nicht nutzbarer Hardware (CC64 Sustain-Pedal) schützt das JASS-Projekt vor Feature-Creep und hält die Codebase schlank.Wenn man sich diese geschärfte Liste anschaut, stechen zwei absolut geniale Kombinationen heraus, die mit überschaubarem Aufwand (★★) einen gewaltigen klanglichen Sprung bedeuten, sowie ein logischer Pfad für euer nächstes großes Ding:1. Das „LFO-Expansions“-Paket (Modulation) — Extrem hoher Nutzen, geringer AufwandClaude hat die Lorenz-Attraktor-Chaosquelle (★★) und die neuen LFO-Shapes (S&H + One-Shot) (★★) bereits als logische Einheit gruppiert.Warum das einschlagen wird: Weil der Lorenz-Attraktor laut eurer neuen Prämisse nur auf Timbre (Cutoff, Wavefolder, Wavetable-Position) gerichtet wird. Zusammen mit einem Sample & Hold-LFO, der den Wavefolder bei jedem Tastendruck auf einen neuen, festen Wert springen lässt, transformiert das JASS augenblicklich von einem starren Synthesizer in eine organisch „atmende“, halbmodulare West-Coast-Maschine.Der Clou: Da ihr die Modulations-Matrix schon habt, ist das Hinzufügen einer neuen ModSource architektonisch sauber und append-only lösbar.2. Feedback-FM / Self-FM (Modulation) — Der DX7-FaktorDass Claude hier eine klare Grenze zur bestehenden Cross-Modulation zieht (eigener Feedback-Knopf wegen Lautstärken-Stabilität), ist goldrichtig.Der klangliche Gewinn: Wenn OSC 1 sich selbst moduliert, bricht die Wellenform mathematisch in Richtung einer Sägezahn-Welle auf, klingt aber durch das digitale Feedback viel aggressiver und metallischer. Für schneidende Bässe und digitale Leads ist das ein absoluter No-Brainer mit mäßigem Aufwand (★★★).3. Der logische Meilenstein: Granular-Synthese mit Skalen-QuantisierungDie Granular-Synthese (★★★★) steht als Königsklasse auf dem Plan. Claudes Notiz bezüglich der skalen-quantisierten Tonhöhe pro Grain ist der absolute Schlüssel, damit das Feature nicht in "Noise" degeneriert:Wenn ein Korn aus einem Piano-Sample gepickt wird, berechnet die Engine einen Zufallswert für den Pitch.Statt diesen Rohwert zu nutzen, jagt die Engine ihn durch eine kleine mathematische Maske (z.B. pitch = quantizeToPentatonic(pitch)).Der Effekt: JASS erzeugt keine anstrengende Rauschwolke, sondern eine wunderschöne, glitzernde Kaskade aus harmonischen Tönen, die wie ein himmlischer Partikel-Schwarm um die gespielte Note herumschweben. Da der Sampler durch Epic 12 bereits das Multisampling im RAM hält, ist das Fundament perfekt vorbereitet.Mein strategischer Rat für das nächste Treffen mit Claude Code:Bevor ihr das riesige Granular-Monster anwerft, holt euch die Quick Wins, die den Synthesizer sofort extrem aufwerten:Schreibt die LFO-Expansion (S&H + One-Shot + Lorenz-Chaos-Quelle). Das gibt euch völlig neue Modulations-Möglichkeiten für die bestehenden Triangles, Wavefolder und Wavetables.Setzt das Feedback-FM um, um den Grundklang der Oszillatoren drastisch zu erweitern.Mit welchem dieser präzise geschärften Pfade möchtest du in die nächste Session starten?

## Meinung chatGPT

Ja — und ich würde die Reihenfolge gegenüber dem eingefügten Text **leicht verändern**.

Die Roadmap ist insgesamt sehr gut, aber ich sehe bei JASS gerade eine interessante Gelegenheit: Nicht einfach „mehr Features“, sondern zuerst die Dinge bauen, die **die bereits vorhandenen Klangquellen stärker miteinander verzahnen**.

### Meine Reihenfolge wäre

**1. Feedback-FM** ★★★★ Klanggewinn / ★★★ Aufwand
**2. S&H + One-Shot** ★★★ / ★★
**3. Lorenz-Chaos** ★★★★ / ★★
**4. Granular** ★★★★★ / ★★★★

Warum?

#### 1. Feedback-FM würde ich vorziehen

Ihr habt bereits OSCs, FM/Cross-Modulation und offenbar eine ziemlich ausgebaute Modulationsmatrix. Self-FM ist deshalb kein neues Subsystem, sondern erweitert das **bestehende Klanguniversum**.

Und klanglich ist es enorm ergiebig:

* subtile Selbstmodulation → Obertonanreicherung
* mittlere Werte → metallisch / gläsern / aggressiv
* hohe Werte → chaotisch, fast zerstörte Wellenformen
* zusammen mit Wavefolder → ziemlich brutale digitale Sounds
* mit Wavetables → besonders interessant, weil die Selbst-FM auf unterschiedliche Grundspektren völlig unterschiedlich reagiert

Ich würde sogar **nicht versuchen, Feedback-FM primär als „DX7-Funktion“ zu betrachten**. Für JASS könnte es eher ein **Spektral-Drive** sein.

Das wäre eine schöne Ergänzung zu eurem Wavefolder:

> Wavefolder verändert die Amplitude/Geometrie der Welle → Feedback-FM verändert das Spektrum über die Phase.

Das könnte ziemlich stark werden.

---

### 2. S&H + One-Shot

Das ist für mich der eigentliche **UX-Quick-Win**.

Vor allem S&H würde ich nicht unterschätzen.

Wenn die Matrix bereits existiert, kannst du damit sofort solche Dinge machen:

**S&H → Wavetable Position**

→ bei jedem Schritt eine neue Klangfarbe.

**S&H → Filter Cutoff**

→ zufällige rhythmische Bewegungen.

**S&H → Wavefolder Drive**

→ sehr schöne digitale „sprechende“ Texturen.

Und dann wird es mit **Clock Sync** richtig interessant. Falls S&H bereits eine Rate aus der LFO-Infrastruktur bekommt, hast du damit praktisch einen kleinen Step-Randomizer.

One-Shot ist dagegen weniger spektakulär, aber konzeptionell sehr wichtig:

> LFO + One-Shot = Modulation kann sowohl zyklisch als auch ereignisbasiert sein.

Das ist eine kleine Erweiterung mit erstaunlich großer Wirkung.

---

### 3. Lorenz-Chaos

Hier würde ich allerdings einen Punkt aus dem vorhandenen Text **noch schärfer formulieren**.

Ich würde Lorenz **nicht als „Random Source“ vermarkten**.

Das Interessante daran ist gerade:

> **deterministisches Chaos statt Zufall.**

Das bedeutet beispielsweise:

**Cutoff ← Lorenz X**

und du bekommst eine Bewegung, die zunächst fast organisch wirkt, sich aber langfristig völlig anders verhält als ein periodischer LFO.

Noch interessanter:

**Lorenz X → Wavetable Position**
**Lorenz Y → Wavefolder Drive**

Dann hängen zwei Parameter **korreliert** zusammen.

Das ist wesentlich spannender als zwei unabhängige Zufallsgeneratoren.

Und genau deshalb finde ich die Entscheidung in eurer Roadmap richtig, Lorenz **nicht auf Pitch** zu legen.

---

## Und dann Granular

Das würde ich tatsächlich als **großes nächstes Kapitel** behandeln.

Aber ich würde bei der ersten Version eine Sache anders machen als die Formulierung „zufälliger Pitch → Scale Quantizer“ nahelegt.

Ich würde zuerst drei getrennte Dimensionen definieren:

**Grain Position**
Wo im Sample wird gelesen?

**Grain Length**
Wie lange ist das Fenster?

**Grain Pitch**
Wie stark wird transponiert?

Und dann die Matrix als Kontrollschicht darüber.

Damit kann JASS beispielsweise:

> Piano → Grain Position langsam durchfahren → kurze Grains → Pitch ±12 → Quantisierung auf Pentatonik

und daraus entsteht bereits ein völlig neues Instrument.

Danach:

> **Lorenz → Grain Position**

Das wäre für mich sogar eine der ersten Kombinationen, die ich ausprobieren würde.

Dann bekommt man keine klassische „Granular-Cloud“, sondern eine **deterministisch chaotische Reise durch das Sample**.

---

### Eine Sache fehlt mir in der Liste

Und zwar etwas, das ich nach unserer letzten Diskussion über Kompression ziemlich weit oben ansiedeln würde:

**ein ordentliches internes Gain-/Dynamics-Konzept.**

Nicht unbedingt gleich ein großer Mastering-Kompressor.

Aber wenn JASS immer komplexer wird mit

* Feedback-FM
* Wavefolder
* Cross-Mod
* Granular
* Resonatoren
* Shimmer

dann wird **Gain Staging** zunehmend wichtig.

Gerade Feedback-FM und Wavefolder können den Pegel massiv verändern.

Ich würde deshalb langfristig überlegen, ob jedes Modul ein klares Konzept bekommt:

**Input → Processing → Output Gain**

statt dass jedes neue Klangmodul seine eigene Pegelproblematik irgendwie löst.

Das ist weniger sexy als Granular oder Chaos — aber es entscheidet darüber, ob JASS am Ende **kontrollierbar** bleibt.

---

### Meine persönliche JASS-Roadmap wäre daher

| Reihenfolge | Feature                    | Warum                                                 |
| ----------- | -------------------------- | ----------------------------------------------------- |
| **1**       | Feedback-FM                | enormer Klanggewinn, nutzt vorhandene Architektur     |
| **2**       | S&H                        | sofort viele neue Modulationspatches                  |
| **3**       | One-Shot                   | kleine Ergänzung, sehr nützlich                       |
| **4**       | Lorenz                     | macht die Modulationsarchitektur wirklich interessant |
| **5**       | Gain/Dynamics sauberziehen | wichtig, bevor die Engine noch komplexer wird         |
| **6**       | Granular                   | nächster großer Synthese-Baustein                     |
| **7**       | Modal                      | danach schöne Ergänzung für physikalische Sounds      |
| **8**       | Shimmer/Convolution        | Effekte später                                        |
| **9**       | Rack D&D                   | UX, wenn die Engine weiter wächst                     |

Und **Sympathetic String Resonance** würde ich tatsächlich vorerst ganz unten lassen. Nicht weil die Idee schlecht ist, sondern weil sie ohne Sustain-Pedal einen großen Teil ihres „Wow“-Effekts verliert.

Der wirklich spannende Punkt ist für mich aber etwas anderes:

**JASS hat inzwischen genug einzelne Klangbausteine, dass die nächste Entwicklungsstufe nicht mehr unbedingt neue Oszillatoren braucht.**

Die Kombination

**Wavetable + Wavefolder + Cross-FM + Self-FM + Chaos + S&H + Matrix**

könnte bereits ein ausgesprochen eigenständiges Klanginstrument ergeben.

Ich würde deshalb als nächstes **nicht Granular bauen**, sondern versuchen, mit diesen vorhandenen Bausteinen bewusst 10–20 „unmögliche“ Sounds zu erzeugen. Wenn dabei Grenzen auftauchen, sagen die Sounds ziemlich zuverlässig, **welches neue Feature JASS tatsächlich braucht**.
