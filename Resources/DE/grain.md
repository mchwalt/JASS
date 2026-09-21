Spielt die Aufnahme des SAMPLERs als **Wolke kurzer Körner** — Texturen, Flächen und langsame Reisen
durch ein Sample statt einer abgespielten Note. Material ist das SET, das der SAMPLER hält; der SAMPLER
selbst darf aus bleiben, beide dürfen zusammen klingen.

- **QUANT** — rastet die Tonhöhe jedes Korns auf eine Skala relativ zur gespielten Note (Chromatisch,
  Dur, Moll, Pentatonik). Off = frei.
- **KEY** — tonhöhensynchroner Modus: ein Korn pro Periode der gespielten Note, die Wiederholrate *ist*
  die Tonhöhe, die Klangfarbe des Samples bleibt stehen (Formantsynthese auf jeder Aufnahme). DENS wird
  ignoriert; SIZE wirkt bis zu zwei Perioden der Note (längere Körner ließen hohe Noten zwischen die
  Teiltöne des Samples fallen); SPRAY klein halten für einen sauberen Ton.
- **POS** — wo im Sample die Körner starten.
- **SPRAY** — wie weit sie um POS streuen. 0 liest eine Stelle (ein Stottern — bei hoher DENS wird die
  Dichte selbst zur Tonhöhe); 100 % verwischt die ganze Datei.
- **SIZE** — Kornlänge: 5 ms schnarrt metallisch, 300 ms verschmiert wie ein Echo.
- **DENS** — Körner pro Sekunde, von einzelnen Klicks bis zur dichten Fläche. 32 Körner klingen
  gleichzeitig; was darüber hinausgeht, startet einfach nicht.
- **PITCH** — zufällige Transposition pro Korn, ± so viele Halbtöne.
- **AMP / PAN** — wie bei jedem Generator; eine Stereo-Datei behält ihre Breite um PAN.

In der MOD MATRIX sind POS, SIZE, PITCH, AMP und PAN Ziele — PITCH verschiebt dort das **Zentrum** der
Wolke: CHAOS X → POS wandert durch die Aufnahme, ein LFO → PITCH mit QUANT läuft eine Tonleiter ab.
Ein Textur-Generator, kein Pitch-Shifter — für saubere Transposition SAMPLER + STRETCH nehmen.
