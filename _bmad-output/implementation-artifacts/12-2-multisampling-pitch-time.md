# Story 12.2: SAMPLER multisampling + pitch/time decoupling

Status: ready-for-dev

<!-- Follow-up to Story 12.1 (see its "Original discussion notes" — the analysis there is this
     story's blueprint). Scope set by user 2026-07-30: multisample import (folder naming
     convention / .sfz) + "playing one WAV at several pitches while keeping playback speed/loops
     truly in sync is its own topic (pitch/time DECOUPLING = timestretch) and belongs with
     multisampling". -->

## Story

As a sound designer,
I want the SAMPLER to (A) spread a folder of pitched recordings across the keyboard automatically
and (B) optionally decouple pitch from playback speed,
so that multisampled instruments stay usable over more than ~1 octave (no chipmunk/molasses) and
transposed loop voices stay truly beat-locked instead of being cut at each master-clock wrap.

## Part A — Multisampling (import, never author)

The 12.1 analysis stands: only AUTHORING a key mapping needs an editor; DERIVING or IMPORTING one
needs a single file action. JASS consumes mappings, it never edits them.

### Acceptance criteria (A)

1. **`SampleSet` grows zones.** Each zone = (L/R buffers at file rate, fileSampleRate, rootKey,
   loKey, hiKey). A 12.1 single-sample set is exactly a one-zone set (root from the ROOT knob,
   range 0–127) — one code path, no special case. Store mechanics stay `SampleBankStore` verbatim:
   message-thread load, atomic release/acquire publish of whole immutable sets, append-only,
   never-free, duplicate-safe by name. One multisample set occupies ONE combo slot.
2. **Mapping source 1 — folder naming convention:** a LOAD FOLDER action scans a directory for
   `*_<note>.wav|aif|aiff` (note = name like `C3`/`A#4` **or** MIDI number). Root per file from the
   suffix; loKey/hiKey split halfway between neighbouring roots; outermost zones extend to 0/127.
   **Note names resolve with C4 = MIDI 60** — same convention as the pitch model and the 12.1
   keyboard relabel (`setOctaveForMiddleC(4)`); do not silently adopt SFZ's C4=72-style ambiguity.
3. **Mapping source 2 — `.sfz` import (minimal subset):** LOAD accepts `*.sfz`. Parse only
   `<group>`/`<region>` headers and the opcodes `sample=`, `key=`, `lokey=`, `hikey=`,
   `pitch_keycenter=` (group opcodes inherit into regions; note names or numbers; sample paths
   relative to the `.sfz`). Every other opcode is IGNORED, silently. `key=x` ⇒ lokey=hikey=
   pitch_keycenter=x. Missing `pitch_keycenter` ⇒ 60 (SFZ default).
4. **Caps replace the 12.1 math, documented in help + code comment:** per-zone file cap stays 60 s;
   NEW per-set total-audio cap (all zones summed — pick a value, document the worst-case RAM
   arithmetic next to it; 12.1's worst case was 32 × 60 s stereo ≈ 675 MB, do not exceed that
   envelope); `MaxSets = 32` stays. Over-cap folder/sfz loads reject the whole set with the
   existing AlertWindow pattern (no partial sets, no silent truncation). Never-free stays —
   reclamation rework is explicitly OUT of scope (12.1 scope decision).
