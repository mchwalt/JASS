# Story 10.4: Kunstkopf externalization via binaural early reflections

Status: done

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

### Implementation (2026-07-29)

**Measured first (AC2, per the epic's "measure before tuning" rule).** Scratch harness
(`measure_binaural_room.py`, pure stdlib cmath + own radix-2 FFT, parses `KemarHrir.h`) verified the
design before any C++ was written:

- **Delays** = primes in samples @44.1 kHz → mutually non-harmonic *by construction*:
  367/499/641/773/919/1061 smp = 8.3/11.3/14.5/17.5/20.8/24.1 ms. Closest pair ratio is 0.106 away
  from an integer — nothing rings on a pitch. All ≥ 8 ms (comb-fusion zone avoided).
- **Azimuths** ±30–70°, alternating sides (55, −40, 70, −60, 30, −50): lateral kernels carry the
  out-of-head cue; alternation balances the ears (measured L/R wet asymmetry 0.7 dB).
- **Level neutrality (AC6):** the naïve normalisation constant (Σg²·kernel-power) was **wrong by
  2.4 dB** — the kernel pairs are pair-normalised (not per-ear) and the send damping eats wet power.
  Fix: `kReflectionPower = 0.45` is the *empirical* per-ear pink-weighted power of the whole wet
  path (kernels + damping + cross-tap incoherence), measured, then gains rescaled. Result: output
  level within **±0.1 dB** of dry at room = 0 / 0.5 / 1.
- **Centre transparency (AC2):** octave bands at room=1 stay within **−1.4…+0.6 dB** of dry — a
  smooth room tilt, no comb colour. (The 1/3-oct spread of 5.9 dB is fine-grained ripple that reads
  as ambience; the octave profile is the tonal-shift metric.) The Story-10.3 centre win is kept.
- **Damping:** one-pole LP at 5.5 kHz on the reflection send (wall/air absorption) — keeps the
  direct sound crisp, the room dark, and the HF comb inaudible.

**DSP (`Source/DSP/BinauralRoom.h`, new, AC1/AC3):** static-sized (8192-sample pow-2 ring, fixed
constexpr tap table), kernels are *pointers into* the constexpr KEMAR table (no copies), zero
allocation anywhere; `prepare(sampleRate)` scales the ms delays to the host rate (+ ring guard) and
`reset()` clears state. Dry/wet gains ramp linearly across each block (anti-zipper). At room = 0 the
convolutions are skipped but the ring keeps filling (turning the knob up later is seamless); the
stage itself is only *called* in Kunstkopf mode (`processBlock`, after the — there inert —
`stereoWidth`, before master gain), with a `wasKunstkopf` edge that resets the ring on mode entry.
Other four modes: untouched code path ⇒ byte-identical (AC3). Cost: 6 taps × 2 ears × 128 MACs
once per block on the bus (≈ 1.5 k MAC/sample), independent of voice count.

**Parameter (AC4/AC5):** `hrtfRoom` (Float 0..1, default **0**, persist key `Room` in the `Stereo`
object) appended to `StereoSpecs.h` — append-only, FormatVersion stays 6, missing ⇒ 0 = today's
sound. ROOM knob on the STEREO module (fits W5H1: combo 2 + 3 knobs = 5 ≤ 6 slots); greyed out
outside Kunstkopf via `Knob::activeWhen` (editor-injected predicate, like WIDTH/TIME). Not a
mod-matrix target (AC7, by design).

**RANDOM:** `hrtfRoom` added to the `masterBusIds` snapshot/restore list — and **`outputMode` too**,
which had been missing since Story 10.1 (RANDOM could silently flip the output mode, violating the
"whole MASTER BUS zone untouched" rule). Docs: STEREO help EN+DE (ROOM paragraph), CHANGELOG.

### File List
- `Source/DSP/BinauralRoom.h` (new)
- `Source/PluginProcessor.h` (member + include + mode-edge flag)
- `Source/PluginProcessor.cpp` (prepare, bus stage, RANDOM master-bus list)
- `Source/Modules/StereoSpecs.h` (hrtfRoom param)
- `Source/Audio/Parameters.h` (ID::hrtfRoom)
- `Source/UI/PluginEditor.cpp` (kunstkopfOnly activeWhen)
- `Resources/EN/stereo.md`, `Resources/DE/stereo.md`
- `CHANGELOG.md`

### Verification
- Design measured offline (see above) — AC2/AC6 confirmed numerically.
- Clean rebuild (`/t:Rebuild /nodeReuse:false`) of `JASS_Standalone`; the DSP class additionally
  proven with a standalone console harness (exact passthrough at room=0; reflections present at all
  six tap delays; −0.9 dB white-noise level at room=1) after a first-build bug (ring `writePos`
  never advanced in the active loop → no audible reflections; fixed in `92400e8`).
- **AC8 ✅ user-verified on headphones (2026-07-29):** both listening tests positive — (1) hard-left
  generator, ROOM 0↔1 clearly audible in the far (right) ear (function check); (2) Karplus plucks
  externalize: image sits outside the head at ROOM=1, in-head at 0. Kunstkopf is now audibly a
  different thing from Binaural. Lesson recorded: the effect is inaudible on STEADY tones by design
  (level-neutral + colouration optimised away + no tail) — early reflections are heard on
  transients; the auto-play drone is the worst possible test signal.

### Range extension + default (user decision, 2026-07-29)

The user's ear-tested preferred value was **1.0 — the old ceiling**, i.e. the range was too weak,
not the default wrong. First resolution: tap gains rescaled ×1.571 so full deflection delivers
**wet == dry power** (P = 1.0; the preferred amount then sat at knob 0.67 = √0.45).
Deliberate exception to the "never change global defaults" rule (AC4 originally said default 0):
`hrtfRoom` only acts in the days-old Kunstkopf mode and the factory `outputMode` is Pseudo-Stereo,
so the factory state is unaffected; the user owns the decision.

### Rework: 5-detent room MACRO (user decision, 2026-07-29, same session)

Next ear-test finding: **no audible difference between ~0.2 and 1.0.** That is textbook
psychoacoustics, not a defect and not the user's ears: the direct-to-reverberant JND is **~5–6 dB**
(Zahorik 2002, JASA — one of the coarsest auditory dimensions), spatial impression **saturates**
within ~10 dB of the direct sound (Barron 1971; precedence/Haas fusion), the single-reflection
threshold is ~−15…−20 dB (Olive & Toole 1989) so knob 0.2 (−14 dB wet) was already above threshold
— and this design deliberately removed the two cues (loudness via level-neutrality, comb colour via
the 10.3-style optimisation) that usually fake "more". The linear knob had ~2–3 JNDs of range.

Rework (mirroring what commercial "room amount" macros do — gang parameters):
- UI: **5 detents** (Float step 0.25 — no type change, append-only stays intact; old fine-grained
  values snap on load), each ganging wet level + send damping.
- First spread (wet −24…0 dB, 6 dB ≈ 1 JND per step) still failed the ear test: detents 0–2 were
  inaudible, 3 slight, 4 perfect — the lower half sat below the user's personal effect threshold.
- **Final, ear-calibrated detents:** wet **{off, −6, −4, −2, 0} dB** × damping
  **{—, 3.5, 4.4, 5.6, 7} kHz** — every step inside the audible window; **the top detent is the
  ear-tested "perfect" setting, preserved bit-exact** (wet 0 dB / 7 kHz / P=1.107). Per-detent
  MEASURED normalisation constants (`kDetWetPower = {—, 0.814, 0.904, 1.008, 1.107}`), level-neutral
  ±0.2 dB at every detent.
- Default **1.0** (the "perfect" top detent — user decision).
- Re-measured: level-neutral ±0.2 dB across the sweep; octave colouration at full −2.6…+1.4 dB.
