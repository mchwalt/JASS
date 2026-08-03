# Story 12.3: SAMPLER pitch/time decoupling (timestretch)

Status: ready-for-dev (one open decision: third-party vendoring — see "Open decision" below)

<!-- Split out of the original combined 12.2 draft (user decision 2026-08-03). Scope set by user
     2026-07-30: "playing one WAV at several pitches while keeping playback speed/loops truly in
     sync is its own topic (pitch/time DECOUPLING = timestretch)". Independent of Story 12.2
     (multisampling) — works on single-sample sets too; if 12.2 lands first, stretch must compose
     with zones. -->

## Story

As a sound designer,
I want an optional SAMPLER mode where the key sets only the pitch and SPEED sets only the playback
time,
so that transposed loop voices stay truly beat-locked (instead of being hard-resynced and cut at
each master-clock wrap) and recordings keep their duration when played across the keyboard.

## The coupling, precisely

Today `rate = transposeRatio · 2^((60−root)/12) · fileSR/hostSR` (`SamplePlayer::trigger`) and
`step = rate · speed` (`nextSample()`) — pitch and time are one number. That product is the single
place to decouple. Consequence today: differently-pitched loop voices traverse START..END at
different speeds, so the shared loop clock HARD-resyncs them each master wrap and a transposed
voice cuts its pass (documented in `setLoopSyncPhase`). 12.1 kept this tape-style behaviour by
design; this story adds the alternative without touching the default.

## Acceptance criteria

1. **New appended param `samplerStretch`** (Bool, default OFF). OFF = 12.1 tape-style behaviour,
   bit-identical (regression guard: old presets/state have no field ⇒ default OFF ⇒ nothing
   changes). ON = key sets pitch only; SPEED (and the region) sets time only; all loop voices
   traverse START..END in the same wall-clock time regardless of pitch ⇒ the master-wrap hard
   resync becomes a no-op by construction (keep the resync code path for OFF mode).
2. **Engine choice is MEASURED, not guessed** ([[feedback_measure_dont_guess_dsp]]). Candidates:
   - **signalsmith-stretch** (github.com/Signalsmith-Audio/signalsmith-stretch): header-only C++11,
     MIT (GPLv3-compatible — README "Third-party" attribution like KEMAR), built exactly for
     polyphonic pitch-shift at time-rate 1:1. Configure/allocate in `prepareToPlay` ONLY (its
     setup allocates); verify `process()` is alloc-free before committing (Epic 11 rules).
   - **Own granular repitcher**: fixed ~50 ms Hann-crossfaded grains; grain read uses the existing
     Hermite `read()`; position advances at time-rate, grains resampled at pitch ratio. RT-safe by
     construction, "character" artifacts acceptable — JASS bends recordings, it does not imitate.
   Scratch-harness measurement (like the 12.1 Hermite bake-off): quality on a bright reference
   sample at ±7 and ±12 semitones AND per-voice CPU at 16 voices stereo. Record numbers in the Dev
   Agent Record; pick the winner; wire the loser out cleanly (no dead half-integrations).
3. **Latency/phase honesty:** if the chosen engine delays the sampler relative to the other
   generators, measure the group delay and state it in the help text (EN+DE). No compensation
   machinery in this story.
4. Stretch mode composes with all four playback modes (One-Shot/Loop/Reverse/Rev-Loop), with the
   loop crossfade (no reintroduced clicks — grain windows or stretcher continuity cover it; verify
   by ear + capture), with LEVEL/PAN modulation, and — once Story 12.2 exists — with multisampled
   zones.
5. All params append-only, no APVTS ID renamed/reordered; FormatVersion stays 6.
6. Help texts `Resources/EN/sampler.md` + `Resources/DE/sampler.md` explain the trade-off
   (tape-style vs stretch, artifacts, latency); README feature bullet + third-party attribution if
   signalsmith-stretch is vendored; CHANGELOG Unreleased entry; `docs/ARCHITECTURE.md` /
   `docs/MODULE_SYSTEM.md` sampler passages extended.
7. Module footprint: one STRETCH toggle, nothing else (W12H1 budget — large modules trigger the
   global auto-fit downscale).
8. Verified by build + running app ([[feedback_ui_verification]]): old preset regression
   (Sampler Demo F7 unchanged with STRETCH off), beat-lock of transposed loop voices with STRETCH
   on (play the same loop at C4 + G4 + C5 and hear one shared round), duration invariance of a
   one-shot across an octave.

## Tasks / Subtasks

- [ ] Task 1: Engine bake-off (AC 2) — scratch harness, quality + CPU numbers into Dev Agent
      Record, decision documented
- [ ] Task 2: Vendor or implement the winner (AC 2) — `Source/ThirdParty/signalsmith-stretch/` +
      README attribution, OR granular engine header in `Source/DSP/`
- [ ] Task 3: Integration (AC 1, 4) — `samplerStretch` param appended in `SamplerSpecs.h`,
      decoupled step in `SamplePlayer`, `prepareToPlay` allocation, mode/crossfade/mod
      composition, resync no-op in stretch mode
- [ ] Task 4: Editor toggle (AC 7) — STRETCH in the hand-built SAMPLER body
      (`Source/UI/PluginEditor.cpp`)
- [ ] Task 5: Latency measurement + docs + help + CHANGELOG (AC 3, 6)
- [ ] Task 6: Verification pass (AC 8) — **clean rebuild mandatory**, see guardrails

