Spielt eigene **Aufnahmen** (WAV/AIFF) als Klangquelle — durch die ganze JASS-Kette: Filter,
Wavefolder, Mod-Matrix, Arpeggiator, PAN und die binauralen Ausgabemodi.

- **LOAD** kopiert eine Datei in deinen Samples-Ordner (`%AppData%\JASS\Samples`) und lädt sie;
  **SET** wählt unter den geladenen Sets. Presets merken sich das Set **namentlich** aus diesem
  Ordner.
- **Multisampling (FOLDER):** lädt einen ganzen Ordner als EIN Set. Dateien mit Namen wie
  `Piano_C3.wav` / `Pad_A#4.wav` (Notenname mit C4 = mittleres C, oder MIDI-Nummer) werden über
  die Tastatur verteilt — jede deckt den Bereich bis zur Mitte zum Nachbarn ab, so bleiben
  Instrumente über mehr als eine Oktave natürlich. LOAD akzeptiert auch eine **.sfz**-Datei
  (ihre Tastenzuordnung wird importiert; gelesen werden nur die grundlegenden Sample-/Key-Opcodes).
- **ROOT** — die Taste, bei der eine **einzelne** Aufnahme in Originalgeschwindigkeit läuft; jede
  andere Taste transponiert sie bandmaschinen-artig (Formanten wandern mit: nutzbar sind grob
  ±1 Oktave — die Natur von Einzel-Samples, kein Defekt). Bei Multisample-Sets bringt jede Zone
  ihren eigenen Root mit, ROOT ist dann inaktiv (gedimmt).
- **START / END** beschneiden den gespielten Bereich; **MODE**: One-Shot, Loop (klickfreier
  Crossfade-Übergang), Reverse, Rev-Loop. **SPEED** (0,25×–4×) multipliziert die
  Abspielgeschwindigkeit zusätzlich zur Taste, bandmaschinen-artig — die Tonhöhe wandert mit.
- **STRETCH** entkoppelt Tonhöhe und Zeit: die Taste bestimmt nur noch die Tonhöhe, SPEED nur
  das Tempo — ein Loop behält auf jeder Taste seinen Rhythmus, und alle Loop-Stimmen bleiben
  unabhängig von der Tonhöhe im Takt. Der Anschlag wird beim Tastendruck vorberechnet, das
  Spielgefühl bleibt direkt; Preis ist ein dezenter Phase-Vocoder-Charakter auf Transienten.
  Aus = klassisches Bandmaschinen-Verhalten.
- **Stereo-Dateien bleiben stereo**: links/rechts werden als zwei platzierte Quellen um PAN herum
  gerendert; in den Modi Mono und Pseudo-Stereo kollabieren sie zum Mono-Downmix.
- **Grenzen:** 60 s pro Datei, 5 Minuten Audio pro Set, 32 Sets.

Anders als WAVETABLE (tonhöhen-gelockter Einzelzyklus) spielt der SAMPLER die Aufnahme durch die
Zeit — ihre eigene Hüllkurve und ihr Charakter sind der Klang.
