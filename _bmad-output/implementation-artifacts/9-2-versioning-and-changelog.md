# Story 9.2: App versioning (CalVer), CHANGELOG & robust preset migration

Status: ready-for-dev

<!-- Note: Validation is optional. Run validate-create-story for quality check before dev-story. -->

## Story

As the maintainer of JASS,
I want a **single, visible app version (CalVer)**, a maintained **`CHANGELOG.md`** tied to **GitHub releases**, and **dependable preset-format versioning & migration** (with backups and the preset's format version surfaced in the UI),
so that every build is identifiable, changes are tracked, and the preset-conversion mishaps that broke patches (Helikopter/whuwhu) can't silently recur.

## Context / Motivation

The 2026‑07 nested‑format rework (`.synthy` flat → `.jass` nested v4) converted presets on load, and that conversion had **silent failures** (a migrated modulation slot stayed muted; an auto‑enable turned a patch into noise). Those bugs were caught by ear, not by the system. Two gaps caused it: there was **no app version** to pin a build to a behaviour, and preset migration was **best‑effort and quiet** (no backup surfaced to the user, no visible format version, no hard failure on a bad convert). This story closes both.

**Two independent version contracts (keep them separate):**
- **App version = CalVer** `YYYY.MM.MICRO` (e.g. `2026.07.0`). Human/release-facing; shown in the UI and on GitHub releases.
- **Preset `FormatVersion` = integer** (currently `4`). A machine contract for the preset schema; bumped only on a *value* migration. Unrelated to the app CalVer.

## Acceptance Criteria

1. **Single source of truth for the app version (CalVer).** The version `YYYY.MM.MICRO` is defined **once** (e.g. in `CMakeLists.txt` `project(JASS VERSION …)` / a `JASS_VERSION` cache var) and flows to the JUCE build (`JucePlugin_VersionString` / `ProjectInfo::versionString`) and to any UI that shows it — no hard-coded duplicate. The current `project(JASS VERSION 1.0.0)` is replaced by the CalVer value. Because CMake `project(VERSION)` requires numeric `MAJOR.MINOR.PATCH`, map CalVer onto it as `YYYY.MM.MICRO` (leading zero of the month dropped for CMake, e.g. `2026.7.0`) and keep the display string (`2026.07.0`) derived from it.
2. **The version is visible in the running app.** The CalVer string is shown in the standalone UI — e.g. under the header title/subtitle, or in an "info"/About affordance reachable from the header. It must be legible and not disturb the rack layout. As a VST3, the host also reports the same version (comes for free from the JUCE version).
3. **`CHANGELOG.md` at the repo root, Keep a Changelog style.** Sections per released version (newest first) plus an `Unreleased` section. It is **back-filled** with the notable history already in git/memory at a reasonable granularity (rack redesign, mod matrix, LFO 2–4, DSP features, `.jass` rebrand + auto-migration, repo went private) — not a full commit dump, but the user-visible milestones. The current state becomes the first CalVer entry.
4. **A release process is documented and exercised once.** A short "Releasing" section (in `CHANGELOG.md` or `docs/`) states: bump the CalVer, move `Unreleased` → the new version with a date, tag `vYYYY.MM.MICRO`, and create a GitHub release. **Create the first release/tag** for the current version on the (private) repo via `gh release create` (private repo → release is visible only to collaborators; no public exposure).
5. **Preset migration is dependable, not silent.** On loading a preset whose `FormatVersion` is older than current, JASS: (a) **backs up** the original file before rewriting/migrating it (a `PresetsBackup_v<n>/` folder or `.bak` alongside — build on the existing backup behaviour rather than inventing a second scheme); (b) migrates via the existing readers; (c) **fails loudly** (a visible message, not a silent default-reset) if the file can't be parsed at all. The specific `migrateLfoTargetsToSlots` "modMatrixOn not set" class of bug is covered by a note/guard so a migrated routing is never left muted.
6. **The preset's format version is surfaced.** The UI indicates when a loaded preset was migrated from an older format (e.g. a small hint by the preset name, or in the info/About panel) so the user knows a conversion happened. Minimum bar: the loaded preset's `FormatVersion` is readable somewhere in the UI.
7. **README + docs updated.** README notes the versioning scheme (CalVer) and links `CHANGELOG.md`. `docs/JASS_Preset_Format.md` documents the `FormatVersion` integer contract, the migration+backup behaviour, and where backups land.
8. **No behavioural regression.** Existing presets (demo `.jass` v4 + any user presets) still load byte-faithfully; the version/CHANGELOG work is additive. Verified by build + running app (no unit tests in this project — see [[feedback_ui_verification]]).

## Tasks / Subtasks

- [ ] Define CalVer single-source (AC: #1)
  - [ ] Replace `project(JASS VERSION 1.0.0)`; decide CMake numeric mapping (`YYYY.MM.MICRO`) + display string
  - [ ] Confirm JUCE picks it up (`JucePlugin_VersionString` / `ProjectInfo::versionString`); expose a `JASS::versionString()` helper if useful
- [ ] Show version in UI (AC: #2)
  - [ ] Render CalVer near header title/subtitle, or via an info/About affordance; keep rack layout intact
- [ ] Author `CHANGELOG.md` (AC: #3, #4)
  - [ ] Keep a Changelog skeleton (`Unreleased` + first CalVer entry)
  - [ ] Back-fill user-visible milestones from git/memory
  - [ ] Add a short "Releasing" checklist
- [ ] First GitHub release (AC: #4)
  - [ ] Tag `vYYYY.MM.MICRO`; `gh release create` on the private repo
- [ ] Harden preset migration (AC: #5, #6)
  - [ ] Back up original before migrating (extend existing `PresetsBackup_*` behaviour)
  - [ ] Loud failure on unparseable file (message vs silent default-reset)
  - [ ] Guard/note the `migrateLfoTargetsToSlots` muted-routing class of bug
  - [ ] Surface loaded preset's `FormatVersion` / "migrated" hint in UI
- [ ] Docs (AC: #7)
  - [ ] README: CalVer scheme + CHANGELOG link
  - [ ] `docs/JASS_Preset_Format.md`: FormatVersion contract + backup/migration behaviour
- [ ] Verify (AC: #8)
  - [ ] Build standalone; load demo + user presets; confirm no regression in running app

## Dev Notes

- **Versioning decision (2026‑07‑20):** app version = **CalVer** `YYYY.MM.MICRO`; scope = **everything in one story** (app version + CHANGELOG + releases + robust preset migration). Preset `FormatVersion` stays an integer contract, independent of CalVer.
- **CMake gotcha:** `project(VERSION)` only accepts numeric `MAJOR.MINOR.PATCH`. Use `YYYY.MM.MICRO` (e.g. `2026.7.0`); build the zero-padded *display* string (`2026.07.0`) in code. Don't hard-code the version twice — derive everything from the CMake value.
- **Existing migration surface** (`Source/Audio/PresetIO.h`): `kFormatVersion = 4`; nested reader `applyVar`; legacy flat reader `applyVarFlatLegacy`; `migrateLfoTargetsToSlots` (the one that forgot `modMatrixOn` — fixed in `cf9244e`, but the *class* of "migration leaves something inert" needs a guard/checklist); one-time legacy convert to nested. Backups already exist for the v2 batch under `%AppData%\Synthy\PresetsBackup_v2\` — reuse that pattern, don't invent a parallel one.
- **AppData layout:** presets under `%AppData%\Roaming\JASS\Presets\*.jass`, LiveState `LiveState.jass` (see `docs/JASS_Preset_Format.md`). Demo presets are **compiled into the exe** (`JASS_DemoPresets` binary data) and seeded on first run — changing a demo requires the repo file **and** a rebuild.
- **UI:** header shows the 3D "JASS" title + "Just Another Simple Synthesizer" subtitle (`SpinningTitle3D`). Version likely fits under the subtitle or behind an info icon; keep the auto-fit rack height rule in mind (don't add a tall element — see Kern-Lehren in [[project-jass-rack-redesign]]).
- **Build/verify:** incremental via `build/JASS_Standalone.vcxproj` (MSBuild/PowerShell); CMake re-runs on `CMakeLists.txt` change. No unit tests → verify in the running app. Rebuild fails LNK1104 while the app is running → close it first.
- **Scope guard:** private repo only; the first release is visible to collaborators, not the public. No public release / CI in this story.

### Project Structure Notes

- Touch points: `CMakeLists.txt` (version), `Source/UI/PluginEditor.*` (show version), `Source/Audio/PresetIO.h` + `Source/PluginProcessor.*` (migration/backup/loud-fail + surface version), new `CHANGELOG.md` (root), `README.md` + `docs/JASS_Preset_Format.md` (docs).

### References

- [Source: CMakeLists.txt] — `project(JASS VERSION 1.0.0)`, `juce_add_plugin`
- [Source: Source/Audio/PresetIO.h] — `kFormatVersion`, `applyVar`, `applyVarFlatLegacy`, `migrateLfoTargetsToSlots`
- [Source: docs/JASS_Preset_Format.md] — current format contract to extend
- [Source: _bmad-output/planning-artifacts/epics.md] — Epic 9

## Dev Agent Record

### Agent Model Used

### Debug Log References

### Completion Notes List

### File List