## Dev Notes

### Open decision (user)

If the bake-off favours signalsmith-stretch, third-party MIT code gets vendored into the repo
(`Source/ThirdParty/signalsmith-stretch/`, license header kept, README attribution). The user has
not yet approved vendoring — confirm before Task 2 commits third-party code; the granular
fallback needs no approval.

### Integration map (subset relevant here; verified against code 2026-08-03)

- `Source/DSP/SamplePlayer.h` — `trigger(transposeRatio)` computes `rate`; `nextSample()` steps
  `rate*speed`; loop crossfade `kXfadeSamples=256` equal-gain against `pos∓len`; Hermite
  `read(p,ch)`; `setLoopSyncPhase` detects master wrap (`f < syncPhase−0.5`) and hard-resyncs
  loop voices — the code path that stays for OFF and becomes a no-op for ON.
- Master loop clock: `PluginProcessor::samplerMasterFrac`, advanced once per block in
  `processBlock` ("Story 12.1: shared SAMPLER loop clock" block), delivered via
  `Parameters::applyToVoice(..., samplerMasterFrac, ...)` → `setLoopSyncPhase`. In stretch mode
  the master clock still defines the round; voices simply never drift from it.
- Voice: `Source/Audio/SynthVoice.h/.cpp` — `SamplePlayer sampler;` embedded BY VALUE;
  `baseSamplerLevel` capture/restore per block; `LFOTarget::SamplerLevel`/`SamplerPan` per-sample
  mod; stereo sets render as two `addPanned` calls into `PanSamplerL`/`PanSamplerR`
  (`kNumPanGenerators = 9` in `Source/DSP/ChannelStrip.h` — unchanged by this story).
- `Source/Modules/SamplerSpecs.h` — existing params `samplerOn/Set/Root/Start/End/Mode/Level/
  Pan/Speed`; `samplerStretch` appends at the END. ID in `Parameters::ID`; `AllModules.h` order
  untouched. Editor body is hand-built in `Source/UI/PluginEditor.cpp` (`// SAMPLER (Story 12.1)`
  block).
- Persistence: nothing new — a Bool param flows through APVTS + `PresetIO` object persistence
  automatically (`persistObject="Sampler"`); missing field ⇒ default OFF is exactly the
  back-compat story.

### Guardrails (violations caused real defects before)

- **`SamplePlayer` grows fields (stretch state) and is embedded BY VALUE in `SynthVoice` ⇒ struct
  sizes change ⇒ `/t:Rebuild` (clean rebuild) MANDATORY** — incremental builds after header
  struct-size changes caused the 0xC0000005 startup heap corruption (2026-07-24 lesson).
- Audio thread: no allocation/locks/logging in `processBlock`/`nextSample` (Epic 11). Stretch
  buffers allocated in `prepareToPlay` only (called repeatedly — re-entrant safe).
- Never change existing param defaults for a new scenario — `samplerStretch` default OFF is the
  pattern; SPEED semantics with STRETCH off must stay bit-identical.
- Scratch-harness measurements use pure cmath + own FFT, no numpy
  ([[feedback_measure_dont_guess_dsp]]).
- Deliver on `develop`; never push/merge — author's call ([[feedback_git_workflow]]). No Claude
  attribution in commits/PRs.

### Previous story intelligence (12.1)

- Hermite chosen by measurement (38.3 dB vs linear 28.5 dB at +7 st) — grains reuse `read()`.
- Loop-click designed out via 256-sample equal-gain crossfade; stretch must not reintroduce
  joins/clicks.
- SPEED is a live tape-style rate multiplier (0.25×–4×, skew 0.5) — in stretch mode it becomes a
  pure time-rate; keep it live (no retrigger needed on change).

### Project Structure Notes

- Granular engine: header-only in `Source/DSP/`. Vendored third-party: `Source/ThirdParty/…`
  subfolder + README attribution (pattern: the KEMAR "Third-party data" section).
- `_bmad-output/project-context.md` is PARTIALLY STALE (kFormatVersion is 6 not 1, AppData is
  `%AppData%\JASS` + `.jass`, output stage is the 5-mode STEREO module) — trust
  `docs/ARCHITECTURE.md` / `docs/MODULE_SYSTEM.md` and the code where they disagree.

### Testing standards

No unit-test rig. Verification = clean build + running Standalone app + user listening
([[feedback_ui_verification]]), plus the scratch-harness bake-off numbers (AC 2) and the group
delay measurement (AC 3).

### References

- [Source: _bmad-output/implementation-artifacts/12-1-sampler-module.md] — Dev Agent Record
  (shared loop clock, SPEED, stereo sub-slots) and the 12.2 scope note that named this topic
- [Source: _bmad-output/implementation-artifacts/12-2-sampler-multisampling.md] — sibling story;
  AC 4 here covers composition with its zones
- [Source: docs/ARCHITECTURE.md#3, #4.2] — RT rules, per-voice rendering
- [Source: Source/DSP/SamplePlayer.h] — read fully before editing
- signalsmith-stretch: github.com/Signalsmith-Audio/signalsmith-stretch (MIT, header-only) +
  design write-up signalsmith-audio.co.uk/writing/2023/stretch-design/

## Dev Agent Record

### Agent Model Used

### Debug Log References

### Completion Notes List

### File List
