Die Bewegungs-Schicht: Modulations-QUELLEN auf ein Ziel routen. Jede Zeile ist ein Routing — **SRC · MOD · PARAM · AMT** — bis zu 8 Zeilen. Mehrere Zeilen dürfen sich auf demselben Ziel STAPELN; ihre Beträge addieren sich.

Das Ziel wird in zwei Schritten gewählt: **MOD** = welches Modul, dann **PARAM** = welcher Parameter darin. So liest sich eine Zeile als „OSC 2 · FREQ" statt als abstrakter globaler Name. Beide Listen sind A→Z sortiert; die PARAM-Liste passt sich dem gewählten Modul an, und jeder PARAM heißt genau wie der Knopf, den er steuert.

Die LFOs haben kein eigenes eingebautes Ziel — sie sind *reine* Matrix-Quellen, hier ist also der EINE Ort, an dem ein LFO geroutet wird.

## Quellen — jede braucht ihr Modul an

- **LFO 1–4** — zyklische Bewegung (Vibrato / Wah). Jeder braucht sein eigenes **LFO**-Modul an (LFO 2–4 sind standardmäßig ausgeblendet — über MODULES einblenden).
- **Envelope** — die ADSR-Kontur. Braucht das **ENVELOPE**-Modul an und eine klingende Note. Ideal als Filter-Hüllkurve: Envelope → FILTER · CUTOFF.
- **Velocity** — wie fest du spielst (fest pro Note). Gehört keinem Modul — einfach unterschiedlich stark spielen.

## Ziel — erst MOD, dann PARAM

Praktisch jeder kontinuierliche Knopf jedes Moduls ist ein Ziel. Highlights:

- **OSC 1 / OSC 2 / OSC 3** — moduliert einen EINZELNEN Oszillator: FREQ · AMP · DETUNE · FB · VOICES. Ein Routing hier bewegt nur diesen einen Oszillator.
- **Alle OSC** — dieselben Parameter auf ALLE Oszillatoren gleichzeitig (klassisches globales Vibrato / Tremolo).
- **WAVETABLE** (POS · FREQ · AMP · VOICES · DETUNE), **SUB** (LEVEL), **FILTER** (CUTOFF · RESO), **FORMANT** (VOWEL · RESO · MIX), **WAVEFOLD** (DRIVE · SYM · MIX), **DISTORTION** (DRIVE · MIX), **BITCRUSH** (BITS · RATE · MIX), **CHORUS** (RATE · DEPTH · MIX), **PHASER** (RATE · DEPTH · FB · MIX), **DELAY** (TIME · FB · MIX), **REVERB** (ROOM · DAMP · MIX).

Ein gewähltes MODUL (außer „Alle OSC") wird automatisch aktiviert — und ein per-OSC-Routing schaltet genau diesen Oszillator an — damit das Routing hörbar ist; die Zeile löschen (MOD = Off) nimmt ein von JASS selbst gesetztes Aktivieren wieder zurück.

- **AMT** — Stärke, bipolar: rechts addiert, links invertiert, Mitte (0) bewirkt nichts.

Hinweis: gestufte Parameter (VOICES, BITCRUSH BITS/RATE) modulieren in ganzen Schritten — sie ändern sich hörbar stufenweise statt gleitend.

Tipp: *LFO 1 → OSC 2 · FREQ* routen für ein Vibrato nur auf dem zweiten Oszillator, während OSC 1 darunter absolut stabil bleibt.
