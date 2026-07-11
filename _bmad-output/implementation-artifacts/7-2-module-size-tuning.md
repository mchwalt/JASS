# Story 7.2: Tighten module size classes to fit control counts

Status: done

<!-- Builds on Story 7.1 (24-col grid + column-based names). The finer grid makes tight small
     sizes possible; this story assigns them, module by module, with the user's eye. -->

## Story

As a JASS player,
I want each module sized to its actual controls (no oversized / over-wide modules),
so that the rack reads tight and balanced — as densely packed as the OSC modules already are.

## Acceptance Criteria

1. **Density benchmark = OSC.** OSC is `W8H1` (8 of 24 cols), 3 per row, content packed tightly (~1.75 slots/col). Loose modules are brought closer to that density where sensible.
2. **User-driven per module.** The user specifies each module's target width; the dev applies it (change only the `SizeClass` argument, adding a new `W{cols}H{rows}` class to `sizeClassSpec`+enum if the target width doesn't exist yet — ONE new case each, AD-2).
3. **Rotary minimum (AD-3).** After shrinking, rotaries stay usable; if a target width would make knobs too small, report it and keep wider.
4. **Combo width.** Combos keep enough width to read their item text.
5. **STEREO tightened.** STEREO (`W4H1`, 2 knobs, looks loose — 2.5× MASTER fits) moves to a narrower class now that intermediate widths exist (e.g. `W6H1`→no; candidate `W5H1`/`W6H1`… decide by eye — its 2 knobs should sit tight like OSC's, not float).
6. **MASTER header fix.** MASTER (`W2H1`, ~116 px) truncates its title since Epic 6 added the info ⓘ icon (title + 3 header icons don't fit 1 old-column). Fix: widen MASTER just enough (e.g. `W3H1`) so title + ⓘ + ↺ + enable fit, OR compact the header — keep the uniform anatomy.
7. **Rules held.** "single-control = smallest class only", MASTER BUS right-aligned / other zones left, uniform header anatomy, fixed grid; OSC stays `W8H1`.
8. **UI-only.** No param/APVTS/DSP/`.synthy` change.
9. **Verified in the running app** per module: balanced look, controls usable/readable, no truncation or grid-boundary breach, height auto-fits.

## Tasks / Subtasks

- [ ] **Task 1 — Per-module targets** — collect the user's target width for each module to change (they drive this). Record the resulting class table.
- [ ] **Task 2 — Add any new intermediate classes** — for each target width not yet in `sizeClassSpec` (e.g. `W3H1`, `W5H1`), add ONE enum value + ONE table case `{cols, 1, cap, KnobSize::Small}`.
- [ ] **Task 3 — Apply + verify each** — change the `SizeClass` arg in `buildSampleRack`; build; check in the app; revert any that cram (AD-3 / combo width).
- [ ] **Task 4 — MASTER header** — resolve the title truncation (widen to `W3H1` or compact header); confirm the title reads with all three icons.
- [ ] **Task 5 — Regression** — zones aligned, height auto-fits, customization/persistence unaffected (ids unchanged).

## Dev Notes

### Current classes (post Story 7.1, on the 24-col grid)
STEREO `W4H1` · MASTER `W2H1` · OSC 1/2/3 `W8H1` · CROSS MOD `W6H1` · SUB `W6H1` · NOISE `W6H1` · STRING-KARPLUS `W8H1` · WAVETABLE `W8H1` · ENVELOPE-ADSR `W8H2` · LFO `W8H1` · ARPEGGIATOR `W8H1` · FILTER `W8H1` · DISTORTION `W8H1` · WAVEFOLD/BITCRUSH/CHORUS/DELAY/REVERB `W6H1` · OSCILLOSCOPE/SPECTRUM `W12H2`.

### Density heuristic
Content slots (combo=2, knob/button=1) ÷ grid cols. OSC ≈ 7/8 ≈ 0.9 (dense, looks good). STEREO = 2/4 = 0.5 (loose). Aim loose modules toward the OSC feel. Widths now come in 24ths, so a 2-knob module can be ~5–6 of 24 instead of a full old-column-pair.

### Guardrails / mechanics
- Body layout is content-driven (`ModuleFrame::resized` derives nCols from slots); shrinking a class narrows the module and tightens content — watch AD-3 rotary min + combo text width (verify by eye).
- Change only `SizeClass` args (+ new table cases). No param/DSP/`.synthy`; module ids unchanged → layout/persistence intact.
- Header now carries ⓘ + ↺ + enable (Epic 6) — very narrow classes must still fit the title; that's the MASTER problem (Task 4).
- Build incrementally (`build/JASS_Standalone.vcxproj`), no CMake reconfigure (no resource change). Verify in the running app ([[feedback_ui_verification]]).

### References
- [Source: _bmad-output/implementation-artifacts/7-1-grid-and-size-names.md] — the 24-col grid + naming foundation this builds on.
- [Source: Source/UI/rack/ModuleDescriptor.h] — `SizeClass` enum + `sizeClassSpec` (where new classes go).
- [Source: Source/UI/PluginEditor.cpp#buildSampleRack] — the `SizeClass` args to tune.
- [Constraint: AD-2 (size-class table), AD-3 (rotary minimum).]

## Dev Agent Record

### Agent Model Used

claude-opus-4-8[1m]

### Debug Log References

- Iterated widths live with the user in the running app (screenshots + eye). Knob "shrink" happens only when the per-slot cell width < the fixed rotary `KnobSize::Small = 46 px`; the 62 px figure is only the widget width cap, not the rotary size — so a module is wide enough as long as `moduleWidth/nContentSlots - 4 ≥ 46`.

### Completion Notes List

- **New intermediate classes added** to `sizeClassSpec`+enum: `W3H1` (3×1), `W4H2` (4×2, ADSR), `W5H1` (5×1, CROSS MOD). `W2H1` retained as the smallest base class (currently unused). A transient `W7H1` was added then removed once LFO settled at W6.
- **Final per-module classes** (user-approved by eye): STEREO/MASTER `W3H1`; OSC 1/2/3 `W8H1` (unchanged, 3-per-row density benchmark); CROSS MOD `W5H1` (5 — 4 truncated "RingMod"); SUB/NOISE `W3H1`; STRING-KARPLUS `W6H1`; ENVELOPE-ADSR `W4H2` (narrower, keeps 2-row curve); LFO & ARPEGGIATOR `W6H1` (equal width, full knobs); FILTER/DISTORTION `W4H1` (knobs verified full — rotary 46 fits the ~56 px cell); WAVEFOLD/BITCRUSH/CHORUS/DELAY/REVERB `W3H1`; OSCILLOSCOPE/SPECTRUM `W12H2`.
- **MASTER header fix:** MASTER W2H1→W3H1 so the title + info ⓘ + reset ↺ + enable fit (Epic-6 regression resolved).
- Rules held: rotaries never below `KnobSize::Small`; combos wide enough to read (`RingMod`, `Highpass`, `Soft Clip`); OSC stays W8H1; uniform header anatomy; MASTER BUS right-aligned. UI-only — no param/APVTS/DSP/`.synthy` change; module ids unchanged (layout/persistence intact).

### File List

- `Source/UI/rack/ModuleDescriptor.h` (new size classes W3H1/W4H2/W5H1 + comments)
- `Source/UI/PluginEditor.cpp` (per-module `SizeClass` assignments)
