Spielt die Aufnahme des SAMPLERs als **Wolke kurzer Körner** — Texturen, Flächen und langsame Reisen
durch ein Sample statt einer abgespielten Note. Material ist das SET, das der SAMPLER hält; der SAMPLER
selbst darf aus bleiben. Beide dürfen zusammen klingen (trockene Note plus Wolke).

- **POS** — wo im Sample die Körner starten; **SPRAY** — wie weit sie darum streuen. SPRAY 0 liest eine
  Stelle (ein Stottern — bei hoher DENS wird die Dichte selbst zur Tonhöhe), 100 % verwischt die ganze Datei.
- **SIZE** — Kornlänge: 5 ms schnarrt metallisch, 300 ms verschmiert wie ein Echo. **DENS** — Körner pro
  Sekunde, von einzelnen Klicks bis zur dichten Fläche. Die Engine hält 32 Körner gleichzeitig; was darüber
  hinausgeht, startet einfach nicht.
- **PITCH** — zufällige Transposition pro Korn, ± so viele Halbtöne. **QUANT** rastet jedes Korn auf eine
  Skala relativ zur gespielten Note (Chromatisch, Dur, Moll, Pentatonik) — aus dem Schmier wird eine
  Melodie. Off = frei.
- **AMP / PAN** wie bei jedem Generator; eine Stereo-Datei behält ihre Breite um PAN.

In der MOD MATRIX sind POS, SIZE, PITCH, AMP und PAN Ziele — PITCH verschiebt dort das **Zentrum** der
Wolke: CHAOS X → POS wandert durch die Aufnahme, ein LFO → PITCH mit QUANT läuft eine Tonleiter ab.
Das ist ein Textur-Generator, kein Pitch-Shifter: für saubere Transposition SAMPLER + STRETCH nehmen.
