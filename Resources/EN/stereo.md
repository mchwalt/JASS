Output stage for the mono engine: choose how it reaches the two output channels.

- **MODE**
  - **Mono** — raw mono sum to both channels.
  - **Pseudo-Stereo** (default) — the Haas widener below (WIDTH/TIME). The only mode that *creates*
    width from a centred source.
  - **Stereo-Pan** — each generator's PAN placed into true L/R (amplitude).
  - **Binaural** — parametric headphone 3-D (ITD + head-shadow), deliberately **exaggerated** for a
    strong effect: everything moves, bass included. Low CPU.
  - **Kunstkopf (HRTF)** — convolves each generator with a measured **MIT KEMAR** head impulse
    response for its PAN azimuth. Physically faithful rather than exaggerated: the **bass stays
    centred** (sound diffracts around a real head), only the highs move to the side, plus the real
    pinna structure. **Headphones only**; costs more CPU.
- **WIDTH** / **TIME** — only apply in Pseudo-Stereo (spread, and the short inter-channel delay), so
  they are greyed out in every other mode.
- **ROOM** — only applies in Kunstkopf (greyed out elsewhere). Adds a few binaural **early
  reflections** (8–24 ms, rendered through lateral KEMAR ears). Dry binaural tends to stay *inside*
  the head no matter how good the HRTF is — reflections are what push the image **out of the head**.
  0 = dry (as before), up = more room. Level-neutral: turning it up does not get louder.

**PAN decides whether you hear anything at all.** With every generator centred, Stereo-Pan, Binaural
and Kunstkopf all render identical mono — there is no direction to place, so switching between them
cannot do anything. Turn at least one generator off centre (say OSC 1 left, OSC 2 right) to tell the
three apart. In Mono and Pseudo-Stereo PAN has no effect and is greyed out. PAN is also available as a
MOD-MATRIX target for auto-panning.

All modes are level-matched, so an A/B comparison is not skewed by loudness.
