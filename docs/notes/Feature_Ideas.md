# JASS — Feature Ideas

Last groomed: 2026-08-18. Legend: effort ★ (small) … ★★★★★ (large) · coolness ★ … ★★★★★

This file carries **open ideas only**. Everything shipped lives in
[`CHANGELOG.md`](../../CHANGELOG.md); the roadmap thread is in
[`JASS_Ideen_Merge.md`](../JASS_Ideen_Merge.md). Ideas that were tried or evaluated
and dropped are recorded at the bottom — with the reason — so they don't come back
by accident.

---

## Sound generation

| Idea | What it does | Effort | Coolness |
|---|---|---|---|
| **Granular synthesis** | Split a sample into grains, scatter/pitch them → clouds and textures. The Epic-12 sampler already holds the raw material (zones, samples in RAM). Design notes worth keeping: model it as **three separate dimensions** — grain *position* (where in the sample), *length* (5 ms reads as metallic crush, 300 ms as a psychedelic echo), *pitch* (how far transposed) — each a matrix target, with the matrix as the control layer on top (ChatGPT, 2026-08). **Per-grain pitch quantized to a scale** relative to the played note turns randomness into music instead of dirt (Gemini, 2026-08) — the QUANT mask shipped 2026-08 is the very mechanism. First patch to try: **Chaos X → grain position** — not a random cloud but a deterministic-chaotic journey through the sample. | ★★★★ | ★★★★★ |
| **Modal synthesis** | Resonator banks for bells and mallets — the Karplus-Strong extension. | ★★★ | ★★★★ |
| **Sympathetic string resonance** | A bank of tuned waveguide resonators on the master bus (where PERC already sits), keyed by the held notes, fed a little of the piano's output — the strings of a real piano ringing along. Honest caveat: the effect shines with a sustain pedal, and there is none on this desk (CC64 is dropped for good) — only currently held notes would resonate, audible with chords but far less spectacular. | ★★★ | ★★ |
| **Wavetable morphing A→B→C** | Morph across up to three banks instead of one bank's position axis. Roadmap candidate. | ★★★ | ★★★★ |
| **SFZ `#include` / `#define`** | Parser support for the two preprocessor opcodes larger SFZ libraries use. | ★★ | ★★ |

## Engine

| Idea | What it does | Effort | Coolness |
|---|---|---|---|
| **Gain-staging concept per module** | One deliberate **input → processing → output gain** rule for every module, instead of each new sound stage solving its own level problem ad hoc (ChatGPT, 2026-08). Self-FM, the wavefolder, cross-mod and a future granular engine all move the level massively; today the only guards are hard-coded scalars and the final clamp. Not glamorous, but it decides whether the engine stays controllable as it grows. | ★★★ | ★★ |
| **Filter keytracking** | The cutoff follows the played pitch (a TRACK knob, 0–100 %), so the timbre stays the same across the keyboard — without it, low notes read dull and high notes get choked by a fixed cutoff. Standard on the classics (Minimoog & Co.). Surfaced by the Los Niños work, 2026-08: a resonant lowpass bassline only speaks evenly over two octaves with tracking. | ★★ | ★★★ |

## Sequencer

| Idea | What it does | Effort | Coolness |
|---|---|---|---|
| **Per-step accent row (story 15.2)** | A second row in the STEP SEQ: each step plays plain or accented, and the accent drives level, note length and/or a filter bump — the Korg SQ-10 / TB-303 mechanism. The Los Niños MIDI transcription (2026-08) proved the original plays **all 24 steps in two accent classes** (long/loud vs. short/quiet); our rests are the approximation. The missing feature for this whole preset class — it is also what parked the Kraftwerk "Roboter" attempt. | ★★★ | ★★★★★ |
| **STEP SEQ ⇄ MIDI import/export** | Standard MIDI files are the lingua franca for patterns: load a figure straight from a `.mid` (velocity maps naturally onto the accent row), export a figure to the DAW or MuseScore. Pairs with a preset-format rework that stores each step as its own object — MIDI note number plus the spelled name (`"C#2"`) for readability. Import/export conventions are worked out in [`Sequencer_Market_Analysis.md`](Sequencer_Market_Analysis.md). | ★★★ | ★★★★ |
| **Euclid fill for PERC** | Three parameters — steps, pulses, rotate — spread hits Euclidean-evenly over a track's grid (E(3,8) = tresillo, E(4,16) = four-on-the-floor) as a starting point the player then edits per step; the Torso T-1's entry model. The maintainer places this on **PERC**, not the melodic STEP SEQ (2026-08-23): density-first entry shines for hi-hat and percussion lanes, while a bass figure lives on *which note where*. | ★★ | ★★★ |

## Effects

| Idea | What it does | Effort | Coolness |
|---|---|---|---|
| **Shimmer reverb** | Pitch-shifted feedback path in the reverb → octave-up halo. Roadmap candidate. | ★★★ | ★★★★ |
| **Convolution reverb** | Load real impulse responses (cathedral, plate). The Kunstkopf mode already does HRIR convolution, so part of the machinery exists. | ★★★★ | ★★★★ |
| **EQ (3-band)** | Bass/Mid/Treble on the master bus. | ★★ | ★★★ |

## Workflow & UX

| Idea | What it does | Effort | Coolness |
|---|---|---|---|
| **Sound-design pass with the existing blocks** | Before building the next engine feature, deliberately author 10–20 "impossible" sounds from what is already there — wavetable + wavefolder + cross-FM + self-FM + chaos + S&H + QUANT (ChatGPT, 2026-08). Where the attempts hit a wall tells reliably which feature JASS actually needs next; the keepers become demo presets. | ★★ | ★★★ |
| **WAV export / recording** | Record what you play. | ★★ | ★★★ |
| **MIDI learn** | Bind knobs to a hardware controller. | ★★★ | ★★★★ |
| **Rack drag & drop** | Move modules between zones, reorder within a zone. Show/hide shipped long ago (MODULES panel), and the redesign delivered the enablers (stable module ids, explicit zone per `ModuleSpec`) — what's left is the layout-as-data model and the drag UI. | ★★★★ | ★★★ |
| **Macro knobs + preset morph** | One knob drives many parameters; blend A/B. **Deferred by the maintainer (2026-08)** — don't re-pitch unprompted. | ★★★ | ★★★★★ |
| **Evolution module** | Slowly mutating patches. Only worth pursuing if it targets **timbre**, not pitch (the story-14.1 lesson). | ★★★★ | ★★★ |

---

## Declined — tried or evaluated, and dropped

- **Per-voice humanize/drift (story 14.1).** Built, heard, rejected: the audible window is
  narrow (±8 ct inaudible, ±25 ct dirty), and detune between voices only sounds "analog"
  on the *same* note — which is what UNISON DETUNE is for. Across chord notes it reads as
  dirt. The code is preserved in the tag `parked/voice-humanize-drift`. If ever revisited:
  per-voice timbre/level variation, never pitch.
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

## Shipped since the last groom (removed above)

- **LFO shapes S&H + One-Shot**, **Chaos mod source (Lorenz)** and the **QUANT scale
  mask** — the "LFO expansion" story, merged 2026-08-18 (PR #48).
- **Feedback-FM / Self-FM** — turned out to be already shipped (per-OSC FB knob since
  July); the gap was WAVETABLE/SUB coverage and taming the chaos region, closed 2026-08-18
  (PR #49: FB on both generators, DX-style two-sample damping in every self-FM path).
