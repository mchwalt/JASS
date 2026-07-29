# Story 10.5: Kunstkopf elevation (height) axis

Status: draft — deliberately sequenced AFTER Story 10.4

<!-- Raised together with Story 10.4 on 2026-07-29. The user asked for both ("Beides, Reflexionen
     zuerst"), with this one as the follow-up so its necessity can be judged after 10.4 lands. -->

## Story

As a sound designer,
I want a **HEIGHT** control in the Kunstkopf mode, so I can place a generator above or below the
horizontal plane — something the parametric Binaural mode cannot do at all.

## Context / Decision

- Today the embedded table holds the **horizontal plane only** (`elev0`, azimuths 0–90° in 5° steps).
  The MIT KEMAR compact set also ships elevations from −40° to +90°, which we do not use.
- **Re-evaluate before building.** Height perception through *another person's* pinnae is unreliable:
  the spectral notches that encode elevation are individual, so most listeners hear a **timbre
  change** rather than a change in height. This story will certainly make Kunstkopf *different* from
  Binaural — but possibly not in the way the label promises, which is a UX risk, not just a DSP one.
  Story 10.4 (early reflections) targets the same goal — making Kunstkopf distinct and genuinely
  out-of-head — with a far more reliable mechanism and no new data. **Judge whether 10.4 already
  satisfied the need before starting this.**
- **The raw data is not in the repo** (correctly — only the generated header is committed, and the
  `elev0` values were recovered back out of it during the 2026-07-29 session). This story therefore
  requires re-downloading <https://sound.media.mit.edu/resources/KEMAR/compact.zip> per
  `tools/README.md`.

## Open questions (resolve before dev)

1. **Per generator or global?** Seven per-generator HEIGHT params (symmetric with PAN, 7 new params,
   more rack clutter) vs. one global elevation for the whole mode (cheap, but much less expressive).
   PAN is per generator, so symmetry argues for per generator; cost argues against.
2. **How many elevations to embed?** The header is 66 KB for one elevation. Five to seven elevations
   put it at ~350–460 KB of `constexpr` float. Acceptable? Alternative: fewer measured elevations
   plus interpolation, or a coarser azimuth grid to buy elevation resolution.
3. **Interpolation.** Kernel selection becomes bilinear over (azimuth, elevation) — 4 kernels blended
   instead of 2. Still once per block, so cheap, but `setPanForBlock` grows a dimension.
4. **Naming.** "HEIGHT" is honest about the intent; if it mostly reads as timbre, a name like
   "TILT" or "ELEV" may set expectations better. Decide with the user after hearing it.

## Acceptance Criteria (provisional — firm up after the open questions)

1. `tools/gen_kemar_hrir.py` extended to read multiple elevation folders and emit a 3-D table, with
   the same post-processing pipeline (frontal-reference EQ, synthesised low end, per-position level
   normalisation) applied per position. The **level must stay constant across elevation too**, not
   just azimuth — otherwise the knob doubles as a volume control.
2. The `--from-header` re-processing path keeps working (or is explicitly retired) for the new layout.
3. New parameter(s) per open question 1, **append-only**, default 0 (horizontal) so every existing
   preset is unchanged. Greyed out outside Kunstkopf via `Knob::activeWhen`.
4. Kernel selection interpolates over both axes; still **once per block**, still allocation-free.
5. Verified by build + running app on headphones — and honestly reported: does it read as height, or
   as timbre? If the latter, decide with the user whether to keep, rename, or drop it.
6. Header regeneration is followed by a **clean rebuild** (`/t:Rebuild`) — the table size is compiled
   into the voice, and stale TUs have produced startup heap corruption before.
7. README "Third-party data" attribution updated if the data scope changes.

## Dev Notes

- Same discipline as 10.3/10.4: verify the emitted kernels numerically (flatness at the neutral
  position, level across positions, ITD/ILD fidelity) before judging by ear. Two ear-guessed
  constants in this epic were measurably wrong.
- Files (expected): `tools/gen_kemar_hrir.py`, `tools/README.md`, `Source/DSP/KemarHrir.h`
  (regenerated), `Source/DSP/HrtfPanner.h`, `Source/Audio/SynthVoice.{h,cpp}`,
  `Source/Modules/*Specs.h`, `Source/Audio/Parameters.h`, `Source/UI/PluginEditor.cpp`,
  `Resources/{EN,DE}/stereo.md`, `README.md`, `CHANGELOG.md`.
- Deliver on branch `develop`, no push/merge — author's call ([[feedback_git_workflow]]).

## Dev Agent Record

_Not started — story recorded 2026-07-29 alongside Story 10.4._
