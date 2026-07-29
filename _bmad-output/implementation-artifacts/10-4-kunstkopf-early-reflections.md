# Story 10.4: Kunstkopf externalization via binaural early reflections

Status: ready-for-dev

<!-- Follow-on to Story 10.3 (Kunstkopf HRTF mode). Raised 2026-07-29 after the listening session:
     once both binaural modes were level-matched, the user could not tell Binaural from Kunstkopf.
     Measurement (below) explains why, and why more HRTF accuracy cannot fix it. -->

## Story

As a headphone user,
I want the **Kunstkopf (HRTF)** mode to place sound genuinely **outside my head**, so that it is
audibly a different thing from the parametric **Binaural** mode rather than a subtler version of it.

## Context / Decision

### The problem, measured

After Story 10.3's tonal fixes, the two binaural modes are level-matched and both deliver ITD +
frequency-dependent ILD. The user reported "höchstens marginal … keinen Unterschied". Measured cues
(`tools/gen_kemar_hrir.py` data vs. the `BinauralPanner` formula, azimuth 90°):

| cue | Binaural (parametric) | Kunstkopf (measured) |
|---|---|---|
| ITD | 1095 µs | 796 µs |
| ILD @ 250 Hz | +17.0 dB | **+0.5 dB** |
| ILD @ 1 kHz | +21.3 dB | +6.1 dB |
| ILD @ 4 kHz | +31.6 dB | +8.1 dB |
| ILD @ 12 kHz | +40.1 dB | +29.4 dB |
| 1–16 kHz ripple, far ear | 20.4 dB | 28.5 dB |

Two findings:

1. **The parametric model is deliberately exaggerated** (`kMaxITDSeconds = 0.0009` — "a touch beyond
   physical, for a stronger effect"; a broadband 16 dB far-ear cut). So the *physically correct* mode
   cannot out-dramatize it. Kunstkopf is the milder of the two by design.
2. **Lateralization saturates.** Past roughly 15–20 dB ILD the image sits fully at one ear; 20 dB vs
   40 dB moves nothing further. Both modes therefore say "right", just for different reasons. What
   genuinely differs is *timbre* (Kunstkopf keeps the bass centred, because sound diffracts around a
   real head at those wavelengths; Binaural pans everything) — not direction or strength.

**Conclusion: more HRTF fidelity is the wrong lever.** Both modes are ILD/ITD renderers, and on that
axis they have converged. Externalization on headphones is driven mainly by **reflections**, not by
HRTF accuracy — dry binaural stays in-head almost regardless of kernel quality. That is the missing
dimension, and the parametric mode has no equivalent, so it also makes the two modes clearly distinct.

### Why reflections and not elevation (first)

Elevation was the other candidate (see Story 10.5). It is deferred because (a) height perception
through *someone else's* pinnae is notoriously unreliable — it usually reads as a timbre change, not
height; (b) it needs a re-download of the MIT set (the repo holds only `elev0`, recovered from the
generated header) and grows the header from 66 KB to ~350–460 KB; (c) it needs a new user-facing
parameter. Reflections need no new data and target the original goal of the epic directly ("echte
Ortung außerhalb des Kopfes").

### Placement decision: bus stage, not per voice

The direct sound must stay per-generator (each generator has its own PAN, hence its own azimuth).
**The reflections must not.** Rendering N reflections per generator per voice would multiply the
128-tap convolution by N across 7 generators × all voices — the existing single convolution is
already the mode's whole CPU cost. Reflections do not need per-source accuracy to externalize, so a
**single shared binaural reflection stage on the stereo bus** (in `PluginProcessor::processBlock`,
gated to `OutputMode::Kunstkopf`) gets nearly all the perceptual benefit at a fixed, tiny cost.

Consequence to accept and verify: the reflections are shared, so a hard-left and a hard-right
generator get the same reflection pattern rather than individually correct ones. Expected to be
inaudible for externalization purposes — **confirm by ear before polishing anything else.**

## Acceptance Criteria

1. **New DSP class** `Source/DSP/BinauralRoom.h` — a small early-reflection stage: a handful (target
   4–6) of taps with distinct delays, each attenuated and rendered to L/R through a **lateral** HRIR
   from the existing `KemarHrir` table (a reflection arriving from the side is what carries the
   out-of-head cue). Static-sized, **no allocation in `process`**, `prepare(sampleRate)` +
   `reset()` like the other DSP classes (RT-safety rules from Epic 11 apply).
2. **Delays chosen for externalization, not for audible echo:** roughly 8–25 ms, mutually
   non-harmonic so the pattern does not ring on a pitch. Anything below ~5 ms fuses into comb
   colouration and must be avoided — verify the tonal effect at pan centre is small (target: the
   centre-transparency won in Story 10.3 must not be given back; re-measure with the existing
   scratch harness approach).
3. **Bus placement**, active only in Kunstkopf mode: no effect whatsoever on the other four output
   modes (byte-identical output — regression oracle, cf. Story 10.3's `kMaxITDSeconds = 0` oracle).
4. **One user parameter**: `hrtfRoom` (0 = dry/current behaviour … 1 = full). **Default 0**, so
   existing presets and the factory state sound exactly as today — the global-default rule applies
   (never change a default to create a "default scenario"; ship a demo preset instead if wanted).
   Knob lives in the STEREO module and is greyed out outside Kunstkopf via the `Knob::activeWhen`
   mechanism added 2026-07-29.
5. **Append-only parameter** — FormatVersion stays 6, no migration.
6. **Level-neutral**: turning the knob up must not raise the perceived level (the five modes were
   just level-matched; do not break that). Normalise so the dry+reflection sum holds constant power.
7. **Mod-matrix target** for `hrtfRoom` is explicitly **out of scope** here (it is a global bus
   param, which needs the block-rate path, not the per-voice one — see Story 8.2's note on truly
   global params).
8. Verified by build + running app on headphones (no unit tests — [[feedback_ui_verification]]):
   does the image leave the head, and is Kunstkopf now unmistakably different from Binaural?

## Dev Notes

- **Measure before tuning.** This epic's history is that ear-guessed constants were wrong twice (a
  700 Hz crossover that let 75 % of a 220 Hz note bypass the HRTF; a linear-frequency energy
  normalisation that left the midrange 1.4 dB low). Both were found only by computing the actual
  transfer function. Keep that habit: verify flatness at pan centre, level vs. Stereo-Pan, and the
  reflection pattern's spectrum before deciding it sounds right.
- Reuse `KemarHrir::kHrir{Left,Right}` — no new data, no new attribution obligation.
- The reflection taps are a shared stereo stage, so they run **once per block on the bus**, not per
  voice. Keep the tap count fixed and `constexpr`.
- Beware the ODR trap: adding members to a header-defined struct that voices embed has caused
  0xC0000005 on startup via incremental builds. This class is a *bus* member, but if any voice-side
  struct changes size, **clean-rebuild** (`/t:Rebuild`).
- Files (expected): `Source/DSP/BinauralRoom.h` (new), `Source/PluginProcessor.{h,cpp}`,
  `Source/Modules/StereoSpecs.h`, `Source/Audio/Parameters.h`, `Source/UI/PluginEditor.cpp`
  (activeWhen wiring), `Resources/{EN,DE}/stereo.md`, `CHANGELOG.md`.
- Deliver on branch `develop`, no push/merge — author's call ([[feedback_git_workflow]]).

## Dev Agent Record

_Not started — story recorded 2026-07-29 at the user's request ("zuerst nur die Story aufnehmen,
noch nicht programmieren")._
