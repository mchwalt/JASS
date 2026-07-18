Die Bewegungs-Schicht: Modulations-QUELLEN auf ZIELE routen. Jede Zeile ist ein Routing (**SRC → DEST**, mit bipolarem **AMT**). Mehrere Zeilen dürfen sich auf demselben Ziel STAPELN — ihre Beträge addieren sich.

Das eigene **TARGET** des LFOs ist im Grunde auch ein eingebautes Routing und kommt daher OBEN DRAUF, wenn eine Matrix-Zeile aufs selbe Ziel zeigt (z. B. LFO TARGET = Frequency *plus* Zeile LFO 1 → Pitch = tieferes Vibrato).

## Quellen — jede braucht ihr Modul an

- **LFO 1** — zyklische Bewegung (Vibrato / Wah). Braucht das **LFO**-Modul an, sonst liefert er nichts.
- **LFO 2** — ein zweiter, unabhängiger LFO. Braucht das **LFO-2**-Modul an (standardmäßig ausgeblendet — über MODULES einblenden).
- **Envelope** — die ADSR-Kontur (öffnet beim Anschlag, klingt aus). Braucht das **ENVELOPE**-Modul an und eine klingende Note. Ideal als Filter-Hüllkurve: Envelope → Cutoff.
- **Velocity** — wie fest du spielst (fest pro Note). Gehört keinem Modul — einfach spielen. Der Auto-Drone hat feste Velocity, also die **Klaviatur** unterschiedlich stark spielen, um es zu hören.

## Ziele — jedes braucht sein Modul an, um hörbar zu sein

- **Pitch** — geht immer (die Oszillatoren).
- **Amplitude** — geht immer (Lautstärke).
- **Cutoff** / **Resonance** — brauchen das **FILTER**-Modul an.
- **WT Pos** — braucht das **WAVETABLE**-Modul an.
- **Vowel** — braucht das **FORMANT**-Modul an.
- **Wavefold** — braucht das **WAVEFOLD**-Modul an.

- **AMT** — Stärke, bipolar: rechts addiert, links invertiert, Mitte (0) bewirkt nichts.

Tipp: *Envelope → Cutoff* und *LFO 1 → Cutoff* stapeln, um beide auf einem Filter zu hören (FILTER + LFO müssen an sein).
