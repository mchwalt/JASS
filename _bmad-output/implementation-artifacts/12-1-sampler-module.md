# Story 12.1: SAMPLER module (samples as a sound source)

Status: **ready-for-dev** (scope settled 2026-07-30, see "Scope decision" below).

<!-- Raised 2026-07-29 in conversation. "wir müssen das aber nochmal später ausdiskutieren, vor allem
     das Thema Multi-Sampling und ob man das ganze nicht doch in ein Schwester-Projekt packt - man muss
     das Rad ja nicht zweimal erfinden" … "es sei denn, man schafft es, das Multisampling mit allem
     drum und dran tatsächlich in ein Modul zu verpacken". -->

## Story

As a sound designer,
I want to load audio files and play them back at the pitch I press, so I can use recordings —
one-shots, loops, textures, drums, and possibly multisampled instruments — as a sound source inside
JASS, running through the engine that is already there.

## Scope decision (user, 2026-07-30)

Asked directly — "eigenständigen Sampler ausliefern & pflegen, oder Samples in JASS spielen?" —
the answer was **the latter's first option: own recordings/one-shots/textures as a sound source
inside JASS** ("Ersteres"). Consequences:

- **Option A — module in JASS.** No sister project, no standalone product, no DSP-library extraction.
- **v1 = ONE sample per set** (one-shot / texture / loop): no mapping, no zone logic. **ROOT stays a
  day-one param** so multisampling remains reachable without a format break.
- **Multisample import (folder naming convention / `.sfz`) = follow-up Story 12.2**, not v1. The
  analysis below (authoring vs. using a mapping) remains valid and is the blueprint for 12.2.
- **Memory:** own recordings are MB-scale, not GB. v1 keeps the proven never-free store policy but
  BOUNDED: per-file cap (~60 s stereo) + set-count cap (32). No disk streaming, no reclamation
  rework in v1 — revisit only if 12.2 (multisample sets) materialises.
- The point of the module is the JASS chain: sample → filter/formant/wavefolder/matrix/arp →
  per-generator PAN → Kunstkopf + ROOM. That combination is the feature, not sampling fidelity.

## Original discussion notes (kept for 12.2 context)

### 1. Can multi-sampling fit in a module? — probably YES, and the first analysis was wrong

An earlier draft claimed full multisampling needs a zone editor and therefore a separate product.
**That conflated two different things: AUTHORING a key mapping and USING one.** Only authoring needs
an editor. A module that *derives* or *imports* the mapping needs no editor at all:

- **Derive it** from a filename convention (`Piano_C3.wav`, `Pad_A#4.wav`) — parse the root key, then
  give each sample the range that splits halfway to its neighbours. UI cost: one "load folder" action.
- **Import it** — an `.sfz` is a text file that already contains the full mapping. UI cost: one file
  action. This is the purest form of "don't invent the wheel twice": consume mappings other tools
  author, rather than building an authoring tool.

And the shape is already proven in this rack. `WavetableBankStore` (`Source/DSP/WavetableBank.h`) is
exactly the required architecture, already RT-hardened:

- named collections, enumerable via `getNames()` — which is what feeds the WAVETABLE BANK combo;
- loaded on the **message thread only** (`loadWav`), published to the audio thread by an **atomic
  count with release/acquire**, append-only, stable addresses, never mutating a slot in use;
- duplicate-safe by name (re-loading the same file re-selects it);
- loaded banks are **never freed**, because a voice caches the raw pointer for a whole render block —
  the use-after-free hazard is documented in that file.

A `SampleSet` is structurally the same object: a named collection of (buffer, rootKey, keyLow,
keyHigh) instead of (frames). Same store mechanics, same combo, same FileAction, same
preset-references-by-name (which is also why the usual "sampler presets are unportable" problem does
not arise here).

**So the UI objection dissolves.** What does not dissolve is memory — see the next point.

### 2. The real wall is memory, not the interface

- Wavetables are kilobytes; `MaxBanks = 64` is free. A multisampled instrument is megabytes to
  gigabytes.
