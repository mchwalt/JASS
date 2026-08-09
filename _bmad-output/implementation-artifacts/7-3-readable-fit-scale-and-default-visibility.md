# Story 7.3: a readable floor for the display-fit scale, and a rack that only pays for what it shows

Status: ready-for-dev — raised 2026-08-09 by the maintainer while planning Story 15.1.
**Sequenced BEFORE 15.1**: the sequencer adds rack height, and today that height is paid for by
shrinking the whole editor. Fix the rule first, then add the module.

## Story

As the person using JASS every day,
I want the editor to stay readable and to stop paying for modules I never show,
so that adding a module is a decision about rack space, not a silent cut in legibility.

## Context — the numbers

`SynthyEditor::refitHeight()` (`Source/UI/PluginEditor.cpp:573-607`) does two separate things:

- the **logical height** follows the **visible** rack (`Rack::preferredHeight`) — this part is fine;
- the **display-fit scale** is computed **once** from the **worst case** (`Rack::maxHeight`, which
  forces *every* module visible, `Rack.cpp:543-551`), clamped to `jlimit(0.5, 1.0, …)`, and applied
  as one uniform transform over the whole editor.

That rule came from PR #27 and was the right trade then: deriving the scale from the visible content
made the window grow and shrink on every preset change, because a preset reveals the modules it
enables. Stable beat big.

Two things have changed since:

1. **The scale is out of headroom.** On the maintainer's screen the factor is **0.65**, and he states
   plainly that this is the readability limit — below it nothing can be read. Back-calculated:
   1440 px screen − 90 px chrome = 1350 px available; `1350 / 0.65 = 2077`; minus the 96 px of header
   and margins ⇒ **1981 px of worst-case rack**. That is exactly where we are. The scale is therefore
   no longer a buffer that absorbs new modules — it is a **budget that is already spent**.
2. **Hiding a module buys nothing.** `maxHeight()` counts every module that exists, so the MODULES
   panel can tidy the rack but can never give back a single pixel of legibility.

Meanwhile the shipped and local presets show that a good part of the rack is asleep. All 17 presets
(8 shipped + 9 in `%AppData%`) were scanned; **these modules are enabled in none of them**:

| module | zone | size |
|---|---|---|
| SUB | GENERATORS | W4H1 |
| CROSS MOD | MODULATION | W5H1 |
| GLIDE | MODULATION | W3H1 |
| PITCH ENV | MODULATION | W3H1 |
| BITCRUSH | PROCESSING | W3H1 |
| FORMANT | PROCESSING | W3H1 |
| PHASER | PROCESSING | W6H1 |
| COMPRESSOR | MASTER BUS | W8H1 — already `defaultVisible = false` |

## Acceptance Criteria

1. **Readability floor.** The clamp becomes a named constant — `kMinFitScale = 0.65` — carrying the
   reason ("below this the rack is not readable; maintainer, 2026-08-09") instead of the bare `0.5`.
2. **The worst case counts only what may appear**: the factory-default visible set plus whatever the
   user has explicitly shown — not every module that exists. Recomputed on a deliberate layout edit
   (MODULES panel, RESET), **never** on a preset load, so the PR #27 property survives: switching
   presets must not move the window. Verify by loading F1…F8 in sequence and confirming the window
   geometry does not change.
3. **Seven sleepers default to hidden**: SUB, CROSS MOD, GLIDE, PITCH ENV, BITCRUSH, FORMANT, PHASER
   (`defaultVisible = false`, as COMPRESSOR already is). Safe by construction — they are off in every
   preset, "hidden ⇒ silent" is already an invariant (`enforceHiddenDisabled`), and
   `revealEnabledModules()` brings any of them straight back the moment a preset enables it.
   **Existing presets that carry a custom layout keep it** — this changes the *default*, not saved layouts.
4. **Measure and report the payoff.** After 3, record the new worst-case rack height and the resulting
   scale on the maintainer's screen. That number decides whether Story 15.1's module fits at all; put
   it in front of him rather than assuming.
5. **The budget is visible.** The MODULES panel shows what the current selection costs, e.g.
   `Rack 1780 / 1981 px · Anzeige 0.72`. When a selection would push the scale below `kMinFitScale`,
   say so in that line. Silent shrinking is what this story exists to end.
6. No audio, parameter or preset-format change anywhere in this story.

## Decide with the user

- **What happens when the budget is genuinely exceeded** (floor reached and the rack still too tall).
  Options: let the window exceed the screen (bad), clip (worse), or refuse to grow and let the panel
  state that something must be hidden first. Recommendation: the last one — the constraint is real,
  and making it explicit is more honest than either silently shrinking or silently clipping. Scrolling
  is off the table (maintainer, PR #27: "scrollen ist scheiße").
- **Sleeping ≠ useless.** A module no preset demonstrates is already invisible; hiding it makes that
  permanent. The other cure is a demo preset that finally shows PHASER, FORMANT and SUB off. The
  recommendation is to do both — hide now, demo later — but the second half is not this story.

## Dev Notes

- `maxHeight()`'s doc comment in `Rack.h:133-137` states the old contract verbatim; update it with the
  new one, including why preset loads must not recompute.
- The scale is cached via `if (fitScale <= 0.0)`. Any recompute path has to invalidate that cache
  explicitly — and, per AC2, only from the layout-edit path.
- Watch the interaction with `revealEnabledModules()`: it exists because a hidden module must never be
  audible. Do not weaken it to save pixels.
- Files (expected): `Source/UI/PluginEditor.cpp`, `Source/UI/rack/Rack.{h,cpp}`,
  `Source/Modules/{Sub,CrossMod,Glide,PitchEnv,Bitcrush,Formant,Phaser}Specs.h`, `CHANGELOG.md`.
- No unit-test rig: verification is build + running app + the maintainer's eye. State plainly what is
  only build-verified.
- Feature branch, **no push, no merge**.

## Dev Agent Record

_Not started — story recorded 2026-08-09._
