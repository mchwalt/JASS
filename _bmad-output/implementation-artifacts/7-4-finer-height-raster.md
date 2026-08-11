# Story 7.4: One knob size, and a raster fine enough to build modules around it

Status: **done** — raised by the maintainer 2026-08-11: "MOD MATRIX ist zu hoch — das Modul kann man
ohne Probleme um ein Drittel eindampfen", and then: "wäre es mein Wunschtraum, wenn du die
Knopfgrößen vereinheitlichen könntest (Standardgröße), so wie du es schon für die Combo-Boxen
geschafft hast".

Supersedes the parked `wip/half-rack-units` attempt (`f9bb1ab`), which had the right idea —
separating grid height from content rows — and the wrong quantum.

## Story

As someone reading the rack,
I want every knob to be the same size and every module to be exactly as tall as its content needs,
so that the rack looks like one instrument instead of a collection of modules that each guessed.

## Why the raster had to change first

A module's height was `units × 114 px`, and `units` meant two things at once: how many rows the body
lays its cells over, and how tall the module is on the grid. 114 px is sized for ONE content row plus
the header — so the second row of a two-row module arrived with the header height built into it
again, and 238 px was the only thing a two-row module could be. There was no way to say "a bit more
than one row".

`kHu` is now a QUARTER unit (21 px). n quarters are `n*21 + (n-1)*10`, so 4 = 114 px and 8 = 238 px
exactly — every module that shipped stands where it stood — while 5 = 145 and 6 = 176 and 7 = 207
exist for the first time. `SizeClassSpec.heightUnits` carries the grid height, `units` keeps meaning
content rows, and `Rack::addModule` places by the former.

## The knob standard: 40 px, measured

Every rotary in the rack is `KnobSize::Standard` = 40 px, capped by its cell exactly as `kComboW` is
capped — the cell decides whether the standard FITS, never how big the knob is.

40 is not a preference. It is the widest knob every current module can host: SAMPLER packs its row
tightest and offers a 48 px cell, and `drawRotarySlider` takes 4 px per side. Anything larger would
have meant widening a module. Before this the rack drew SIX different sizes — 34, 40, 44, 45, 46 and
53 px — none of them chosen.

Module heights then follow FROM the standard instead of the other way round: a knob row is
13 caption + 40 knob + 8 + 14 value box + 4 cell padding = **79 px**, so a two-row body plus the
22 px header and 8 px padding wants 188 → seven quarter units = **207 px**.

## Acceptance Criteria

1. **One knob size everywhere.** Verified by measuring the built app: 27 modules, every rotary 40 px.
2. **No module keeps height it does not need.** MOD MATRIX, STEP SEQ, PERC and ADSR go 238 → 207 px.
3. **Nothing else moves.** Every one-row module stays at 114 px, the visualisers at 238; zones,
   x-positions and widths are unchanged (verified by dumping every frame rectangle).
4. **The captions stay.** 176 px is reachable for MOD MATRIX, but only by dropping the 32 repeated
   `SRC · MOD · PARAM · AMT`. Maintainer: "Beschriftungen werden NICHT geopfert." The size class for
   it (`W28U6`) is kept in the table, unused, in case that trade is ever wanted.
5. **The dead strip on MOD MATRIX's right edge is spent, not hoarded.** 32 cells of 54 px leave 16 px
   of a 1744 px body; those pixels now form three 5 px bands BETWEEN the four routing slots, painted
   in the dim tone the inactive slots use. The right edge keeps only the module's own 5 px padding.
6. **STEP SEQ and PERC are visible by default** (maintainer's call, made with the cost stated: a
   factory-visible module is always inside `Rack::maxHeight`, so the fit scale must carry it).

## What this revises in Story 7.5

7.5 made the knob diameter follow the cell and set `KnobSize::Small` (46 px) as the FLOOR, on the
argument that nothing in the rack should ever get smaller. That floor makes a shorter module
impossible: MOD MATRIX at 176 px has 69 px cells, and an 81 px block laid into one overflows into the
row below — caught here by measuring the cell rectangles, not by looking at the screen. The floor is
now `KnobSize::Minimum` (22 px), the point where a rotary stops being aimable.

## Dev Notes

- `Rack::kHu` 114 → 21; `SizeClassSpec` gains `heightUnits`; `addModule` places by it.
- New size classes `W28U7`, `W20U7`, `W4U7` (all 207 px). ADSR's class lives in the EDITOR's
  hand-built `add()`, not in `AdsrSpecs.h` — the spec's size field is dead for that module, which is
  why it stayed 238 px for one build.
- `ModuleFrame::groupGaps` holds the separator bands; they are derived in `resized()` and filled in
  `paint()`. The slot-dimming rectangles are unions of widget bounds, so they follow the shift on
  their own.
- Method, which is the durable part: **instrument `resized()`, dump every rectangle to a file, run
  the app, read the numbers.** Three earlier attempts at this module reasoned from the layout code
  and were wrong three times. Every claim in this story was measured on the built app.
