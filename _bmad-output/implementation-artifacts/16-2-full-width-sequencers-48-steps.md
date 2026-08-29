# Story 16.2: Full-width sequencers — 48 steps, collapsible ADSR curve

Status: BUILT 2026-08-31 (83e2110 fold + 9d4634f/add10b9 STEP SEQ + da93a46 PERC) — eye-test
in progress with the maintainer. Deviations from the design, all maintainer-decided at the
screen: STEP SEQ is W28 (not W30 — the spacer cells read as wasted air; 27 cells/row, knob
size unchanged, spacing a hair tighter); PERC is W24 (boxes pack denser than knobs; the
lane-name column gave its spare air to the boxes; pairs with ARP into one full row);
`ModuleSpec::bodyOrder` was born here (display order decoupled from append-only
registration). Remaining: flip the ADSR fold default once the zone layout settles (task 4),
Los-Niños-×2 test (AC3).

## Story

As a player writing longer figures,
I want STEP SEQ and PERC to use the rack's full width with 48 steps each, and the ADSR
curve to collapse behind a button,
so that three bars of sixteenths (or two Los Niños cycles) fit one pattern and no rack
column is wasted on air.

## Design (maintainer, 2026-08-30)

- **STEP SEQ: W20U7 → W30U7, 32 → 48 steps.** The rack is 30 columns; at full width the
  two knob rows carry 24 step knobs each = **48** (the maintainer's "49" resolved to 48 —
  it is the number the grid gives for free, and musically 3 bars of 16ths / 2 × 24-step
  Los Niños cycles). Knob size unchanged (cell width is width/cols).
- **PERC: W20U7 → W30U7, 32 → 48 steps** ("genauso das PERC Modul"). The painted grid
  keeps today's cell density (48 across 30 cols ≈ 32 across 20).
- **ADSR: the curve collapses behind a header latch** (maintainer: "auf Knopfdruck würde
  die Hüllkurve dann wieder angezeigt und andere Module entsprechend verschoben"):
  - Collapsed (default): the module is one knob row, no/half curve — it fits a FLAT row
    with the other small modulators.
  - Expanded: the full curve returns, the module grows, **the rack re-packs and the rows
    below shift down** — live reflow, same machinery a MODULES show/hide already triggers.
  - View state like the GATE latch: never persisted into presets; an app setting MAY
    remember it across restarts (nice-to-have, not required).
- **Displaced modules** (ADSR, PITCH ENV, GLIDE from STEP SEQ's row; ARP, CHAOS from
  PERC's row) are NOT hand-placed: the rack packs them in registration order. Task 4
  balances the order so the Modulation zone gains at most one flat (H1) row.

## The append-only contract (the dangerous part)

- New parameters `seqPitch33..48`, `seqStep33..48`, `seqAcc33..48`, `seqSGate33..48` and
  `percStep{1..4}_{33..48}` are **appended at the END of their spec's param list**, after
  every existing param — the QUANT precedent (ModMatrixSpecs 2026-08): existing APVTS
  order stays untouched.
- `JASS_INDEXED_ID` caps 32 → 48; `StepSequencer::kMaxSteps` / `PercSequencer::kMaxSteps`
  32 → 48. Both structs are embedded by value in voice/processor state ⇒ **/t:Rebuild
  mandatory** (the 0xC0000005 class).
- `seqLength` / `percLength` ranges 1..32 → 1..48. Preset files store RAW values ⇒ old
  presets load bit-identically. KNOWN trade-off: VST3 DAW automation stores normalized
  values, so a range change remaps old DAW state — accepted (VST3 is experimental,
  story 3.4 is the open round-trip item; standalone is the product).
- Preset format stays **v10**: the `Steps` array simply grows to 48 entries, `Perc.Lanes`
  step strings to 48 chars. Older files (≤32) load with steps 33..48 at defaults (off) —
  the missing⇒default rule, nothing new. No version bump: no value changed meaning.

## Touch list

1. `Parameters.h`: INDEXED_ID caps, appended param registration (order!), LEN ranges.
2. `StepSequencer.h` / `PercSequencer.h`: kMaxSteps 48 (arrays grow), LEN clamps.
3. `PluginProcessor`: param→config copy loops read to kMaxSteps (grep for literal 32s).
4. Specs: StepSeqSpecs/PercSpecs sizes W30U7; step loops to 48 **with the appended-at-end
   registration order from (1)**; AdsrSpecs collapsible display + header latch; rack
   reflow on toggle; balance Modulation-zone packing order.
5. `PresetIO.h`: the v7 Steps pass loops 1..48 (write) / jmin(48) (read); Perc lane pass
   already counts via hasProperty — verify only.
6. `SeqMidiIO.h`: every 32 literal → kMaxSteps (cycle search 2..48, non-loop fallback 48,
   import clamp, export LEN clamp).
7. Editor: STEP SEQ injection loops (audition, note names, cursor, write mode after LEN),
   PercGrid (uses kMaxSteps — verify), doublePercPattern cap (uses kMaxSteps — verify).
8. Help EN/DE (stepseq: 48; perc: 48; adsr: the curve button), CHANGELOG.
9. Build + maintainer's eye/ear; state plainly what is only build-verified.

## Acceptance Criteria

1. STEP SEQ and PERC span the full rack width; STEP SEQ shows 24+24 step knobs, PERC 48
   grid columns; knob/cell sizes unchanged vs today.
2. Every existing preset (incl. all demo presets) loads bit-identical: steps 33..48 off,
   LEN as saved, figures sound exactly as before.
3. Los Niños ×2 fits: a 48-step figure plays, saves, reloads, MIDI-exports and reimports.
4. ADSR: collapsed by default at reduced height; the header button expands it, the rows
   below shift; collapsing again restores the compact rack. No preset interaction.
5. GATE row (15.7), accent switches (15.2), COPY/x2 (PERC) and MIDI import/export (15.8)
   all work across steps 33..48.

## References

- [Source: 15-7 / 15-2 story files] — the step-knob row mechanics being widened
- [Source: ModMatrixSpecs.h QUANT comment] — the append-at-spec-end precedent
- [Source: memory project_jass_session_2026_08_29] — append-only lessons (migration bug)
