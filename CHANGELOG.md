# Changelog

All notable changes to JASS are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
JASS uses **CalVer** versioning: `YYYY.MM.MICRO` (e.g. `2026.07.0`), where `MICRO`
increments for additional releases within the same month. This is the app/release
version and is independent of the preset **`FormatVersion`** (an integer schema
contract — currently `4`; see [`docs/JASS_Preset_Format.md`](docs/JASS_Preset_Format.md)).

## [Unreleased]

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

Work reaches `main` **only via pull requests** — no direct pushes. **Each PR merged to
`main` bumps the CalVer.** As part of the PR (or immediately before merging):

1. Bump the CalVer in `CMakeLists.txt` — `project(JASS VERSION YYYY.M.MICRO)` (month
   unpadded for CMake; the UI displays it zero-padded). Use the current year/month with
   `MICRO = 0`, or increment `MICRO` for a further merge in the same month.
2. Move the `Unreleased` notes into a new `## [YYYY.MM.MICRO] – YYYY-MM-DD` section.
3. After the PR is merged, tag and publish from `main`:
   ```powershell
   git tag vYYYY.MM.MICRO
   git push origin vYYYY.MM.MICRO
   gh release create vYYYY.MM.MICRO --title "vYYYY.MM.MICRO" --notes-from-tag
   ```
   The repository is **private**, so a release is visible only to collaborators.
