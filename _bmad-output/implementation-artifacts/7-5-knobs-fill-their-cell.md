# Story 7.5: The knobs fill their cell

Status: **done** — raised by the maintainer 2026-08-11 after the dead space in MOD MATRIX was
finally measured instead of reasoned about: "die Knöpfe sollen ihr Platzpotential voll ausschöpfen —
dann verschwindet auch der Leerraum zwischen den zwei Zeilen".

## Story

As someone reading the rack,
I want a knob to be as large as the space its module actually gives it,
so that a two-row module looks filled instead of showing a band of nothing between its rows.

## What was measured (and why the two earlier attempts failed)

The cause of MOD MATRIX's empty space was derived from the layout code twice and was wrong twice.
The third attempt measured it: temporary instrumentation in `ModuleFrame::resized` dumped every
cell rectangle at runtime, and the maintainer's own screenshot was decoded pixel by pixel. Both
agree:

| | measured |
|---|---|
| Module | 238 px = header 22 + body 208 (padding 4/4) |
| Grid | 2 rows × 28 cells ⇒ **cell 104 px tall, 62 px wide** |
| AMT knob: content | **81 px** (13 caption + 46 knob + 8 + 14 value box) ⇒ 26 px gap between the rows |
| SRC/MOD/PARAM: content | **35 px** (13 caption + 22 box) ⇒ 72 px gap between the rows |

Two things follow, and both had been guessed wrong before:

1. **A knob's block is a constant 81 px** — it fits a 1-row module exactly (cell 80 px) and leaves a
   2-row module's cell a fifth empty. Shrinking the module (the parked half-rack-units attempt)
   therefore treats the wrong end: it takes height away from the row that has none to give.
2. **A knob is capped by the NARROWER side of its cell.** The AMT cell is 104 px tall but only
   62 px wide, and `SynthyLookAndFeel::drawRotarySlider` reduces by 4 px per side — so even an
   unbounded knob could only have reached 50 px there. Height alone would have allowed 65.

The combo columns' 72 px are a separate matter (35 px of content in a 104 px cell) and are
deliberately NOT addressed here — the maintainer's call: "die Comboboxen tun hier nix zur Sache".

## Acceptance Criteria

1. **A knob's diameter is derived from its cell**, bounded by height (what is left after caption and
   value box), by width (the cell minus the 8 px the LookAndFeel reduces by) and clamped to
   `[KnobSize::Small, KnobSize::Large]`. Small stays the FLOOR, so no knob in the rack gets smaller.
2. **The AMT knob claims two body slots**, so its cell is wide enough for the knob to reach the
   height the row offers. MOD MATRIX keeps its 28 grid columns and its 238 px height — nothing about
   the window, the height budget or the fit scale moves.
3. **No module gets taller or shorter.** This story changes what fills a cell, never the cell count
   or the module footprint.
4. `Knob::slots` exists next to `Combo::slots` and defaults to 1, so a knob only takes extra width
   where a descriptor asks for it.

## Result (measured on the built app)

| module | knob Ø before → after | gap between the rows |
|---|---|---|
| MOD MATRIX (AMT, 2 slots) | 46 → **65 px** | 26 → **4 px** |
| STEP SEQ (32 steps + globals) | 46 → **53 px** | 26 → **16 px** |
| PERC (NOTE/AMP/PAN per track) | 46 → **53 px** | 26 → **16 px** |
| every 1-row module, ADSR | unchanged (46) | unchanged |

STEP SEQ and PERC land at 53 rather than 65 because their cells are 65 px wide — there the WIDTH is
the binding constraint, exactly as AC1 describes. Maintainer verdict on the running app: "Größe
passt jetzt".

## Dev Notes

- One layout site: `ModuleFrame::resized`. `setKnobDiameter` moved from `buildBody` (once per module,
  from the size class) to `resized` (per cell) — the LookAndFeel caps the rotary at that value.
- `sizeClassSpec(W28H2).slotCapacity` raised 56 → 64 to match the AMT's second slot. The capacity
  assert undercounts anyway (`elementSlots` returns 1 for a multi-slot Combo); that is pre-existing
  and left alone.
- AD-3 in `docs/ARCHITECTURE.md` said "one knob diameter everywhere". It now reads: one diameter per
  cell size, with the old value as the floor.
- No unit-test rig: build, run, and the maintainer's eye — plus the runtime dump above, which is the
  method this story exists to establish. **Measure the rectangles before changing the layout.**

## Follow-up, not this story

- The combo columns still hold 35 px of content in a 104 px cell (72 px between the rows). A cheap
  cosmetic nachzug would centre caption + box as one BLOCK instead of centring only the box
  (today 28 px above / 41 px below).
- Story 7.4 (finer height raster) is next and is now founded on the same measurement.
