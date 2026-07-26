The movement layer: route modulation SOURCES to a destination. Each row is one routing — **SRC · MOD · PARAM · AMT** — up to 8 rows. Several rows may STACK on the same destination; their amounts add up.

The destination is picked in two steps: **MOD** = which module, then **PARAM** = which parameter inside it. So a row reads like "OSC 2 · FREQ" instead of an abstract global name. The MOD list is mostly A→Z (the most recently added modules sit at the end); the PARAM list follows the module you pick, and each PARAM is named exactly like the knob it drives.

The LFOs have no built-in target of their own — they are *purely* matrix sources, so this is the ONE place any LFO is routed.

## Sources — each needs its module on

- **LFO 1–4** — cyclic movement (vibrato / wah). Each needs its own **LFO** module on (LFO 2–4 are hidden by default — show them via MODULES).
- **Envelope** — the ADSR contour. Needs the **ENVELOPE** module on and a sounding note. Great as a filter envelope: Envelope → FILTER · CUTOFF.
- **Velocity** — how hard you play (constant per note). Belongs to no module — just play with varying strength.

## Destination — MOD then PARAM

Essentially every continuous knob on every module is a target. Highlights:

- **OSC 1 / OSC 2 / OSC 3** — modulate a SINGLE oscillator: FREQ · AMP · DETUNE · FB · VOICES. A routing here moves only that one oscillator.
- **Alle OSC** — the same params applied to ALL oscillators at once (classic global vibrato / tremolo).
- **WAVETABLE** (POS · FREQ · AMP · VOICES · DETUNE), **SUB** (LEVEL), **NOISE** (AMP), **KARPLUS** (AMP · DAMP · STR), **PITCH ENV** (AMOUNT), **FILTER** (CUTOFF · RESO), **FORMANT** (VOWEL · RESO · MIX), **WAVEFOLD** (DRIVE · SYM · MIX), **DISTORTION** (DRIVE · MIX), **BITCRUSH** (BITS · RATE · MIX), **CHORUS** (RATE · DEPTH · MIX), **PHASER** (RATE · DEPTH · FB · MIX), **DELAY** (TIME · FB · MIX), **REVERB** (ROOM · DAMP · MIX).
- **KARPLUS** offers AMP · DAMP · STR only — the string's pitch is baked in the moment it is plucked, so FREQ can't be modulated while it rings.

### Master-bus targets (LFO sources only)

**STEREO** (WIDTH · TIME), **MASTER** (VOL · TEMPO) and **COMPRESSOR** (THRESH · RATIO · ATK · REL · GAIN) run once on the finished stereo mix, not per voice. They can only be driven by **LFO** sources — Velocity and Envelope are per-note and have no single value at the master bus, so they do nothing here. **MASTER · VOL** = master tremolo, **STEREO · WIDTH** = a moving stereo image, and **MASTER · TEMPO** wobbles the Tempo-Sync BPM (drifting synced delays/LFOs — "swinging beats").

Choosing a MODULE (other than "Alle OSC") auto-enables it — and a per-OSC routing switches that oscillator on — so the routing is actually heard; clearing the row (MOD = Off) undoes an enable JASS made itself.

- **AMT** — amount, bipolar: right adds, left inverts, centre (0) does nothing.

Note: stepped parameters (VOICES, BITCRUSH BITS/RATE) modulate in whole steps, so they change audibly in stages rather than gliding.

Tip: route *LFO 1 → OSC 2 · FREQ* for a vibrato on just the second oscillator, while OSC 1 stays rock-steady underneath.