- The store's deliberate **never-free policy** (correct for KB banks, chosen to avoid the documented
  UAF) becomes a leak-shaped problem at MB scale: every set loaded in a session accumulates. Fixing
  it properly needs safe reclamation (e.g. deferred free once no voice can hold the pointer), which
  is new work on a recently hardened subsystem — treat it as part of this story, not a detail.
- **Disk streaming** (background reader thread + lock-free ring per voice) does not exist in this
  codebase at all. That is the genuine dividing line: a bounded in-RAM set is a module; a
  GB-scale streamed library is a different engineering project.
- Loading hundreds of files must not block the message thread — needs a background load with
  progress, which the current one-file `loadWav` does not do.
- **Not** a problem: per-voice DSP cost. One sample sounds per voice regardless of how many are
  mapped, so a large set costs memory, not CPU.

### 3. Module in JASS, or sister project?

The constraint "don't invent the wheel twice" cuts both ways, which is why this needs deciding:

| option | duplication | cost |
|---|---|---|
| **A. Module inside JASS** | none by construction — the sample inherits filter, formant, wavefolder, delay, reverb, the 8-slot mod matrix, arpeggiator, poly-glide, HRTF panning | rack space, memory management above, and JASS drifts toward "does everything" |
| **B. Sister project + extracted shared core** | none, but only if `Source/DSP` (and arguably the `rack` UI framework) is first pulled into a library both products consume | a real refactor of a working, recently RT-hardened codebase; two build targets, two release pipelines |
| **C. Sister project with its own engine** | **the wheel invented twice** — what the user explicitly wants to avoid | rejected |

Revised recommendation to argue from: **A**, with the dividing line drawn at *streaming and
authoring*, not at multisampling. A module can do "load a folder or an `.sfz`, keys derived or
imported, bounded set kept in RAM". What would justify **B** is wanting to edit mappings inside the
app, or wanting GB-scale streamed libraries. Worth asking directly: is a standalone sampler something
to ship and maintain, or is the real wish just to have samples available inside JASS?

## Context: what exists, and what does not

**Reusable — more than expected:** `juce::AudioFormatManager` / `AudioFormatReader` WAV reading is
already in `WavetableBank`; the shared lock-free store described above; user assets in AppData
referenced by name; the "LOAD WAV" `FileAction` pattern; per-generator PAN / enable / mod-matrix
wiring.

**NOT reusable — the playback engine is genuinely new.** `WavetableOscillator` is not a starting
point. It advances a phase normalised to ONE cycle and wraps it
(`WavetableOscillator.h:51-53`): pitch sets how fast a single cycle is traversed, table length is
fully decoupled from time, and the sound never changes duration. A sampler needs the opposite — one
**absolute** read position running through a long buffer at `rate = freq / rootFreq`, to the end or to
a loop point, with the recording's own amplitude envelope as part of the sound. The two share nothing
but interpolating into a float buffer.

**Why multisampling is not merely a nice-to-have:** because a sampler scales the whole time axis with
pitch, formants move with it, so one sample stretched across the keyboard falls apart after roughly an
octave (chipmunk upward, molasses downward). A single-sample module is still coherent for JASS — it is
not a realism instrument (wavefolder, bitcrusher, Karplus, formant filter), so transposition damage is
material rather than defect — but "one sample per instrument" and "sounds like the real thing" are
mutually exclusive, and the story should not pretend otherwise.

## Acceptance criteria (v1, scope as decided above)

1. New `Source/DSP/SamplePlayer.h` (per voice) + a shared immutable sample store modelled on
   `WavetableBankStore` (single instance, lock-free reads, loading off the audio thread — Epic 11
   rules apply, no allocation in `process`). Never-free policy kept, BOUNDED by the caps in #3
   (no reclamation rework in v1).
2. Params: LOAD (FileAction), **ROOT** key, START, END, LOOP (off / sustain, with a click-free
   crossfaded join), REVERSE, LEVEL, PAN. Append-only; FormatVersion stays 6. ROOT exists from day
   one even in the single-sample version, so the multisample route (12.2) stays reachable without a
   format break.