5. **Persistence by name, portable:** a loaded multisample set is copied into
   `%AppData%\JASS\Samples\<SetName>\` (folder or `.sfz`+samples) by the existing copy-on-load
   pattern; `PresetIO::toVar/applyVar` keep persisting `Sampler.File` = set name and re-resolve via
   `indexOf(name)` → AppData lookup → `loadFile`/loadFolder. `preloadSamples()` also restores
   multisample subfolders/sfz at startup (alphabetical, stable indices). FormatVersion stays 6 —
   everything is append-only.
6. **Zone selection at note-on:** the voice picks the zone whose [loKey,hiKey] contains the played
   note and computes rate from THAT zone's rootKey + fileSampleRate. `SamplePlayer::trigger` needs
   the MIDI note (pass it, don't reconstruct it from transposeRatio). For multisample sets the
   ROOT knob is inert (mapping wins); for single-sample sets it keeps its 12.1 meaning — state
   this in the help text.
7. All params append-only, no APVTS ID renamed/reordered; `AllModules.h` order untouched.

## Part B — Pitch/time decoupling (timestretch)

Today `rate = transposeRatio · 2^((60−root)/12) · fileSR/hostSR` and `step = rate · speed`
(`SamplePlayer::trigger` / `nextSample`) — pitch and time are one number. That is the single place
to decouple. Consequence today: differently-pitched loop voices traverse the region at different
speeds, so the shared loop clock HARD-resyncs them each master wrap (a transposed voice cuts its
pass — `setLoopSyncPhase` comment documents this).

### Acceptance criteria (B)

8. **New appended param `samplerStretch`** (Bool, default OFF). OFF = 12.1 tape-style behaviour,
   bit-identical (regression guard: old presets/state have no field ⇒ default OFF ⇒ nothing
   changes). ON = key sets pitch only; SPEED (and the region) sets time only; all loop voices
   traverse START..END in the same wall-clock time regardless of pitch ⇒ the master-wrap hard
   resync becomes a no-op by construction (keep the resync code path for OFF mode).
9. **Engine choice is MEASURED, not guessed** ([[feedback_measure_dont_guess_dsp]]). Candidates:
   - **signalsmith-stretch** (github.com/Signalsmith-Audio/signalsmith-stretch): header-only C++11,
     MIT (GPLv3-compatible — add to README "Third-party" like KEMAR), built exactly for polyphonic
     pitch-shift at time-rate 1:1. Configure/allocate in `prepareToPlay` ONLY (its setup
     allocates); verify `process()` is alloc-free before committing (Epic 11 rules).
   - **Own granular repitcher**: fixed ~50 ms Hann-crossfaded grains; grain read uses the existing
     Hermite `read()`; position advances at time-rate, grains resampled at pitch ratio. RT-safe by
     construction, "character" artifacts acceptable — JASS bends recordings, it does not imitate.
   Scratch-harness measurement (like the 12.1 Hermite bake-off): quality on a bright reference
   sample at ±7 and ±12 semitones AND per-voice CPU at 16 voices stereo. Record numbers in the Dev
   Agent Record; pick the winner; wire the loser out cleanly (no dead half-integrations).
10. **Latency/phase honesty:** if the chosen engine delays the sampler relative to the other
    generators, measure the group delay and state it in the help text (EN+DE). No compensation
    machinery in this story.
11. Stretch mode composes with Part A (a multisampled zone can also be stretched), with all four
    playback modes, with the loop crossfade, and with LEVEL/PAN modulation.

## Shared acceptance criteria

12. Help texts `Resources/EN/sampler.md` + `Resources/DE/sampler.md` updated: mapping conventions
    (filename pattern, sfz subset, C4=60), caps, ROOT-inert rule, STRETCH trade-offs. README
    feature bullet + third-party attribution if signalsmith-stretch is vendored; CHANGELOG
    Unreleased entry; `docs/MODULE_SYSTEM.md`/`docs/ARCHITECTURE.md` sampler passages extended.
13. Module footprint stays small (W12H1 if at all possible — large modules trigger the global
    auto-fit downscale). Budget: STRETCH toggle + LOAD FOLDER action; no zone display, no editor.
14. Verified by build + running app ([[feedback_ui_verification]]): old preset regression
    (Sampler Demo F7 unchanged with STRETCH off), folder import plays correct pitches across
    zone boundaries, sfz import of a hand-written 3-region file, beat-lock of transposed loop
    voices with STRETCH on.

## Tasks / Subtasks

- [ ] Task 1: SampleSet zones + store (AC 1, 4)
  - [ ] Zone struct + multi-zone `SampleSet` (single-sample = one zone), caps + worst-case comment
  - [ ] `loadFolder(dir)` / `loadSfz(file)` on the store (message thread, whole-set reject on cap)
- [ ] Task 2: Mapping import (AC 2, 3)
  - [ ] Filename note parser (names C4=60 + MIDI numbers), halfway-split range derivation
  - [ ] Minimal sfz parser (group inheritance, 5 opcodes, relative paths, ignore rest)
- [ ] Task 3: Voice + player (AC 6)
  - [ ] `trigger(transposeRatio, midiNote)` + zone lookup; per-zone root/rate; ROOT-inert rule
- [ ] Task 4: Stretch engine bake-off (AC 9) — scratch harness, numbers in Dev Agent Record
- [ ] Task 5: Stretch integration (AC 8, 10, 11) — `samplerStretch` param, decoupled step,
      prepareToPlay allocation, mode/crossfade/mod-matrix composition
- [ ] Task 6: Editor + persistence (AC 5, 13) — LOAD FOLDER FileAction (needs a `pickDirectory`
      flag on `FileAction` in `ModuleDescriptor.h` + `ModuleFrame.cpp` handling), sfz in LOAD
      wildcard, copy-on-load for sets, PresetIO re-resolve + preload of set folders
- [ ] Task 7: Docs + help + CHANGELOG (AC 12)
- [ ] Task 8: Verification pass (AC 14) — **clean rebuild mandatory**, see guardrails

## Dev Notes

### Integration map (verified against code 2026-08-03)

- `Source/DSP/SampleBank.h` — `SampleSet` (name, `data[2]`, empty R ⇒ mono, file-rate;
  `kMaxSeconds=60`; `loadFromFile` rejects over-cap with nullptr) + `SampleBankStore`
  (`MaxSets=32`, singleton `instance()`, `std::array` slots + `std::atomic<int> count`
  release/acquire, `loadFile` → index or −1, `indexOf` case-insensitive). The never-free comment
  explicitly predicates on the 12.1 caps — update it with the new math (AC 4).
- `Source/DSP/SamplePlayer.h` — state incl. `pos` (absolute fractional), `syncPhase`;
  `trigger(transposeRatio)` computes `rate`; `nextSample()` steps `rate*speed`, loop crossfade
  `kXfadeSamples=256` equal-gain against `pos∓len`; Hermite `read(p,ch)` edge-clamped;
  `setLoopSyncPhase` detects master wrap (`f < syncPhase−0.5`) and hard-resyncs loop voices.
- Master loop clock: `PluginProcessor::samplerMasterFrac`, advanced once per block in
  `processBlock` ("Story 12.1: shared SAMPLER loop clock" block), delivered via
  `Parameters::applyToVoice(..., samplerMasterFrac, ...)` → `setLoopSyncPhase`.
- Voice: `Source/Audio/SynthVoice.h/.cpp` — member `SamplePlayer sampler;` (BY VALUE);
  `trigger` on note-on, `baseSamplerLevel` capture/restore per block, `LFOTarget::SamplerLevel`/
  `SamplerPan` per-sample mod; render: mono host ⇒ `0.5*(l+r)` into `PanSamplerL`, stereo set ⇒
  two `addPanned` into `PanSamplerL`/`PanSamplerR` (each through binaural/HRTF/equal-power).
- `Source/DSP/ChannelStrip.h` — `kNumPanGenerators = 9`, `PanGen{...,PanSamplerL,PanSamplerR}`.
  **Unchanged by this story** (stretch/zones add no pan slots).
- `Source/Modules/SamplerSpecs.h` — params `samplerOn/Set/Root/Start/End/Mode/Level/Pan/Speed`
  (Set: Float 0..31, non-automatable, def 2 = "CH_01" alphabetical; Mode order must match
  `SamplePlayer::Mode`). Append `samplerStretch` at the END. IDs in `Parameters::ID`;
  registration `Source/Modules/AllModules.h` (`sampler()` last — keep).
- Editor body: hand-built in `Source/UI/PluginEditor.cpp` (`// SAMPLER (Story 12.1)` block):
  SET combo with `indexIsValue = true` (combo-index bug class — keep for anything list-backed),
  LOAD `FileAction` (copy-on-load + AlertWindow rejection), knobs. `FileAction`/`Combo` vocabulary:
  `Source/UI/rack/ModuleDescriptor.h`, handling in `Source/UI/rack/ModuleFrame.cpp`
  (`fileChooserActive` re-entrancy guard, `refreshCombo`, `indexIsValue` resync on preset load).
