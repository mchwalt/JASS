# JASS — Feature Ideas

Last groomed: 2026-08-17. Legend: effort ★ (small) … ★★★★★ (large) · coolness ★ … ★★★★★

This file carries **open ideas only**. Everything shipped lives in
[`CHANGELOG.md`](../../CHANGELOG.md); the roadmap thread is in
[`JASS_Ideen_Merge.md`](../JASS_Ideen_Merge.md). Ideas that were tried or evaluated
and dropped are recorded at the bottom — with the reason — so they don't come back
by accident.

---

## Modulation

| Idea | What it does | Effort | Coolness |
|---|---|---|---|
| **Chaos mod source (Lorenz attractor)** | Three coupled differential equations, integrated per block — a mod source that never repeats but never degenerates into random noise. A new append-only `ModSource` next to LFO 1–4. Aim it at **timbre** (cutoff, wavefold drive, wavetable position), not pitch — story 14.1 taught that pitch dirt across chord notes reads as out-of-tune, not as analog. Pairs naturally with the LFO shapes below as one "LFO expansion" story. | ★★ | ★★★★ |
| **LFO shapes: Sample & Hold + One-Shot** | Two classic shapes the four LFOs still lack (current waves: Sine/Triangle/Square/Saw). S&H for stepped random motion, One-Shot for envelope-like single sweeps. | ★★ | ★★★ |
| **Feedback-FM / Self-FM** | An oscillator modulates its own frequency (DX-style — brighter, saw-like timbres). Needs its **own feedback-amount knob**; CROSS MOD deliberately couples two *different* OSCs, and self-feedback needs its own stability/loudness handling. | ★★★ | ★★★★ |

## Sound generation

| Idea | What it does | Effort | Coolness |
|---|---|---|---|
| **Granular synthesis** | Split a sample into grains, scatter/pitch them → clouds and textures. The Epic-12 sampler already holds the raw material (zones, samples in RAM). Two design notes worth keeping (Gemini, 2026-08): grain length as a matrix target (5 ms grains read as metallic crush, 300 ms as a psychedelic echo), and **per-grain pitch quantized to a scale** relative to the played note — the quantization is what turns randomness into music instead of dirt. | ★★★★ | ★★★★★ |
| **Modal synthesis** | Resonator banks for bells and mallets — the Karplus-Strong extension. | ★★★ | ★★★★ |
| **Sympathetic string resonance** | A bank of tuned waveguide resonators on the master bus (where PERC already sits), keyed by the held notes, fed a little of the piano's output — the strings of a real piano ringing along. Honest caveat: the effect shines with a sustain pedal, and there is none on this desk (CC64 is dropped for good) — only currently held notes would resonate, audible with chords but far less spectacular. | ★★★ | ★★ |
| **Wavetable morphing A→B→C** | Morph across up to three banks instead of one bank's position axis. Roadmap candidate. | ★★★ | ★★★★ |
| **SFZ `#include` / `#define`** | Parser support for the two preprocessor opcodes larger SFZ libraries use. | ★★ | ★★ |

## Effects

| Idea | What it does | Effort | Coolness |
|---|---|---|---|
| **Shimmer reverb** | Pitch-shifted feedback path in the reverb → octave-up halo. Roadmap candidate. | ★★★ | ★★★★ |
| **Convolution reverb** | Load real impulse responses (cathedral, plate). The Kunstkopf mode already does HRIR convolution, so part of the machinery exists. | ★★★★ | ★★★★ |
| **EQ (3-band)** | Bass/Mid/Treble on the master bus. | ★★ | ★★★ |

## Workflow & UX

| Idea | What it does | Effort | Coolness |
|---|---|---|---|
| **WAV export / recording** | Record what you play. | ★★ | ★★★ |
| **MIDI learn** | Bind knobs to a hardware controller. | ★★★ | ★★★★ |
| **Rack drag & drop** | Move modules between zones, reorder within a zone. Show/hide shipped long ago (MODULES panel), and the redesign delivered the enablers (stable module ids, explicit zone per `ModuleSpec`) — what's left is the layout-as-data model and the drag UI. | ★★★★ | ★★★ |
| **Macro knobs + preset morph** | One knob drives many parameters; blend A/B. **Deferred by the maintainer (2026-08)** — don't re-pitch unprompted. | ★★★ | ★★★★★ |
| **Evolution module** | Slowly mutating patches. Only worth pursuing if it targets **timbre**, not pitch (same 14.1 lesson as above). | ★★★★ | ★★★ |

---

## Declined — tried or evaluated, and dropped

- **Per-voice humanize/drift (story 14.1).** Built, heard, rejected: the audible window is
  narrow (±8 ct inaudible, ±25 ct dirty), and detune between voices only sounds "analog"
  on the *same* note — which is what UNISON DETUNE is for. Across chord notes it reads as
  dirt. Code parked on `feat/voice-humanize-drift` (no PR). If ever revisited: per-voice
  timbre/level variation, never pitch.
- **Sustain pedal (CC64).** No pedal on this desk, and JUCE's standalone holds note-offs
  itself. Dropped for good — do not bring it back.
- **"Wave-scraping" — sampler output as audio-rate phase modulator of the wavetable**
  (Gemini, 2026-08). Architecturally local (both generators live in the voice, an FM path
  exists), but uncontrollable in practice: every sample breaks differently, nothing about
  it is preset-able.
- **Audio-rate formant FM** (Gemini, 2026-08). The formant filter itself shipped 2026-07;
  modulating the formant frequencies at audio rate would need a direct engine path (the
  matrix runs at block rate) — engine surgery for a niche growl sound.
- **"Vektor-Mischkreuz" as a stereo stepping stone** (Gemini, 2026-08). Recommended
  building a vector-mixing crossfade that JASS never had planned, to solve a stereo
  problem Epic 10 already solved differently (per-generator PAN, five output modes,
  Kunstkopf HRTF).
