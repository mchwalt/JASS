# Story 15.8 (D): MIDI ⇄ STEP SEQ through the LOAD/SAVE dialogs

Status: built 2026-08-30 — build-verified + algorithm simulated against los_ninos.mid; in-app ear test pending

## Story

As a player collecting figures,
I want to load a .mid transcription straight into the STEP SEQ and save my figure back out
as a .mid,
so that reference transcriptions (Los Niños, Der Mussolini, basic-pitch output) become
playable figures in seconds, and JASS figures travel to the DAW.

## Design (maintainer, 2026-08-27)

- **Entry point: the existing LOAD/SAVE dialogs** (maintainer's pick; two header buttons are
  the fallback if this feels hidden). LOAD's filter grows to `*.jass;*.mid;*.midi` — picking
  a MIDI file imports the figure instead of loading a preset. SAVE's filter grows to
  `*.jass;*.mid` — saving as .mid exports the figure. No new rack UI.
- **SMF contract** (the 15.6 market analysis, confirmed by this project's measurements):
  - Position + velocity are ground truth; the grid is **1/16** (PPQ/4 per step), anchored on
    the first note-on.
  - **Velocity ⇒ accent by clustering**, never read continuously: spread ≥ 8 splits at the
    midpoint (Los Niños: v98 vs v80/86 ⇒ the 98s are accents).
  - **Duration ⇒ gate/TIE/SLIDE**: within its step ⇒ percent; holding through following
    empty steps ⇒ synthesized held steps + TIE; **overlapping the next note-on ⇒ SLIDE**
    (the 303 convention). Ending exactly ON the next onset = plain 100 % (legato retrigger —
    DAF), NOT a tie.
  - **Cycle detection**: transcriptions loop the figure many times; the smallest period
    (2..32 steps, ≥ 2 full cycles seen) is the figure. Los Niños' 240-note file must come in
    as its 24-step cycle, not as the first 32 sixteenths.
  - Root = the figure's most frequent note (the pedal); offsets clamp ±24; the root lands in
    the LATCH (via the PresetIO hook), so an imported figure starts playing like a loaded
    sequencer preset. SYNC ⇒ 1/16, LEN ⇒ cycle, MASTER tempo ⇒ the file's first tempo event,
    STEP SEQ switched on (the ARP exclusion coupling handles the rest).
  - Import runs inside `setPresetLoading` (PR #60): couplings silent, voices killed on both
    edges.
- **Export**: 480 PPQ, 120 ticks/step, velocities exactly what the engine emits (127/100),
  durations from the step gates; TIE chains merge into one note; a pitch takeover (TIE or
  SLIDE with a new pitch) exports as the 303's **overlap** — documented asymmetry: a
  pitch-changing TIE reimports as SLIDE (MIDI cannot carry the difference).
- Errors are LOUD (message box, both languages), like the preset LOAD path: unreadable file,
  SMPTE time format, no notes.

## Acceptance Criteria

1. LOAD dialog opens `D:\downloads\los_ninos.mid` ⇒ 24 steps, two velocity classes as
   accents, gates ≈ 87/36/41, root Bb1 latched and playing, tempo 115.
2. SAVE as .mid of Los Niños re-imports to the identical figure (position/velocity/gate
   round-trip; pitch-changing ties come back as slides — documented).
3. Preset load/save through the same dialogs is untouched.
4. Docs: help EN/DE (presets page + STEP SEQ page name the .mid path), CHANGELOG.

## Tasks

- [x] Task 1: `Source/Audio/SeqMidiIO.h` (header-only): importFigure / exportFigure per the
      contract above
- [x] Task 2: PluginEditor LOAD/SAVE branches (+ bilingual result/error boxes)
- [x] Task 3: Help EN/DE, CHANGELOG
- [x] Task 4: Build + import los_ninos.mid + maintainer's ear; say what is only build-verified

## Dev Agent Record (2026-08-30)

- `Source/Audio/SeqMidiIO.h`: import quantizes onto the 1/16 grid (louder note wins a step),
  detects the cycle on positions+notes only, then FOLDS velocities/durations across cycles
  (median) — transcription wobble averages out. Durations resolve to covered-steps + last-gate
  (percent / 100-legato / SLIDE-overlap), synthesized held steps chain with TIE. Import writes
  inside the `setPresetLoading` bracket, sets SYNC 1/16, LEN, GATE 1.0 (measured percents
  assume an unscaled gate), tempo (clamped 40–250), and latches the root via
  `applySeqLatchRoot` (requantize ⇒ enters on the drums' downbeat). Export: 480 PPQ, one
  cycle, TIE chains merge, pitch takeover = 303 overlap (+30 ticks).
- Editor: **entry point moved to the announced fallback (maintainer 2026-08-30)** — the
  combined dialog filter could not separate ".jass or .mid?" up front (JUCE's IFileDialog
  path builds ONE `COMDLG_FILTERSPEC` from the whole pattern string; separate dropdown
  entries are unreachable without patching JUCE). The preset LOAD/SAVE dialogs are back to
  `*.jass` only; STEP SEQ's title bar carries **LOAD MIDI / SAVE MIDI** buttons via the new
  generic `ModuleDescriptor::headerActions` (built by ModuleFrame next to the GATE latch).
  `importMidiFigure` shows bilingual error boxes (unreadable / SMPTE / no notes) and clears
  the write cursor on success. MIDI export deliberately does NOT touch preset name or clean
  state; both choosers start at the system's last-used folder, not the Presets folder.
- **Maintainer test round 1 (2026-08-30):** buttons found, export works; a RAW basic-pitch
  whole-song dump imported as doubled-note hash. Two fixes: (1) free-time material is
  REJECTED loudly (mean onset distance to the 1/16 grid > 0.2 steps => notQuantized, with
  the way out named: quantize in the DAW / export a looped section; the dump measures
  0.235, los_ninos.mid 0.0); (2) folding happens ONLY when a real cycle was detected —
  a non-looping file now imports as its first 32 sixteenths (the contract's fallback)
  instead of the whole piece piled mod 32.
- **Verification**: builds clean; the import algorithm was mirrored 1:1 in Python and run
  against `D:\downloads\los_ninos.mid` → PPQ 480, tempo 115.0, 240 notes → period 24, root 34
  (Bb1), two classes (80/86 vs 98), gates exactly 87/36/41 (AC1 ✓, figure arrives rotated to
  its first onset — a loop-identical rotation, inherent to the anchor rule). The in-app path
  (dialog → SeqMidiIO → engine) and the .mid round-trip (AC2) are the maintainer's ear test:
  LOAD `los_ninos.mid`, compare against the Los Ninos preset; SAVE as .mid, re-LOAD, compare.

## References

- [Source: docs/notes/Sequencer_Market_Analysis.md] — SMF round-trip conventions
- [Source: 15-7-per-step-gate-tie-slide.md] — gate/TIE/SLIDE semantics the import targets
- [Source: memory project_jass_session_2026_08_26] — Los Niños class data (104/43/49 ticks)
