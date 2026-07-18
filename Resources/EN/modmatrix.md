The movement layer: route modulation SOURCES to TARGETS. Each row is one routing (**SRC → DEST**, with a bipolar **AMT**). Several rows may STACK on the same target — their amounts add up.

The LFO's own **TARGET** is really a built-in routing too, so it adds ON TOP of any matrix row pointing at the same destination (e.g. LFO TARGET = Frequency *plus* a row LFO 1 → Pitch = deeper vibrato).

## Sources — each needs its module on

- **LFO 1** — cyclic movement (vibrato / wah). Needs the **LFO** module on, else it sends nothing.
- **LFO 2** — a second, independent LFO. Needs the **LFO 2** module on (hidden by default — show it via MODULES).
- **Envelope** — the ADSR contour (opens on attack, fades on release). Needs the **ENVELOPE** module on and a sounding note. Great as a filter envelope: Envelope → Cutoff.
- **Velocity** — how hard you play (constant per note). Belongs to no module — just play. The auto-drone has a fixed velocity, so play the **keyboard** with varying strength to hear it.

## Targets — each needs its module on to be heard

- **Pitch** (how high/low the note) — always works (the oscillators).
- **Amplitude** (loudness) — always works.
- **Cutoff** (filter brightness) / **Resonance** (emphasis/peak at the cutoff) — need the **FILTER** module on.
- **WT Pos** (wavetable position — sweeps through the waveforms) — needs the **WAVETABLE** module on.
- **Vowel** (formant colour, "ah/oh/ee") — needs the **FORMANT** module on.
- **Wavefold** (wavefolding distortion — folds the wave back on itself) — needs the **WAVEFOLD** module on.

- **AMT** — amount, bipolar: right adds, left inverts, centre (0) does nothing.

Tip: stack *Envelope → Cutoff* and *LFO 1 → Cutoff* to hear both act on one filter (needs FILTER + LFO on).
