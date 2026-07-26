Ausgangsstufe der Mono-Engine: legt fest, wie das Signal auf die zwei Ausgangskanäle kommt.

- **MODE**
  - **Mono** — reine Mono-Summe auf beide Kanäle.
  - **Pseudo-Stereo** (Standard) — der Haas-Verbreiterer unten (WIDTH/TIME).
  - **Stereo-Pan** — die PAN jedes Generators wird echt nach L/R gelegt (Amplitude).
  - **Binaural** — parametrisches Kopfhörer-3-D (Laufzeit + Kopfschatten); starker L/R-Verbreiterer, wenig CPU.
  - **Kunstkopf (HRTF)** — faltet jeden Generator mit einer gemessenen **MIT-KEMAR**-Kopf-Impulsantwort
    für seinen PAN-Winkel → echte Ortung außerhalb des Kopfes. **Nur Kopfhörer**; kostet mehr CPU.
- **WIDTH** / **TIME** — wirken nur im Pseudo-Stereo (Breite bzw. kurze Kanal-Verzögerung).

PAN (pro Generator, auch als MOD-MATRIX-Ziel für Auto-Panning) steuert Stereo-Pan, Binaural und Kunstkopf.
