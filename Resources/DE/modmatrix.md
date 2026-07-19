Die Bewegungs-Schicht: Modulations-QUELLEN auf ZIELE routen. Jede Zeile ist ein Routing (**SRC → DEST**, mit bipolarem **AMT**), bis zu 6 Zeilen. Mehrere Zeilen dürfen sich auf demselben Ziel STAPELN — ihre Beträge addieren sich.

Die LFOs haben kein eigenes eingebautes Ziel — sie sind *reine* Matrix-Quellen, hier ist also der EINE Ort, an dem ein LFO geroutet wird.

## Quellen — jede braucht ihr Modul an

- **LFO 1–4** — zyklische Bewegung (Vibrato / Wah). Jeder braucht sein eigenes **LFO**-Modul an, sonst liefert er nichts (LFO 2–4 sind standardmäßig ausgeblendet — über MODULES einblenden).
- **Envelope** — die ADSR-Kontur (öffnet beim Anschlag, klingt aus). Braucht das **ENVELOPE**-Modul an und eine klingende Note. Ideal als Filter-Hüllkurve: Envelope → Cutoff.
- **Velocity** — wie fest du spielst (fest pro Note). Gehört keinem Modul — einfach spielen. Der Auto-Drone hat feste Velocity, also die **Klaviatur** unterschiedlich stark spielen, um es zu hören.

## Ziele — jedes braucht sein Modul an, um hörbar zu sein

- **Pitch** (Tonhöhe) — geht immer (die Oszillatoren).
- **Amplitude** (Lautstärke) — geht immer.
- **Cutoff** (Filter-Grenzfrequenz = „Helligkeit") / **Resonance** (Resonanz = Betonung an der Grenzfrequenz) — brauchen das **FILTER**-Modul an.
- **WT Pos** (Wavetable-Position = Durchlauf durch die Wellenformen) — braucht das **WAVETABLE**-Modul an.
- **Vowel** (Vokal-Klangfarbe, „ah/oh/ih") — braucht das **FORMANT**-Modul an.
- **Wavefold** (Wellenfaltung = Verzerrung durch Falten der Welle) — braucht das **WAVEFOLD**-Modul an.

- **AMT** — Stärke, bipolar: rechts addiert, links invertiert, Mitte (0) bewirkt nichts.

Tipp: *Envelope → Cutoff* und *LFO 1 → Cutoff* stapeln, um beide auf einem Filter zu hören (FILTER + LFO müssen an sein).
