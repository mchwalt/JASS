Plays the SAMPLER's recording as a **cloud of short grains** — textures, pads and slow journeys through a sample instead of a replayed note. The material is whatever SET the SAMPLER holds (the header shows its name); the SAMPLER itself may stay off, and both may sound together.

- **QUANT** — snaps every grain's pitch to a scale relative to the played note (Chromatic, Major, Minor, Pentatonic). Off = free.
- **KEY** — pitch-synchronous mode: one grain per period of the played note, so the repetition rate *is* the pitch and the sample's own timbre stays where it is (formant synthesis on any recording). DENS is ignored; SIZE acts up to two periods of the note; keep SPRAY low for a clean tone.
- **POS** — where in the sample the grains start.
- **SPRAY** — how far they scatter around POS. 0 reads one spot (a stutter — at high DENS the density itself becomes the pitch); 100 % smears the whole file.
- **SIZE** — grain length: 5 ms buzzes metallic, 300 ms smears like an echo.
- **DENS** — grains per second, from single clicks to a dense pad. 32 grains sound at once; beyond that the extra ones are simply not started.
- **PITCH** — random transposition per grain, ± this many semitones.
- **AMP / PAN** — as on every generator; a stereo file keeps its width around PAN.

In the MOD MATRIX, POS, SIZE, PITCH, AMP and PAN are targets — PITCH there shifts the **centre** of the cloud: CHAOS X → POS wanders through the recording, an LFO → PITCH with QUANT on walks a scale. A texture generator, not a pitch-shifter — for clean transposition use SAMPLER + STRETCH.
