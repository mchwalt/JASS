Spielt eigene **Aufnahmen** (WAV/AIFF/FLAC) als Klangquelle — durch die ganze JASS-Kette: Filter,
Wavefolder, Mod-Matrix, Arpeggiator, PAN und die binauralen Ausgabemodi.

- **LOAD** kopiert eine Datei in deinen Samples-Ordner (`%AppData%\JASS\Samples`) und lädt sie;
  **SET** wählt unter den geladenen Sets. Presets merken sich das Set **namentlich** aus diesem
  Ordner.
- **Multisampling (FOLDER):** lädt einen ganzen Ordner als EIN Set. Dateien mit Namen wie
  `Piano_C3.wav` / `Pad_A#4.wav` (Notenname mit C4 = mittleres C, oder MIDI-Nummer) werden über
  die Tastatur verteilt — jede deckt den Bereich bis zur Mitte zum Nachbarn ab, so bleiben
  Instrumente über mehr als eine Oktave natürlich. LOAD akzeptiert auch eine **.sfz**-Datei
  (ihre Tastenzuordnung wird importiert; gelesene Opcodes: `sample`, `key`, `lokey`/`hikey`,
  `pitch_keycenter`, `default_path`, `offset`, `ampeg_release` — alles andere, auch
  Velocity-Layer, wird ignoriert; eine defekte Zone in einem importierten Set korrigiert man
  durch Editieren der .sfz).
- **ROOT** — die Taste, bei der eine **einzelne** Aufnahme in Originalgeschwindigkeit läuft; jede
  andere Taste transponiert sie bandmaschinen-artig (Formanten wandern mit: nutzbar sind grob
  ±1 Oktave — die Natur von Einzel-Samples, kein Defekt). Bei Multisample-Sets bringt jede Zone
  ihren eigenen Root mit, ROOT ist dann inaktiv (gedimmt).
- **START / END** beschneiden den gespielten Bereich; **MODE**: One-Shot, Loop (klickfreier
  Crossfade-Übergang), Reverse, Rev-Loop. Beim Wählen eines **Multisample-Sets richtet sich das
  Modul automatisch als Instrument ein**: MODE → One-Shot, STRETCH → aus, REL wird von 0 auf
  ~2 s angehoben, und — nur wenn der SAMPLER der einzige aktive Generator ist — schaltet das
  ENVELOPE-Modul ab (nichts schneidet den eigenen Ausklang ab) und der Ausgabemodus auf
  Stereo-Pan (der einzige Modus, der eine Stereo-Aufnahme unangetastet wiedergibt;
  Kunstkopf/Binaural verräumlichen sie hörbar um). Presets bleiben davon unberührt, und jede
  Einstellung lässt sich sofort wieder umschalten. **SPEED** (0,25×–4×) multipliziert die
  Abspielgeschwindigkeit zusätzlich zur Taste, bandmaschinen-artig — die Tonhöhe wandert mit.
- **STRETCH** entkoppelt Tonhöhe und Zeit: die Taste bestimmt nur noch die Tonhöhe, SPEED nur
  das Tempo — ein Loop behält auf jeder Taste seinen Rhythmus, und alle Loop-Stimmen bleiben
  unabhängig von der Tonhöhe im Takt. Die Engine arbeitet intern ~60 ms voraus; dieser Vorlauf
  wird beim Tastendruck vorberechnet, das Spielgefühl bleibt direkt. Preis ist ein dezenter
  Phase-Vocoder-Charakter auf Transienten. Aus = klassisches Bandmaschinen-Verhalten.
- **REL** — der **eigene Ausklang** des Samplers: wie lange eine losgelassene Taste ausklingt
  (der Klang schwingt unter dem Fade weiter — das Dämpfen eines echten Instruments statt eines
  harten Schnitts). Eine importierte .sfz kann das pro Zone setzen (`ampeg_release` — die
  Piano-Sets tun es); REL greift dann nur für Zonen ohne Wert. 0 = aus (klassisches Verhalten).
  Einfachster Aufbau für gesampelte Instrumente: **ENVELOPE aus** — dann regelt der Sampler
  seinen Ausklang komplett selbst. Mit ENVELOPE an formt die ADSR die Stimme obendrauf
  (dann A 0 / D 0 / S max / R mindestens so lang wie der längste Ausklang). Das Haltepedal
  (CC64) hält Noten; der Ausklang startet beim Loslassen des Pedals.
- **Stereo-Dateien bleiben stereo**: links/rechts werden als zwei platzierte Quellen um PAN herum
  gerendert; in den Modi Mono und Pseudo-Stereo kollabieren sie zum Mono-Downmix.
- **Grenzen:** 60 s pro Datei, 15 Minuten Audio pro Set, 32 Sets (plus ein globales
  Speicher-Budget — grob drei voll ausgebaute Sets).

Anders als WAVETABLE (tonhöhen-gelockter Einzelzyklus) spielt der SAMPLER die Aufnahme durch die
Zeit — ihre eigene Hüllkurve und ihr Charakter sind der Klang.
