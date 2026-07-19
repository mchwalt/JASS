# Story 9.1: Prepare the repository for GitHub (private, not yet published)

Status: ready-for-dev

<!-- Note: Validation is optional. Run validate-create-story for quality check before dev-story. -->

## Story

As the maintainer of JASS,
I want a polished README plus the essential repository files (license, .gitignore/.gitattributes, screenshots, build/install docs) and the project pushed to a **private** GitHub repo,
so that the project is clean, self-explanatory and safely version-controlled off-machine — **without making it public yet**.

## Acceptance Criteria

1. **README.md is rewritten for the current state of the project.** The stale framing must go: JASS is now the **C++/JUCE** product; the C# app is **frozen**; C# compatibility was **dropped** (preset extension is now `.jass`, app-data folder is `%AppData%\JASS`, one-time auto-migration from the old `Synthy` folder). It documents: what JASS is, key features (19″ rack UI with zones + per-module & per-zone enable/reset/info, modulation matrix, LFOs, cross-mod, effects, wavetables with example bank, 3D header), and a **build + run guide** (clone, `git submodule update --init --recursive` for JUCE, CMake configure, MSBuild `JASS_Standalone`, where the `.exe` lands). At least one **screenshot** is embedded.
2. **A LICENSE file exists and is JUCE-compatible.** Because JASS links JUCE as a submodule, the license choice must respect JUCE's terms (JUCE is **GPLv3 or a paid JUCE licence**). Recommended default: **GPLv3** (matches free JUCE use). The README states the license and notes the JUCE licensing implication. If the maintainer wants a non-GPL/commercial route, that decision is recorded instead of a wrong license being committed.
3. **`.gitignore` reviewed and `.gitattributes` added.** `.gitignore` already covers `build/`, `.vs/`, `*.user`, `_bmad` junction, `.claude/skills/`, `settings.local.json` — verify nothing sensitive/large is tracked (e.g. no `%AppData%` data, no build output). Add a **`.gitattributes`** that normalises line endings (`* text=auto`) — this removes the constant "LF will be replaced by CRLF" warnings — and marks binaries (`*.wav`, `*.jass` presets) as `binary` so Git doesn't mangle them.
4. **Screenshots captured.** One or more current screenshots of the running Standalone (full rack, and optionally the MODULES panel / mod-matrix) stored under a repo path (e.g. `docs/screenshots/`) and referenced from the README.
5. **The GitHub repo is created PRIVATE and the current `main` is pushed** with the JUCE submodule reference intact (submodule pointer only — JUCE's own files are NOT copied into the repo). **No public visibility, no release, no tags/publishing.** `gh repo create` uses `--private`.
6. **Fresh-clone sanity (best effort):** the README build steps are accurate enough that a fresh `git clone --recurse-submodules` + documented CMake/MSBuild steps produce `JASS.exe`. (Full clean-clone build verification is a stretch goal; at minimum the steps are reviewed against the real build commands used in this project.)

## Tasks / Subtasks

- [ ] Rewrite `README.md` (AC: #1)
  - [ ] Remove the "twice-built / C# + C++ / .synthy / %AppData%\Synthy / class-names-stay" framing; reframe around C++ JASS (C# frozen, C# break, `.jass`, `%AppData%\JASS`, auto-migration)
  - [ ] Feature overview (rack/zones, enable/reset/info, mod-matrix, LFOs, cross-mod, effects, wavetables + examples, 3D header, presets)
  - [ ] Build & run guide: submodule init, CMake configure path, MSBuild `build/JASS_Standalone.vcxproj` (or `cmake --build`), output path `build/JASS_artefacts/Release/Standalone/JASS.exe`
  - [ ] Embed screenshot(s)
- [ ] Add `LICENSE` (AC: #2)
  - [ ] Confirm license choice with maintainer (default **GPLv3** for free-JUCE use); note JUCE licensing in README
- [ ] Repo hygiene (AC: #3)
  - [ ] Review `.gitignore` (nothing large/secret tracked)
  - [ ] Add `.gitattributes` (`* text=auto`, `*.wav binary`, `*.jass binary`)
- [ ] Screenshots (AC: #4)
  - [ ] Capture running Standalone; store under `docs/screenshots/`; link from README
- [ ] Create + push PRIVATE repo (AC: #5)
  - [ ] `gh repo create <owner>/JASS --private --source . --remote origin` (do NOT `--public`)
  - [ ] `git push -u origin main`; confirm `.gitmodules` / JUCE pointer pushed, JUCE files NOT bloating the repo
  - [ ] Verify on GitHub the repo is **Private** (no release, no pages)
- [ ] Fresh-clone review (AC: #6)
  - [ ] Re-read README build steps against the actual commands; adjust wording

## Dev Notes

- **JUCE licensing is the key gotcha.** JUCE (submodule `JUCE/`, currently 8.0.14) is dual-licensed **GPLv3 / commercial**. A public repo distributing a JUCE app must comply (GPLv3, or hold a paid JUCE licence). Since the repo starts **private**, distribution isn't triggered yet — but pick a LICENSE that will still be correct if it later goes public. Default recommendation: **GPLv3**. Flag this to the maintainer rather than guessing.
- **JUCE is a git submodule** (`.gitmodules`). The push carries only the submodule commit pointer; JUCE's ~thousands of files must NOT be added into the JASS repo. A fresh clone needs `--recurse-submodules` (or `git submodule update --init --recursive`).
- **What must stay out of the repo:** `build/` (already ignored), `.vs/`, `*.user`, the `_bmad` junction (global BMAD core) and `.claude/skills/` (both ignored), `settings.local.json`. `_bmad-output/` **is** tracked on purpose (BMAD planning/impl artifacts incl. this story). The assistant memory folder lives under `~/.claude/projects/.../memory/` — **outside** the repo, so no risk.
- **README is currently wrong for today's state.** It still says both apps share `.synthy` in `%AppData%\Synthy` and that class names stay Synthy*. After this session: C# is frozen, C# compat dropped, presets are `.jass` under `%AppData%\JASS` (auto-migrated once), demo presets + example wavetables ship embedded and seed on first run.
- **CRLF warnings** on every commit come from missing `.gitattributes`. `* text=auto` fixes it; mark `*.wav` and `*.jass` `binary` so they're never line-ending–converted.
- **Build commands (verified this project):** CMake generator is "Visual Studio 17 2022"; cmake.exe is not on PATH (`C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`). Build target `JASS_Standalone`; output `build/JASS_artefacts/Release/Standalone/JASS.exe`. See [[feedback_build]].
- **Scope guard:** this story PREPARES and pushes to a **private** repo only. No public release, no CI, no versioned release artifacts — those would be follow-up stories.

### Project Structure Notes

- New/edited files expected at repo root: `README.md` (edit), `LICENSE` (new), `.gitattributes` (new), `.gitignore` (review), `docs/screenshots/*` (new).
- Existing docs live under `docs/`; README stays in root and links into `docs/` (keep that convention).

### References

- [Source: README.md] — current (outdated) content to rewrite
- [Source: .gitignore], [Source: .gitmodules] — repo hygiene baseline
- [Source: CMakeLists.txt] — build targets, embedded resources (Help/DemoPresets/Wavetables)
- [Source: docs/] — existing documentation set to link from README

## Dev Agent Record

### Agent Model Used

### Debug Log References

### Completion Notes List

### File List