- Persistence: `Source/Audio/PresetIO.h` — `samplesFolder()` = `%AppData%\JASS\Samples`,
  `seedSamples()` (embedded `Samples::` binary data, idempotent), `preloadSamples()`
  (alphabetical sort ⇒ stable indices), `toVar`/`applyVar` `Sampler.File` name round-trip
  (silent on failure — keep that asymmetry: alerts only from the interactive LOAD path).
- Mod matrix: `Source/DSP/ModMatrixCatalog.h` SAMPLER entry (last, append-only) +
  `Source/DSP/ModTargets.h` X-macro (`SamplerLevel`, `SamplerPan` at end). New targets, if any,
  append to BOTH ends + bump the entry's `numParams`.
- Build glue: `CMakeLists.txt` `juce_add_binary_data(JASS_Samples ...)` from `Samples/*.wav`;
  new `.cpp` files need `target_sources` + CMake regenerate; header-only needs nothing.

### Guardrails (violations caused real defects before)

- **`SamplePlayer` grows fields and is embedded BY VALUE in `SynthVoice` ⇒ struct sizes change ⇒
  `/t:Rebuild` (clean rebuild) MANDATORY** — incremental builds after header struct-size changes
  caused the 0xC0000005 startup heap corruption (2026-07-24 lesson).
