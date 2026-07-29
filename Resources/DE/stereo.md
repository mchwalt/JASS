Ausgangsstufe der Mono-Engine: legt fest, wie das Signal auf die zwei Ausgangskanäle kommt.

- **MODE**
  - **Mono** — reine Mono-Summe auf beide Kanäle.
  - **Pseudo-Stereo** (Standard) — der Haas-Verbreiterer unten (WIDTH/TIME). Der einzige Modus, der aus
    einer zentrierten Quelle Breite *erzeugt*.
  - **Stereo-Pan** — die PAN jedes Generators wird echt nach L/R gelegt (Amplitude).
  - **Binaural** — parametrisches Kopfhörer-3-D (Laufzeit + Kopfschatten); bewusst **übertrieben**
    für einen starken Effekt: es wandert alles, auch der Bass. Wenig CPU.
  - **Kunstkopf (HRTF)** — faltet jeden Generator mit einer gemessenen **MIT-KEMAR**-Kopf-Impulsantwort
    für seinen PAN-Winkel. Physikalisch korrekt statt übertrieben: der **Bass bleibt mittig** (um einen
    echten Kopf beugt sich der Schall herum), nur die Höhen wandern zur Seite, dazu die echte
    Ohrmuschel-Struktur. **Nur Kopfhörer**; kostet mehr CPU.
- **WIDTH** / **TIME** — wirken nur im Pseudo-Stereo (Breite bzw. kurze Kanal-Verzögerung); in allen
  anderen Modi sind sie deshalb ausgegraut.
- **ROOM** — wirkt nur im Kunstkopf (sonst ausgegraut). Fügt binaurale **frühe Reflexionen** hinzu
  (8–24 ms, über seitliche KEMAR-Ohren gerendert). Trockenes Binaural bleibt *im* Kopf, egal wie gut
  die HRTF ist — erst Reflexionen schieben das Klangbild **aus dem Kopf heraus**. Der Regler hat
  **5 Raststufen** (bewusst: mehr Abstufungen kann das Ohr bei Raumanteil gar nicht unterscheiden);
  jede Stufe = stärkerer **und hellerer** Raum, 0 = ganz trocken. Pegelneutral: mehr Raum wird nicht
  lauter. Am besten hörbar auf Transienten — Plucks und kurze Arp-Noten, nicht auf Dauertönen.

**PAN entscheidet, ob man überhaupt etwas hört.** Stehen alle Generatoren in der Mitte, geben
Stereo-Pan, Binaural und Kunstkopf identisches Mono aus — es gibt dann keine Richtung zu rendern, das
Umschalten kann also nichts bewirken. Dreh mindestens einen Generator aus der Mitte (z. B. OSC 1 nach
links, OSC 2 nach rechts), um die drei Modi zu unterscheiden. In Mono und Pseudo-Stereo ist PAN
wirkungslos und daher ausgegraut. PAN steht auch als MOD-MATRIX-Ziel für Auto-Panning bereit.

Alle Modi sind pegelgleich, ein A/B-Vergleich ist also nicht durch Lautstärke verfälscht.