3. **Caps, documented in the help text:** max ~60 s stereo per file, max 32 loaded sets; loading a
   file over the cap is rejected with a friendly message (no truncation surprises).
4. `kNumPanGenerators` 7 → 8, plus a mod-matrix module entry. Two consequences: in Kunstkopf mode
   every generator carries its own 128-tap convolution **per voice**, so an eighth adds real CPU
   there; and generators are embedded by value in `SynthVoice`, so the struct grows → **clean
   rebuild** (`/t:Rebuild`) or the startup heap corruption returns.
5. Module kept **small** and **default-hidden** (like COMPRESSOR) — large modules trigger the global
   auto-fit downscale and shrink the whole rack.
6. Help text (EN+DE) states the distinction from WAVETABLE explicitly — pitch-locked single-cycle
   timbre vs. time-locked recording — and the usable transposition range.
7. Verified by build + running app ([[feedback_ui_verification]]).

## Dev Notes

- Interpolation quality matters more than for wavetables: a transposed recording exposes aliasing on
  upward shifts. Decide linear vs. cubic vs. cheap oversampling and **measure** it rather than guess
  ([[feedback_measure_dont_guess_dsp]]).
- Loop-point clicks are the classic defect — needs at least a short crossfade at the join.
- Deliver on branch `develop`, no push/merge — author's call ([[feedback_git_workflow]]).

## Dev Agent Record

### Implementation (2026-07-30)

- **Stereo decision (user, mid-implementation): stereo files STAY stereo** — downmix only where the
  engine itself is mono (Mono / Pseudo-Stereo modes). Realised via TWO pan-generator slots
  (`PanSamplerL/R`, `kNumPanGenerators` 7→9): a stereo set's channels are two mono sub-sources
  spread ±0.5 around the PAN knob, each with its own equal-power/binaural/HRTF placement; the
  SamplerPan mod target moves both together. Mono sets use slot L at the plain pan.
- **Interpolation measured** (scratch harness): linear 28.5 dB vs **Hermite 38.3 dB** SNR on a
  bright component at +7 semitones → 4-point Hermite.
- `Source/DSP/SampleBank.h` (SampleSet + SampleBankStore, WavetableBankStore mechanics, caps 60 s /
  32 sets), `Source/DSP/SamplePlayer.h` (absolute-position playback, ROOT-key rate, START/END,
  modes One-Shot/Loop/Reverse/Rev-Loop with ~6 ms crossfaded loop join).
- Params via `SamplerSpecs.h` (appended last, format stays v6); REVERSE folded into the MODE combo
  (deviation from AC2's separate LOOP+REVERSE — one control instead of two).
- **Preset portability:** LOAD copies the file into `%AppData%\JASS\Samples`; PresetIO persists
  "Sampler.File" (name) beside the session SET index and re-resolves it on load (RackLayout-style
  special case in toVar/applyVar).
- Matrix: SAMPLER module (LEVEL/PAN) appended to catalog + 2 LFOTargets appended; editor body
  hand-built (dynamic SET combo + LOAD FileAction + AlertWindow on cap rejection), default-hidden.
- **Session iterations (user-tested):** SPEED knob (0.25×–4×, tape-style, live); example catalog
  (`Samples/` repo folder → embedded → seeded → PRE-LOADED into the SET combo at startup,
  alphabetical = stable indices); SET combo needed `indexIsValue` (classic combo-index bug class —
  found by ear: "Brass" played CH_01); module default-VISIBLE (user decision, overrides the AC);
  keyboard now labels MIDI 60 as **C4** (`setOctaveForMiddleC(4)` — JUCE's C3 default made "plays
  original at C3" look like a rate bug); **shared loop clock** (master phase at ROOT×SPEED,
  loop notes start on it + HARD resync of all loop voices at each master wrap = beat-lock).
- **→ Story 12.2 scope note (user, 2026-07-30):** playing one WAV at several pitches while keeping
  playback speed/loops truly in sync is its own topic (pitch/time DECOUPLING = timestretch) and
  belongs with multisampling — v1 stays tape-style + beat-resync by design.