- Audio thread: no allocation/locks/logging in `processBlock`/`nextSample` (Epic 11). All stretch
  and zone buffers allocated in `prepareToPlay` (called repeatedly — re-entrant safe) or on the
  message thread inside the store.
- Combo choice order == enum order (ComboBoxAttachment maps by index); list-backed combos use
  `indexIsValue`.
- Never change existing param defaults for a new scenario (missing⇒default retro-modulates old
  presets) — `samplerStretch` default OFF is the pattern.
- Seeding files into `Samples/` alphabetically below index 2 shifts the `samplerSet` default's
  meaning ("CH_01") — if example multisample content is added, name it to sort AFTER existing
  entries, or adjust the documented default.
- No context menus / right-click UI ([[feedback_no_context_menus]]); visible controls only.
- Deliver on `develop`; never push/merge — author's call ([[feedback_git_workflow]]). No
  Claude attribution in commits/PRs.

### Previous story intelligence (12.1)

- Hermite chosen by measurement (38.3 dB vs linear 28.5 dB at +7 st) — reuse `read()` for grains.
- "Brass played CH_01" = combo-index bug class; `indexIsValue` is the cure.
- Keyboard labels MIDI 60 as C4 (`setOctaveForMiddleC(4)`) — keep every note-name surface on that
  convention (AC 2).
- Loop-click designed out via 256-sample equal-gain crossfade; stretch mode must not reintroduce
  joins/clicks (grain windows or stretcher continuity cover it — verify by ear + capture).
- Module default-VISIBLE was a user decision overriding the draft AC — don't flip it back.

### Project Structure Notes

- New DSP stays header-only in `Source/DSP/` (e.g. `SfzImport.h`, stretch engine header) unless a
  `.cpp` is unavoidable (then CMake `target_sources` + regenerate). Vendored third-party header →
  a clearly-named subfolder (e.g. `Source/ThirdParty/signalsmith-stretch/`) + README attribution.
- `_bmad-output/project-context.md` is PARTIALLY STALE (kFormatVersion is 6 not 1, AppData is
  `%AppData%\JASS` + `.jass` since the Epic-8 migration, output stage is the 5-mode STEREO module,
  commit language drifted to English) — trust `docs/ARCHITECTURE.md` / `docs/MODULE_SYSTEM.md`
  and the code where they disagree (noted in MODULE_SYSTEM.md §11).

### Testing standards

No unit-test rig. Verification = clean build + running Standalone app + user listening
([[feedback_ui_verification]]), plus scratch-harness measurements for DSP decisions (pure
cmath/own FFT, no numpy — [[feedback_measure_dont_guess_dsp]]). AC 14 lists the concrete checks.

### References

- [Source: _bmad-output/implementation-artifacts/12-1-sampler-module.md] — scope decision,
  multisampling analysis, memory-wall analysis, Dev Agent Record (all load-bearing here)
- [Source: docs/ARCHITECTURE.md#3, #4.2, #5] — RT rules, per-voice rendering, modulation
- [Source: docs/MODULE_SYSTEM.md#7, #8, #10] — matrix integration, persistence contract, recipes
- [Source: Source/DSP/SampleBank.h, Source/DSP/SamplePlayer.h] — read fully before editing
- SFZ subset: sfzformat.com opcodes `sample`/`key`/`lokey`/`hikey`/`pitch_keycenter`
- signalsmith-stretch: github.com/Signalsmith-Audio/signalsmith-stretch (MIT, header-only)

## Dev Agent Record

### Agent Model Used

### Debug Log References

### Completion Notes List

### File List
