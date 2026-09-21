Plays the SAMPLER's recording as a **cloud of short grains** — textures, pads and slow journeys
through a sample instead of a replayed note. The material is whatever SET the SAMPLER holds; the
SAMPLER itself may stay off. Both can sound together (dry note plus cloud).

- **POS** — where in the sample the grains start; **SPRAY** — how far they scatter around it. SPRAY 0
  reads one spot (a stutter — at high DENS the density itself becomes the pitch), 100 % smears the whole
  file.
- **SIZE** — grain length: 5 ms buzzes metallic, 300 ms smears like an echo. **DENS** — grains per second,
  from single clicks to a dense pad. The engine holds 32 grains at once; beyond that the extra ones are
  simply not started.
- **PITCH** — random transposition per grain, ± this many semitones. **QUANT** snaps every grain to a scale
  relative to the played note (Chromatic, Major, Minor, Pentatonic) — the cloud becomes a melody instead of a
  smear. Off = free.
- **AMP / PAN** as on every generator; a stereo file keeps its width around PAN.

In the MOD MATRIX, POS, SIZE, PITCH, AMP and PAN are targets — PITCH there shifts the **centre** of the
cloud, so CHAOS X → POS wanders through the recording and an LFO → PITCH with QUANT on walks a scale.
This is a texture generator, not a pitch-shifter: for clean transposition use SAMPLER + STRETCH.
