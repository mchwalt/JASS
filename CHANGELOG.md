# Changelog

All notable changes to JASS are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
JASS uses **CalVer** versioning: `YYYY.MM.MICRO` (e.g. `2026.07.0`), where `MICRO`
increments for additional releases within the same month. This is the app/release
version and is independent of the preset **`FormatVersion`** (an integer schema
contract — currently `6`; see [`docs/JASS_Preset_Format.md`](docs/JASS_Preset_Format.md)).

## [Unreleased]

### Added
- **MOD MATRIX destination = MODULE → PARAM** — the DEST is now chosen in two steps
  (a **MOD** combo, then a **PARAM** combo whose items follow the picked module). Both lists
  are sorted A→Z. PARAM labels match the target module's own knobs (FREQ, CUTOFF, DRIVE, …),
  removing the old abstract names (e.g. "Pitch" → OSC · **FREQ**).
- **Per-oscillator modulation** — FREQ / AMP / DETUNE / FB / VOICES can target a SINGLE
  oscillator (OSC 1/2/3) instead of all at once; "Alle OSC" keeps the classic global
  behaviour. A per-OSC routing auto-enables just that oscillator and lights only its ring.
- **Full per-module target coverage** — essentially every continuous knob is now a matrix
  destination: WAVETABLE FREQ/AMP/VOICES/DETUNE, FILTER RESO, FORMANT RESO/MIX, WAVEFOLD
  SYM/MIX, DISTORTION MIX, BITCRUSH BITS/RATE, CHORUS RATE/MIX, DELAY FB, REVERB ROOM/DAMP,
  OSC FB/VOICES — each with its own live ring.
- **MOD MATRIX grown to 8 routing slots** (was 6), full-width layout.
- **PHASER added as a matrix destination** (RATE · DEPTH · FB · MIX) — the last effect
  module that was missing from full coverage; auto-enables + rings like the others.
- **FREQ modulation clamped** to ±4 octaves (OSC/Alle-OSC/WAVETABLE) so stacked slots
  can't drive the pitch into absurd, aliased territory.
- Preset **FormatVersion 6** with automatic migration (`.jass` files): the legacy single
  "Target" per slot converts to MODULE + PARAM (global Pitch/Amp/Detune → "Alle OSC"), and
  the A→Z reorder remaps the persisted PARAM index. Older presets are backed up before upgrade.
- **Modulation-matrix target expansion** — 8 new per-voice destinations: Delay Time,
  Delay Mix, Reverb Mix, Chorus Depth, Dist Drive, Bitcrush, Sub Level, Detune. Each
  shows a live modulation ring and (except Detune) auto-enables its module when routed.
- **Single-source target catalog** (`Source/DSP/ModTargets.h`, X-macro) — the enum,
  persist strings, DEST labels and enable-map are generated from one table.
- **Demo preset "FX Motion"** — 4 LFOs breathing delay/reverb/chorus/detune.

### Changed
- **DELAY** and **LFO** control order aligned to the module-wide convention
  (selector combos first, then knobs): DELAY = SYNC·TIME·FB·MIX, LFO = WAVE·SYNC·RATE·DEPTH.
- Zone help (EN+DE) explains the Modulation-vs-Processing distinction (audio vs. control).

- **CI release pipeline** (`.github/workflows/release.yml`): on every merge to `main`,
  derive the next CalVer, build **Windows + Linux** artifacts (Standalone + VST3), and
  publish them to a GitHub release. Prominent **Download** link in the README →
  `releases/latest`.
- **Versioned no-direct-push hook** (`.githooks/pre-push`) with `git config core.hooksPath .githooks`.

## [2026.07.0] – 2026-07-20

First versioned release. Summarises the project's notable state up to this point.

### Added
- **19″ rack UI** — every module (sources, modulation, processing, visualization,
  keyboard) rendered from declarative descriptors in zones, with per-module and
  per-zone **enable / reset / info** (context help, EN/DE).
- **Rack customization** — show/hide and reorder modules by drag & drop; the layout
  is persisted in the preset and resettable to the factory arrangement.
- **Modulation matrix** — 6 slots, free source→target routing with amount; routing
  auto-enables the source and the target module.
- **4 LFOs** with tempo sync; **cross-mod** (additive / ring / FM between selectable
  sources); **self-FM** feedback per oscillator; **pitch envelope**; **poly glide**;
  **arpeggiator**.
- **Effects & processing** — biquad filter, formant filter, distortion, wavefolder,
  bitcrusher, compressor, phaser/flanger, delay (tempo-synced), chorus, reverb,
  pseudo-stereo master stage.
- **Wavetable oscillator** — 6 built-in banks plus example WAVs (embedded, seeded on
  first run), position morph, WAV import.
- **3D spinning JASS logo** in the header (toggle via right-click).
- **App version (CalVer)** shown in the header subtitle and in the right-click title
  info menu (alongside the loaded preset's format version).
- **CHANGELOG.md** (this file).

### Changed
- **Module-spec architecture** — "one module = one place": each module declares its
  parameters and rack layout in `Source/Modules/<Name>Specs.h`; APVTS params and rack
  descriptors are generated from those specs.
- **Preset format** moved to a **nested, per-module JSON** layout (`FormatVersion 4`),
  extension `.jass`, under `%AppData%\JASS`. A one-time migration copies an existing
  legacy `%AppData%\Synthy` folder on first run.
- **RANDOM** now includes the modulation matrix (unique targets, tamed frequency/filter
  ranges) and leaves the whole Master Bus untouched.

### Fixed
- Preset migration bugs from the format rework (a migrated modulation slot left muted;
  a cross-mod auto-enable turning a patch into noise).
- Preset **migration is now dependable**: loading an older-format preset via the LOAD
  dialog backs up the original to `PresetsBackup/`, upgrades it in place, and a corrupt
  or non-JASS file now **fails with a visible message** instead of silently resetting to
  defaults.
- Various UI/audio fixes: hung notes on octave switch and with the envelope off,
  proportional mouse-wheel steps, oscilloscope time-base range, formant defaults.

## Releasing

Work reaches `main` **only via pull requests** — no direct pushes (enforced locally by
`.githooks/pre-push`). **Each PR merged to `main` bumps the CalVer**, and the CI pipeline
(`.github/workflows/release.yml`) does the rest **automatically**:

1. Move any `Unreleased` notes into a new `## [YYYY.MM.MICRO] – YYYY-MM-DD` section as part
   of the PR.
2. On merge, the pipeline derives `YYYY.MM.MICRO` (current year/month; `MICRO` = existing
   `vYYYY.MM.*` tag count), builds Windows + Linux artifacts, and creates the tag +
   GitHub release with them attached. No manual tagging needed.

Manual fallback (if the pipeline is unavailable), from `main` after merge:
```powershell
git tag vYYYY.MM.MICRO
git push origin vYYYY.MM.MICRO
gh release create vYYYY.MM.MICRO --title "vYYYY.MM.MICRO" --notes-from-tag
```
The repository is **private**, so releases are visible only to collaborators.
