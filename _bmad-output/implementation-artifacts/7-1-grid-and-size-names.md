# Story 7.1: Finer 24-column grid + column-based size-class names (foundation)

Status: done

<!-- Foundation for the module-size tuning (Story 7.2): the 12-column grid was too coarse to
size small modules tightly, and the T-shirt names (XXS…XL) hid the grid maths. -->

## Story

As a JASS developer,
I want the rack grid refined from 12 to 24 columns and the size classes renamed to their column footprint,
so that small modules can later be sized tightly (finer widths exist) and every size reads as explicit grid maths.

## Acceptance Criteria

1. **24-column grid.** `Rack::kDefaultCols` is 24 (was 12); one grid column ≈ 58 px. Finer granularity is now available for the size-tuning pass.
2. **Column-based names.** `SizeClass` is renamed from `XXS/XS/S/M/L/XL` to `W{cols}H{rows}` on the 24-col grid: `W2H1, W4H1, W6H1, W8H1, W8H2, W12H2`.
3. **Visually neutral.** Every module keeps its exact current width/height — the mapping doubles the old column counts (old 1→2, 2→4, 3→6, 4→8 cols; rows unchanged), so the rack looks identical to before. Verified by screenshot compare.
4. **All references updated.** The enum, the `sizeClassSpec` table, and every `SizeClass::…` call site (`buildSampleRack` + CROSS MOD) use the new names; no old names remain.
5. **UI-only, no persistence impact.** Size class is compile-time in the descriptors (not stored in `.synthy`), so the rename has zero preset/state impact. No audio/param/DSP change.

## Tasks / Subtasks

- [x] `ModuleDescriptor.h`: rename the `SizeClass` enum to `W2H1/W4H1/W6H1/W8H1/W8H2/W12H2`; update `sizeClassSpec` cols to 2/4/6/8/8/12 (units 1/1/1/1/2/2 unchanged; slotCapacity/knobSize unchanged); update the header comment to the 24-col / column-name scheme.
- [x] `Rack.h`: `kDefaultCols` 12 → 24 (+ comment).
- [x] `PluginEditor.cpp`: rename all `SizeClass::…` call sites (XXS→W2H1, XS→W4H1, S→W6H1, M→W8H1, L→W8H2, XL→W12H2).
- [x] Build + screenshot compare against the pre-refactor capture → identical.

## Dev Notes

### Why this is visually neutral
Module pixel width = `fcols·wc + (fcols-1)·gutter`, where `wc = (gridWidth - (cols-1)·gutter)/cols`. Doubling both `cols` (12→24) and each class's `fcols` cancels out (the extra gutters are absorbed by the smaller `wc`), so widths are unchanged (±1 px rounding). Body layout (`ModuleFrame::resized`) derives `nCols` from CONTENT slots, independent of the grid `cols`, so inner layouts are untouched. Confirmed: OSC still 3-per-row, all modules same as before.

### Mapping (old → new)
| old | old cols×rows | new cols×rows (24-grid) | new name |
|---|---|---|---|
| XXS | 1×1 | 2×1 | W2H1 |
| XS | 2×1 | 4×1 | W4H1 |
| S | 3×1 | 6×1 | W6H1 |
| M | 4×1 | 8×1 | W8H1 |
| L | 4×2 | 8×2 | W8H2 |
| XL | 6×2 | 12×2 | W12H2 |

### For Story 7.2 (the actual tuning, uses this foundation)
Now that widths come in 24ths, intermediate classes are possible — add them to `sizeClassSpec` + enum as ONE new case each (AD-2), e.g. `W3H1` (=1.5 old cols) for a tight 2-knob module like STEREO. Candidates flagged: STEREO (W4H1 → narrower, e.g. W3H1), and the **MASTER title truncation** (W2H1 header can't hold title + 3 icons since Epic 6 — widen slightly or compact the header). OSC stays W8H1.

### References
- [Source: Source/UI/rack/ModuleDescriptor.h] — `SizeClass` enum + `sizeClassSpec` (renamed).
- [Source: Source/UI/rack/Rack.h] — `kDefaultCols`.
- [Source: Source/UI/rack/Rack.cpp#layout] — width maths (`wc`, `pw`).
- [Source: Source/UI/PluginEditor.cpp#buildSampleRack] — call sites.
- [Constraint: ARCHITECTURE-SPINE AD-2] — size classes in one table; AD-3 rotary minimum (relevant to 7.2).

## Dev Agent Record

### Agent Model Used

claude-opus-4-8[1m]

### Completion Notes List

- Grid 12→24 (`Rack::kDefaultCols`); `SizeClass` renamed to column-footprint names `W{cols}H{rows}` (W2H1…W12H2); `sizeClassSpec` cols doubled (units/slotCapacity/knobSize unchanged); 20 call sites in `PluginEditor.cpp` + enum/table in `ModuleDescriptor.h` updated.
- Visually neutral — screenshot after refactor matches before (OSC 3-per-row, all module widths unchanged). No `.synthy`/param/DSP impact.

### File List

- `Source/UI/rack/ModuleDescriptor.h` (enum + sizeClassSpec + comment)
- `Source/UI/rack/Rack.h` (kDefaultCols 12→24 + comment)
- `Source/UI/PluginEditor.cpp` (all SizeClass:: call sites renamed)
