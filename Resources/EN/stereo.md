Output stage for the mono engine: choose how it reaches the two output channels.

- **MODE**
  - **Mono** — raw mono sum to both channels.
  - **Pseudo-Stereo** (default) — the Haas widener below (WIDTH/TIME).
  - **Stereo-Pan** — each generator's PAN placed into true L/R (amplitude).
  - **Binaural** — parametric headphone 3-D (ITD + head-shadow); a strong L/R widener, low CPU.
  - **Kunstkopf (HRTF)** — convolves each generator with a measured **MIT KEMAR** head impulse
    response for its PAN azimuth → genuine out-of-head placement. **Headphones only**; costs more CPU.
- **WIDTH** / **TIME** — only apply in Pseudo-Stereo (spread, and the short inter-channel delay).

PAN (per generator, and as a MOD-MATRIX target for auto-panning) drives Stereo-Pan, Binaural and Kunstkopf.
