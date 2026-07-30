Spielt eigene **Aufnahmen** (WAV/AIFF, bis 60 s, max. 32 geladen) als Klangquelle — durch die ganze
JASS-Kette: Filter, Wavefolder, Mod-Matrix, Arpeggiator, PAN und die binauralen Ausgabemodi.

- **LOAD** kopiert die Datei in deinen Samples-Ordner (`%AppData%\JASS\Samples`) und lädt sie;
  **SET** wählt unter den geladenen Samples. Presets merken sich das Sample **namentlich** aus
  diesem Ordner.
- **ROOT** — die Taste, bei der die Aufnahme in Originalgeschwindigkeit läuft; jede andere Taste
  transponiert sie bandmaschinen-artig (Formanten wandern mit: nutzbar sind grob ±1 Oktave — das
  ist die Natur von Einzel-Samples, kein Defekt).
- **START / END** beschneiden den gespielten Bereich; **MODE**: One-Shot, Loop (klickfreier
  Crossfade-Übergang), Reverse, Rev-Loop. **SPEED** (0,25×–4×) multipliziert die
  Abspielgeschwindigkeit zusätzlich zur Taste, bandmaschinen-artig — die Tonhöhe wandert mit.
- **Stereo-Dateien bleiben stereo**: links/rechts werden als zwei platzierte Quellen um PAN herum
  gerendert; in den Modi Mono und Pseudo-Stereo kollabieren sie zum Mono-Downmix.

Anders als WAVETABLE (tonhöhen-gelockter Einzelzyklus) spielt der SAMPLER die Aufnahme durch die
Zeit — ihre eigene Hüllkurve und ihr Charakter sind der Klang.
