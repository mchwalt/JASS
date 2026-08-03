# Story 12.2: SAMPLER multisampling

Status: ready-for-dev

<!-- Follow-up to Story 12.1 (see its "Original discussion notes" — the analysis there is this
     story's blueprint). Scope set by user 2026-07-30: multisample import (folder naming
     convention / .sfz). Pitch/time DECOUPLING (timestretch) was split out into Story 12.3
     (user decision 2026-08-03) — this story keeps the 12.1 tape-style pitch model untouched. -->

## Story

As a sound designer,
I want the SAMPLER to spread a folder of pitched recordings across the keyboard automatically,
so that multisampled instruments stay usable over more than ~1 octave (no chipmunk/molasses)
instead of one sample being stretched across the whole keyboard.

## Scope: import, never author

The 12.1 analysis stands: only AUTHORING a key mapping needs an editor; DERIVING or IMPORTING one
needs a single file action. JASS consumes mappings, it never edits them.

## Acceptance criteria

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
8. Help texts `Resources/EN/sampler.md` + `Resources/DE/sampler.md` updated: mapping conventions
   (filename pattern, sfz subset, C4=60), caps, ROOT-inert rule. README feature bullet; CHANGELOG
   Unreleased entry; `docs/MODULE_SYSTEM.md`/`docs/ARCHITECTURE.md` sampler passages extended.
9. Module footprint stays small (W12H1 if at all possible — large modules trigger the global
   auto-fit downscale). Budget: one LOAD FOLDER action; no zone display, no editor.
10. Verified by build + running app ([[feedback_ui_verification]]): old preset regression
    (Sampler Demo F7 unchanged), folder import plays correct pitches across zone boundaries,
    sfz import of a hand-written 3-region file.

## Tasks / Subtasks

- [ ] Task 1: SampleSet zones + store (AC 1, 4)
  - [ ] Zone struct + multi-zone `SampleSet` (single-sample = one zone), caps + worst-case comment
  - [ ] `loadFolder(dir)` / `loadSfz(file)` on the store (message thread, whole-set reject on cap)
- [ ] Task 2: Mapping import (AC 2, 3)
  - [ ] Filename note parser (names C4=60 + MIDI numbers), halfway-split range derivation
  - [ ] Minimal sfz parser (group inheritance, 5 opcodes, relative paths, ignore rest)
- [ ] Task 3: Voice + player (AC 6)
  - [ ] `trigger(transposeRatio, midiNote)` + zone lookup; per-zone root/rate; ROOT-inert rule
- [ ] Task 4: Editor + persistence (AC 5, 9) — LOAD FOLDER FileAction (needs a `pickDirectory`
      flag on `FileAction` in `ModuleDescriptor.h` + `ModuleFrame.cpp` handling), sfz in LOAD
      wildcard, copy-on-load for sets, PresetIO re-resolve + preload of set folders
- [ ] Task 5: Docs + help + CHANGELOG (AC 8)
- [ ] Task 6: Verification pass (AC 10) — **clean rebuild mandatory**, see guardrails

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
  This story only touches source/zone selection + `trigger` — the rate/step model stays 12.1
  tape-style (decoupling = Story 12.3).
- Master loop clock: `PluginProcessor::samplerMasterFrac`, advanced once per block in
  `processBlock` ("Story 12.1: shared SAMPLER loop clock" block), delivered via
  `Parameters::applyToVoice(..., samplerMasterFrac, ...)` → `setLoopSyncPhase`.
- Voice: `Source/Audio/SynthVoice.h/.cpp` — member `SamplePlayer sampler;` (BY VALUE);
  `trigger` on note-on, `baseSamplerLevel` capture/restore per block, `LFOTarget::SamplerLevel`/
  `SamplerPan` per-sample mod; render: mono host ⇒ `0.5*(l+r)` into `PanSamplerL`, stereo set ⇒
  two `addPanned` into `PanSamplerL`/`PanSamplerR` (each through binaural/HRTF/equal-power).
- `Source/DSP/ChannelStrip.h` — `kNumPanGenerators = 9`, `PanGen{...,PanSamplerL,PanSamplerR}`.
  **Unchanged by this story** (zones add no pan slots).
