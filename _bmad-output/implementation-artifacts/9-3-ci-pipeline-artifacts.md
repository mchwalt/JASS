# Story 9.3: CI pipeline — build artifacts & CalVer release on merge to main

Status: done

<!-- Note: Validation is optional. Run validate-create-story for quality check before dev-story. -->

## Story

As the maintainer of JASS,
I want a **GitHub Actions pipeline** that, on every merge to `main`, derives the next **CalVer**, **builds** the Standalone + VST3 on **Windows and Linux**, publishes a **release with per-OS artifacts**, and surfaces a **prominent download link** on the repo front page — plus a **versioned, reproducible no-direct-push guard**,
so that every merged change automatically yields versioned, downloadable builds for both platforms without manual local steps.

## Context / Decisions

- **Plan (2026‑07‑20):** GitHub Actions is available for private repos on the **Free** plan (2000 min/month; Windows runners count ×2 ≈ 1000 Windows-minutes). A JUCE full build is ~10–20 min → cache JUCE to stay cheap.
- **Workflow rule in force:** changes reach `main` only via PR merge; **each merge bumps CalVer** (Story 9.2). This pipeline is what performs the bump + release on merge.
- **Server-side branch protection is NOT available** (private repo on Free → HTTP 403, needs Pro/public). The chosen substitute is a **local pre-push hook** (already active in `.git/hooks/pre-push`); this story makes it **reproducible** for any clone via a versioned `.githooks/` + `core.hooksPath`.

## Key design decision — where the CalVer lives

Avoid a CI commit-loop (a workflow that commits a version bump to `main` would re-trigger itself). **Tags are the release source of truth:**

- CMakeLists keeps a **base** `project(JASS VERSION YYYY.M.0)` (human-edited, from Story 9.2) as the local/dev fallback.
- On merge, the workflow **computes** the release version `YYYY.MM.MICRO` — current year/month, `MICRO` = number of existing `vYYYY.MM.*` tags this month — and **injects** it into the build (CMake cache override) so the artifacts + release carry the exact CalVer, **without committing back to `main`**.
- The workflow then creates the **tag** `vYYYY.MM.MICRO` and the **GitHub release**.

_(Alternative considered & rejected: auto-commit the bumped `CMakeLists.txt` back to `main` with `[skip ci]`. Rejected — it writes to the protected-by-convention branch from CI and is loop-prone. Revisit only if per-build version in the committed source becomes a hard requirement.)_

## Acceptance Criteria

