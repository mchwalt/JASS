# Story 15.1: STEP SEQ — a 16-step note sequencer

Status: ready-for-dev — scope agreed with the user 2026-08-09 ("erst Preset, dann (einfacher)
Sequenzer"). Two decisions below need the user before the rack body is built.

<!-- Raised 2026-08-09 after measuring DAF "Der Mussolini" (1981) as a concrete target. The
     measurement is the acceptance test: the figure either comes out, or it does not. -->

## Story

As a player,
I want a step sequencer that plays a short note figure in time with the tempo,
so that JASS can drive a sequencer bass by itself instead of only sounding the one note I hold.

## Why now — and what the target actually is

The reference was measured, not guessed (own FFT/autocorrelation over the source WAV; the intro
0–6 s is bass-solo and clean). Numbers to build against:

| | measured |
|---|---|
| step | **192.0 ms → 156.25 BPM, eighth notes** (191.8–192.0 ms across the full 3:55 — the machine does not drift) |
| figure | **16 steps, every step played, no rests** |
| notes (MIDI) | `35, 50, 35, 47, 35, 35, 47, 35, 47, 35, 47, 45, 35, 47, 35, 40` — a B pedal with octave jumps plus D/A/E |
| relative to the root | `0, +15, 0, +12, 0, 0, +12, 0, +12, 0, +12, +10, 0, +12, 0, +5` |
| articulation | **legato** — the envelope never falls between steps; gate ≈ 100 % |
| accents | none (step peaks vary ±1.5 dB, no pattern) |
| portamento | none (pitch change complete within one cycle, ~15 ms) |

The preset `DAF Bass` (in `%AppData%\JASS\Presets`, not yet in the repo) already reproduces the
**tone** and is user-verified. What no part of JASS can play today is the **figure**.

## What already exists — do not rebuild it

- **The clock.** `SyncDivision.h` + the MASTER `syncTempo` param, with the host BPM overriding it
  when hosted (`PluginProcessor.cpp` ~line 719). DELAY and the LFOs already ride on it.
- **The note-emission model.** `Arpeggiator::processBlock` + its call site
  (`PluginProcessor.cpp:835-873`): drop the raw channel-1 note on/off from the buffer, emit the
  generated notes into it instead, leave the channel-16 drone alone. Copy that shape.
- **The per-step filter sweep does NOT need this story.** An LFO set to Sawtooth + SYNC 1/8, routed
  through the mod matrix to FILTER CUTOFF with a negative amount, already produces it — that is how
  `DAF Bass` works today. The per-step modulation row is therefore explicitly **out** of this story.

The ARP cannot do the job: it plays only *held chord tones* in a fixed direction (no authored pitch
per step, no octave jumps), and it runs **free in Hz** — its own comment says "a later tempo-sync
could replace rateHz with a musical division" (`Arpeggiator.h:17`).

## Scope

**In:** one pattern · up to 16 steps · per-step semitone offset · per-step gate (0 = rest) ·
step length as a musical division · transposed by the held key · stored in the preset.

**Out:** song mode, pattern chaining, several tracks, recording, MIDI file import/export, and the
per-step modulation row (→ Story 15.2). Rationale agreed with the user: the moment those appear we
are competing with REAPER, which he already owns — and as a VST3 the host sequences anyway, so the
internal sequencer only earns its keep in the standalone.

## Acceptance Criteria

1. **New module `STEP SEQ`** in the MODULATION zone, `persistObject "StepSeq"`, enable param
   `seqOn` **default off**. A preset saved before this story loads bit-identical (missing ⇒ default).
2. **Clock.** A `SYNC` combo fed **verbatim** from `SyncDivision::kNames` (same as DELAY/LFO) plus a
   free-running `RATE` in steps/s for the `Free` entry. Default `1/8`. At 156 BPM one step is then
   192.3 ms — within 0.3 ms of the measured 192.0 ms.
3. **Pattern parameters.** 16 × pitch (`Int`, −24…+24 semitones, default 0) and 16 × gate
   (`Float` 0…1, default 1.0, **0 = rest**) — gate carries both "does this step sound" and "how
   long", so no third parameter per step. Plus `LENGTH` (1…16, default 16). All append-only.
4. **Trigger model.** The pattern runs while at least one channel-1 key is held; the **lowest held
   note is the root** and each step's offset is added to it. The raw held notes are swallowed exactly
   as the ARP does, so only the pattern sounds. Releasing the last key stops it and releases any
   sounding step. Starting to play from silence restarts the pattern at step 0; adding a key to an
   already-running pattern does **not** restart it.
5. **Sample-accurate emission** into the MIDI buffer from `processBlock`, no allocation on the audio
   thread (reuse the ARP's scratch-member pattern).
6. **Gate 1.0 is truly legato** — the reference is legato, and this is the one place the
   implementation can easily be wrong: the previous step's note-off must not cut the next step's
   note-on. Verify both that there is no audible gap *and* that no note is left hanging (also when
   consecutive steps carry the same pitch, which is most of the DAF figure).
7. **ARP interaction** resolved and visible in the UI — see the decision below.
8. **Persistence** append-only, no `kFormatVersion` bump.
9. **Help texts** `Resources/{EN,DE}/stepseq.md`, terse per the house rules, both languages in one pass.
10. **The acceptance test is the figure.** With `LENGTH 16`, `SYNC 1/8`, MASTER tempo 156, gates at
    1.0 and the offsets `0,+15,0,+12,0,0,+12,0,+12,0,+12,+10,0,+12,0,+5`, holding **B1** must produce
    the measured line. Verified by the user against the record.

## Decide with the user before building the rack body

1. **Size, and what it costs.** 16 steps × 2 controls = 32 controls; the largest existing class is
   `W24H2` (MOD MATRIX fits 6 slots × 4 controls there), so this needs a **hand-built body** like the
   SAMPLER's, or a third row. The trap: `Rack::maxHeight()` measures the rack **with every module
   visible** (`Rack.cpp:543-551`) and the editor derives its one display-fit scale from that — so a
   new tall module shrinks the **entire window**, permanently, *even when the module is hidden*.
   Today: worst-case rack 1980 px → factor 0.65 → 988 px window. Measure the new factor before and
   after and put the number in front of the user; a taller rack is his call, not ours.
2. **ARP vs STEP SEQ.** Both replace the held chord, so they cannot both run. Recommendation: make
   them mutually exclusive (enabling one switches the other off, so the rack always shows the truth)
   rather than a silent precedence rule that leaves a lit ARP doing nothing.

## Dev Notes

- **Parameter count.** ~36 new APVTS params (16 + 16 + 4). Precedented — the mod matrix added 33 in
  one go — but it is the largest single addition since. They all become host-automatable; keep them
  in their own parameter group.
- **Combo item order MUST equal the enum order** (`ComboBoxAttachment` maps by index). This project
  has been bitten by that twice (LFO WAVE, OSC WAVE). Feeding the SYNC combo straight from
  `SyncDivision::kNames`, as DELAY does, is the safe pattern — do not retype the list.
- **Do not touch global defaults** — missing ⇒ default hits old presets retroactively.
- A new `.md` resource needs a **CMake reconfigure + `/t:Rebuild /nodeReuse:false`** of the help
  targets, otherwise the link fails with LNK1236.
- Build with `/t:Rebuild`: core structs are embedded by value in `SynthVoice`.
- Files (expected): `Source/DSP/StepSequencer.h` (new), `Source/PluginProcessor.{h,cpp}`,
  `Source/Modules/StepSeqSpecs.h` (new) + `ModuleRegistry.cpp`, `Source/Audio/Parameters.h`,
  `Source/Audio/PresetIO.h`, `Source/UI/PluginEditor.cpp` (hand-built body),
  `Resources/{EN,DE}/stepseq.md`, `README.md`, `CHANGELOG.md`.
- **No unit-test rig**: verification is build + running app + the user's ear. Say plainly when
  something is only build-verified.
- Deliver on a feature branch; **no push, no merge** — the maintainer decides.

## Follow-up: Story 15.2 (not this story)

A second per-step row feeding a mod-matrix **source**, so the sequencer can move the filter cutoff
(or anything else) per step — the Korg SQ-10 had three rows and the second one classically went to
the MS-20's cutoff. Only worth building once row one proves itself: a tempo-synced LFO already
covers the DAF case, which is why it is not in 15.1. Needs a new entry in `ModSource` +
the SRC list in `ModMatrixSpecs.h` (append-only, order must match).

## Dev Agent Record

_Not started — story recorded 2026-08-09._