- `Source/Modules/SamplerSpecs.h` — params `samplerOn/Set/Root/Start/End/Mode/Level/Pan/Speed`
  (Set: Float 0..31, non-automatable, def 2 = "CH_01" alphabetical; Mode order must match
  `SamplePlayer::Mode`). New params, if any, append at the END. IDs in `Parameters::ID`;
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

- **`SamplePlayer`/`SampleSet` grow fields and the player is embedded BY VALUE in `SynthVoice` ⇒
  struct sizes change ⇒ `/t:Rebuild` (clean rebuild) MANDATORY** — incremental builds after
  header struct-size changes caused the 0xC0000005 startup heap corruption (2026-07-24 lesson).
- Audio thread: no allocation/locks/logging in `processBlock`/`nextSample` (Epic 11). All zone
  buffers allocated on the message thread inside the store; the voice only reads published sets.
- Combo choice order == enum order (ComboBoxAttachment maps by index); list-backed combos use
  `indexIsValue`.
- Never change existing param defaults for a new scenario (missing⇒default retro-modulates old
  presets).
- Seeding files into `Samples/` alphabetically below index 2 shifts the `samplerSet` default's
  meaning ("CH_01") — if example multisample content is added, name it to sort AFTER existing
  entries, or adjust the documented default.
- No context menus / right-click UI ([[feedback_no_context_menus]]); visible controls only.
- Deliver on `develop`; never push/merge — author's call ([[feedback_git_workflow]]). No
  Claude attribution in commits/PRs.

### Previous story intelligence (12.1)

- Hermite chosen by measurement (38.3 dB vs linear 28.5 dB at +7 st) — `read()` is the only
  interpolator; zones reuse it unchanged.
- "Brass played CH_01" = combo-index bug class; `indexIsValue` is the cure.
- Keyboard labels MIDI 60 as C4 (`setOctaveForMiddleC(4)`) — keep every note-name surface on that
  convention (AC 2).
- Loop-click designed out via 256-sample equal-gain crossfade — zone playback must not bypass it.
- Module default-VISIBLE was a user decision overriding the draft AC — don't flip it back.

### Project Structure Notes

- New DSP stays header-only in `Source/DSP/` (e.g. `SfzImport.h`) unless a `.cpp` is unavoidable
  (then CMake `target_sources` + regenerate).
- `_bmad-output/project-context.md` is PARTIALLY STALE (kFormatVersion is 6 not 1, AppData is
  `%AppData%\JASS` + `.jass` since the Epic-8 migration, output stage is the 5-mode STEREO module,
  commit language drifted to English) — trust `docs/ARCHITECTURE.md` / `docs/MODULE_SYSTEM.md`
  and the code where they disagree (noted in MODULE_SYSTEM.md §11).

### Testing standards

No unit-test rig. Verification = clean build + running Standalone app + user listening
([[feedback_ui_verification]]); mapping correctness is checked across zone boundaries by ear and
against a hand-written `.sfz` (AC 10). DSP decisions, if any arise, are measured, not guessed
([[feedback_measure_dont_guess_dsp]]).

### References

- [Source: _bmad-output/implementation-artifacts/12-1-sampler-module.md] — scope decision,
  multisampling analysis, memory-wall analysis, Dev Agent Record (all load-bearing here)
- [Source: _bmad-output/implementation-artifacts/12-3-pitch-time-decoupling.md] — the split-out
  timestretch story; composes with this one (a zone can be stretched) but neither blocks the other
- [Source: docs/ARCHITECTURE.md#3, #4.2, #5] — RT rules, per-voice rendering, modulation
- [Source: docs/MODULE_SYSTEM.md#7, #8, #10] — matrix integration, persistence contract, recipes
- [Source: Source/DSP/SampleBank.h, Source/DSP/SamplePlayer.h] — read fully before editing
- SFZ subset: sfzformat.com opcodes `sample`/`key`/`lokey`/`hikey`/`pitch_keycenter`

## Dev Agent Record

### Agent Model Used

### Debug Log References

### Completion Notes List

### File List