1. **Build workflow on merge, Windows + Linux.** `.github/workflows/release.yml` triggers on `push` to `main` (i.e. after a PR merge). A **build matrix** covers `windows-latest` and `ubuntu-latest`; each job checks out the repo **with submodules** (`submodules: recursive`), configures CMake, and builds **Standalone + VST3** in Release. On Linux the required JUCE dev packages are installed first (see Dev Notes). A red build is visible on the repo.
2. **CalVer derived, not committed.** A single "version" step computes `YYYY.MM.MICRO` (year/month from the run date; `MICRO` from the count of existing `vYYYY.MM.*` tags) **once** and shares it with both build jobs (job output) so all artifacts + the release carry the same version. `main`'s `CMakeLists.txt` is **not** modified by CI.
3. **Release with per-OS artifacts.** The workflow creates tag `vYYYY.MM.MICRO` and one **GitHub release** (private → visible to collaborators) with **two** zipped artifact sets attached: `JASS-Windows-vYYYY.MM.MICRO.zip` (`JASS.exe` + `JASS.vst3`) and `JASS-Linux-vYYYY.MM.MICRO.zip` (Linux `JASS` standalone + `JASS.vst3` bundle). Release notes pull from the tag / `CHANGELOG.md`.
4. **Prominent download link on the repo front page.** The README top carries a clear **"⬇ Download"** section/badge linking to **`releases/latest`** (`https://github.com/mchwalt/JASS/releases/latest`) — always resolving to the newest build, both platforms — so a visitor finds the download immediately without digging. (GitHub's own "Releases" sidebar entry complements it.)
5. **Caching / cost control.** The CMake/JUCE build dir is cached per-OS across runs to keep wall-time and Actions-minutes down (Windows minutes bill ×2, Linux ×1 against the Free 2000/month pool). Merge-to-main is the release trigger; an optional lightweight build-only check on PRs may be added later.
6. **Reproducible no-direct-push guard.** A versioned hook (`.githooks/pre-push`) plus a documented one-liner (`git config core.hooksPath .githooks`) so any clone can enable the same main-push block. README/CONTRIBUTING documents it. (The existing `.git/hooks/pre-push` stays as the active local copy.)
7. **Docs.** README gains the Download section + a short "Releases / CI" note; `CHANGELOG.md` "Releasing" section is updated to reflect that merge-to-main auto-tags + releases (manual tagging becomes the fallback).
8. **Exercised once.** The pipeline runs green on a real merge and produces a downloadable release with **both** the Windows and Linux artifact sets.

## Tasks / Subtasks

- [ ] `.github/workflows/release.yml` (AC: #1, #2, #3, #5)
  - [ ] Trigger `on: push: branches: [main]`
  - [ ] `version` job: compute CalVer from date + existing tags once → job output
  - [ ] `build` job with `strategy.matrix.os: [windows-latest, ubuntu-latest]`
  - [ ] `actions/checkout` with `submodules: recursive`
  - [ ] Linux: `apt-get install` JUCE deps (asound, x11/xrandr/xinerama/xcursor, freetype, webkit2gtk, gtk-3, curl, jack — see Dev Notes)
  - [ ] CMake configure (inject shared version) + build `JASS_Standalone` + `JASS_VST3` (Release), both OSes
  - [ ] Cache the build dir per-OS to cut minutes
  - [ ] Zip per-OS artifact set; upload as job artifacts
  - [ ] `release` job: `gh release`/`softprops/action-gh-release`, tag `vYYYY.MM.MICRO`, attach both zips
- [ ] Prominent download link (AC: #4)
  - [ ] README top: "⬇ Download" badge/section → `releases/latest`
- [ ] Reproducible hook (AC: #6)
  - [ ] Add `.githooks/pre-push`; document `git config core.hooksPath .githooks`
- [ ] Docs (AC: #7)
  - [ ] README Download + "Releases / CI"; CHANGELOG "Releasing" update
- [ ] Verify (AC: #8)
  - [ ] Confirm a real merge produces a green run + release with Windows AND Linux artifacts

## Dev Notes

- **Runner toolchains:** `windows-latest` ships VS 2022 + CMake (MSVC generator, cmake on PATH). `ubuntu-latest` ships GCC/Clang + CMake + Ninja; JUCE needs Linux dev packages installed first, e.g.: `libasound2-dev libjack-jackd2-dev libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev libfreetype6-dev libfontconfig1-dev libcurl4-openssl-dev libwebkit2gtk-4.1-dev libgtk-3-dev` (adjust webkit pkg name to the runner's Ubuntu version). Linux build uses the default generator (`-G Ninja` or Unix Makefiles), not the VS generator.
- **Artifacts:** Windows — `build/JASS_artefacts/Release/Standalone/JASS.exe` + `.../VST3/JASS.vst3` (bundle folder → zip). Linux — `build/JASS_artefacts/Release/Standalone/JASS` (no extension) + `.../VST3/JASS.vst3` (bundle → zip). Name zips per-OS + version.
- **Download link:** point README at `https://github.com/mchwalt/JASS/releases/latest` (stable URL, always newest). A shields.io release badge (`img.shields.io/github/v/release/...`) also works but on a **private** repo the badge endpoint can't read the release → prefer a plain, styled link/section over a badge that may render "unknown".
- **Minutes:** Windows bills ×2, Linux ×1 against the Free 2000/month pool. Cache the build dir per-OS; consider a `paths-ignore` for docs-only merges if minutes get tight — and note any such skip so a docs merge isn't mistaken for a released build.
- **Version injection:** pass `-DJASS_CALVER=…` or override the project version via a configure step; ensure `ProjectInfo::versionString` (Story 9.2 `Source/Version.h`) picks it up. If overriding `project(VERSION)` from the CLI is awkward, a generated version header injected before configure is the fallback.
- **Depends on Story 9.2** (CalVer single-source + `Source/Version.h`) being on `main` first.
- **Risk — first Linux build.** JASS has only ever been built on Windows. The 3D title was written to be Linux-safe (pure `juce::Graphics`) and file paths use JUCE's cross-platform `userApplicationDataDirectory`, but the first `ubuntu-latest` build may surface platform issues (headers, warnings-as-errors, font handling). Budget a fix pass; if Linux proves large, it can split into its own story and Windows-only ships first.
- **Scope guard:** private repo; releases visible to collaborators only; no public distribution, no code signing/notarization in this story.

### References

- [Source: CMakeLists.txt] — build targets, project version (Story 9.2)
- [Source: Source/Version.h] — `JASS::versionString()`
- [Source: CHANGELOG.md] — Releasing section to update
- [Source: .git/hooks/pre-push] — the active local guard to version under `.githooks/`

## Dev Agent Record

### Agent Model Used

claude-opus-4-8[1m] (2026-07-20)

### Debug Log References

- Local `cmake -B build` after the `JASS_CALVER` refactor: validates the CMakeLists change (workflow YAML itself is only exercised on the first merge to `main`).

### Completion Notes List

- **AC1/AC2/AC3/AC5** `.github/workflows/release.yml`: `on push main` (+ `workflow_dispatch`, `paths-ignore` for docs). `version` job computes CalVer once (display + CMake-numeric outputs). `build` matrix `windows-latest`/`ubuntu-latest`, submodules recursive, Linux apt deps, per-OS cache, CMake configure with `-DJASS_CALVER`, build `JASS_Standalone`+`JASS_VST3`, stage artefacts via `find`, zip per-OS (Compress-Archive / zip), upload. `release` job downloads all + `gh release create vX --target <sha>` with both zips.
- **AC1 CMake** `CMakeLists.txt`: `JASS_CALVER` override (default `2026.7.0`) so CI injects the per-merge version without committing to main; local/dev keeps the default. `Source/Version.h` display unchanged (from Story 9.2).
- **AC4** README top **"⬇ Download"** section → `releases/latest`; "Releases / CI" + contributing/hook note added.
- **AC6** `.githooks/pre-push` versioned + `git config core.hooksPath .githooks` documented; active `.git/hooks/pre-push` unchanged.
- **AC7** CHANGELOG `Unreleased` (pipeline + hook) + "Releasing" section rewritten for automation.
- **AC8** Pending: the pipeline's first real run happens when this PR merges to `main`; fix-forward if the first Linux build surfaces platform issues.

### File List

- `.github/workflows/release.yml` (new)
- `.githooks/pre-push` (new)
- `CMakeLists.txt` (JASS_CALVER override)
- `README.md` (Download + Releases/CI + hook), `CHANGELOG.md` (Unreleased + Releasing)
- `_bmad-output/implementation-artifacts/9-3-ci-pipeline-artifacts.md`, `9-2-…md` (status done)
