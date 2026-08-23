Die Bewegungs-Schicht: Modulations-QUELLEN auf ein Ziel routen. Jede Zeile ist ein Routing â€” **SRC Â· MOD Â· PARAM Â· AMT** â€” bis zu 8 Zeilen. Mehrere Zeilen dÃ¼rfen sich auf demselben Ziel STAPELN; ihre BetrÃ¤ge addieren sich.

Das Ziel wird in zwei Schritten gewÃ¤hlt: **MOD** = welches Modul, dann **PARAM** = welcher Parameter darin. So liest sich eine Zeile als â€žOSC 2 Â· FREQ" statt als abstrakter globaler Name. Die MOD-Liste ist Ã¼berwiegend Aâ†’Z (die zuletzt ergÃ¤nzten Module stehen am Ende); die PARAM-Liste passt sich dem gewÃ¤hlten Modul an, und jeder PARAM heiÃŸt genau wie der Knopf, den er steuert.

Die LFOs haben kein eigenes eingebautes Ziel â€” sie sind *reine* Matrix-Quellen, hier ist also der EINE Ort, an dem ein LFO geroutet wird.

## Quellen â€” jede braucht ihr Modul an

- **LFO 1â€“4** â€” zyklische Bewegung (Vibrato / Wah). Jeder braucht sein eigenes **LFO**-Modul an (LFO 2â€“4 sind standardmÃ¤ÃŸig ausgeblendet â€” Ã¼ber MODULES einblenden).
- **Envelope** â€” die ADSR-Kontur. Braucht das **ENVELOPE**-Modul an und eine klingende Note. Ideal als Filter-HÃ¼llkurve: Envelope â†’ FILTER Â· CUTOFF.
- **Velocity** â€” wie fest du spielst (fest pro Note). GehÃ¶rt keinem Modul â€” einfach unterschiedlich stark spielen.
- **Chaos X / Chaos Y** â€” die Lorenz-Bahn des CHAOS-Moduls: nie wiederholend, nie zufÃ¤llig. X und Y reiten dieselbe Bahn â€” zwei Routings driften gemeinsam, aber nicht gleich. Braucht **CHAOS** an; lÃ¤uft frei, seine Ringe bewegen sich also auch ohne Note.

## Ziel â€” erst MOD, dann PARAM

Praktisch jeder kontinuierliche Knopf jedes Moduls ist ein Ziel. Highlights:

- **OSC 1 / OSC 2 / OSC 3** â€” moduliert einen EINZELNEN Oszillator: FREQ Â· AMP Â· DETUNE Â· FB Â· VOICES. Ein Routing hier bewegt nur diesen einen Oszillator.
- **Alle OSC** â€” dieselben Parameter auf ALLE Oszillatoren gleichzeitig (klassisches globales Vibrato / Tremolo).
- **WAVETABLE** (POS Â· FREQ Â· AMP Â· VOICES Â· DETUNE), **SUB** (AMP), **NOISE** (AMP), **KARPLUS** (AMP Â· DAMP Â· STR), **PITCH ENV** (AMOUNT), **FILTER** (CUTOFF Â· RESO), **FORMANT** (VOWEL Â· RESO Â· MIX), **WAVEFOLD** (DRIVE Â· SYM Â· MIX), **DISTORTION** (DRIVE Â· MIX), **BITCRUSH** (BITS Â· RATE Â· MIX), **CHORUS** (RATE Â· DEPTH Â· MIX), **PHASER** (RATE Â· DEPTH Â· FB Â· MIX), **DELAY** (TIME Â· FB Â· MIX), **REVERB** (ROOM Â· DAMP Â· MIX).
- **KARPLUS** bietet nur AMP Â· DAMP Â· STR â€” die TonhÃ¶he der Saite wird beim Anzupfen festgelegt, FREQ lÃ¤sst sich wÃ¤hrend des Ausklingens also nicht modulieren.

### Master-Bus-Ziele (nur LFO-Quellen)

**STEREO** (WIDTH Â· TIME), **MASTER** (VOL Â· TEMPO) und **COMPRESSOR** (THRESH Â· RATIO Â· ATK Â· REL Â· GAIN) laufen einmalig auf dem fertigen Stereo-Mix, nicht pro Stimme. Sie lassen sich nur von **LFO**-Quellen treiben â€” Velocity und Envelope sind pro Note und haben am Master-Bus keinen Einzelwert, hier bewirken sie also nichts. **MASTER Â· VOL** = Master-Tremolo, **STEREO Â· WIDTH** = wanderndes Stereobild, und **MASTER Â· TEMPO** lÃ¤sst die Tempo-Sync-BPM schwanken (driftende Sync-Delays/LFOs â€” â€žschwankende Beats").

Ein gewÃ¤hltes MODUL (auÃŸer â€žAlle OSC") wird automatisch aktiviert â€” und ein per-OSC-Routing schaltet genau diesen Oszillator an â€” damit das Routing hÃ¶rbar ist; die Zeile lÃ¶schen (MOD = Off) nimmt ein von JASS selbst gesetztes Aktivieren wieder zurÃ¼ck.

- **AMT** â€” StÃ¤rke, bipolar: rechts addiert, links invertiert, Mitte (0) bewirkt nichts.
- **QUANT** â€” rastet den Pitch-Anteil dieser Zeile auf eine Skala (Chrom / Major / Minor / Penta). Off = gleitende Oktaven (Vibrato, Drift). Mit S&H oder Chaos auf einem FREQ-Ziel werden aus Stufen Melodien. Wirkt nur, wenn die Zeile FREQ trifft; pro Zeile â€” ein glattes Vibrato kann neben einer gerasterten Melodie laufen.

Hinweis: gestufte Parameter (VOICES, BITCRUSH BITS/RATE) modulieren in ganzen Schritten â€” sie Ã¤ndern sich hÃ¶rbar stufenweise statt gleitend.

Tipp: *LFO 1 â†’ OSC 2 Â· FREQ* routen fÃ¼r ein Vibrato nur auf dem zweiten Oszillator, wÃ¤hrend OSC 1 darunter absolut stabil bleibt.
