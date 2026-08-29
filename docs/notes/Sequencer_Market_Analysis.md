# Sequencer Market Analysis — Pattern Entry

Story 15.6, Task 1. Written 2026-08-23. Sources: official manuals wherever available
(links per section); condensed from four research passes (classic hardware, current
hardware, software, modular).

**Question:** how do the sequencers people love — and the ones they curse — handle
*programming a pattern*: entering notes, writing rests and ties, accents, changing
length, recording live, and editing while the loop runs?

---

## The four entry archetypes

Every surveyed device uses one or more of these; the best-loved offer at least two.

1. **Value-per-step surface** — one physical control per step (SQ-10, BeatStep Pro,
   Metropolix). The whole pattern is on the panel; every edit is live. *JASS's step
   knobs are this archetype.*
2. **Toggle-on-a-grid (x0x)** — the bar is 16 buttons; click = note, unlit = rest;
   writing happens against the running loop (TR-808/909, Digitakt GRID, FL Studio).
3. **Typewriter step-advance** — play a key, the cursor advances; a dedicated gesture
   advances *without* writing (= rest), another advances *while holding* (= tie)
   (KeyStep, Digitakt STEP REC, Live/Waveform step input, volca bass). *JASS's
   write-by-playing (15.4) is this archetype.*
4. **Quantized loop overdub** — play in real time against the running loop, quantized
   to the grid (TR-909 TAP, Digitakt LIVE, volca, BSP, all DAWs). *JASS does not have
   this.*

**The dividing line between beloved and infamous is editing against the running
loop.** The 808/909/SQ-10/Digitakt/volca/FL workflows are all "hear the edit within
one bar"; the TB-303's two-pass batch entry is the one workflow everyone calls
painful — and the TD-3 reproduced it anyway, because its *per-step flag vocabulary*
(accent, slide, tie, rest) is what actually matters.

---

## Classic hardware

### Korg SQ-10 (1978) — JASS's spiritual ancestor

- **Entry:** one knob per step, 3 rows (A/B/C) × 12 steps, raw CV tuned **by ear** —
  no quantization, no display. Portamento per row (global, not per step).
- **Rests:** none native. Every step fires; silence is patched (row → VCA, knob to zero).
- **Ties/slides:** none per step.
- **Per-step modulation — the signature:** all three rows output simultaneously.
  Classic MS-20 patch: A → VCO pitch, **B → VCF cutoff** (per-step filter opening —
  proto-acid), C → VCA level or the SQ-10's own clock (per-step *duration*).
- **Length:** 12 (A+B parallel) or 24 (A→B serial); shorter needs a patch cable
  (last step's trig-out → reset-in).
- **Live/edit-while-running:** no recording (knobs only); editing is inherently live —
  turn a knob, hear it on the next pass. That *is* the workflow.
- **Fast:** zero modes, the pattern is physically on the surface. **Painful:** tuning
  by ear, no memory, no rests.
- Sources: [SQ-10 manual (PDF)](https://www.vintagesynth.com/sites/default/files/2017-05/SQ-10_Usermanual.pdf),
  [Vintage Synth Explorer](https://www.vintagesynth.com/korg/sq-10-analog-sequencer)

### Roland TB-303 (1981)

- **Entry:** two **separate blind passes**. Pitch Mode: press keys left to right —
  pitches only, no timing. Time Mode: per 16th step press one of three selectors —
  *note* / *tied note* / *rest*. The machine merges the two lists: pitch *n* is
  consumed at the *n*-th "note begins" step.
- **Rests:** explicit third time value (a rest consumes a step but no pitch).
- **Ties/slides:** tie = time-mode value (extends gate one step). **Slide = per-note
  flag** (hold TAP + SLIDE in pitch mode): gate stays open, pitch glides, envelope
  does not retrigger.
- **Accent:** per-note flag (TAP + ACCENT); boosts VCA and filter-env depth — the
  acid "wow". Full per-note vocabulary: octave up/down, accent, slide.
- **Length:** FUNCTION + STEP presses before writing (default 16).
- **Live/edit-while-running:** **no.** Patterns cannot be edited during playback;
  BACK steps the cursor back one step for correction.
- **Verdict:** the entry workflow is famously painful (nobody can see what a pattern
  holds); the *flag vocabulary* is the feature every modern acid box copies while
  replacing the entry with x0x-style grid editing.
- Sources: [TB-303 owner's manual (archive.org)](https://archive.org/stream/synthmanual-roland-tb-303-owners-manual/rolandtb-303ownersmanual_djvu.txt),
  [tinyloops programming guide](https://www.tinyloops.com/tb303/quick_results.html)

### Roland TR-808 (1980) / TR-909 (1983)

- **Entry:** the original x0x paradigm — pattern loops in write mode, select an
  instrument, press step buttons 1–16; the hit sounds on the next pass. 909 adds a
  running playhead LED across the 16 buttons.
- **Rests:** automatic — an unlit step is silence.
- **Accent:** 808 = **one shared accent row** placed on steps like an instrument, one
  global depth knob. 909 = two tiers: the shared TOTAL ACCENT row **plus per-step,
  per-instrument accent by pressing the same step button twice** — LED dark/dim/bright
  = off/normal/accented. The cheapest per-step dynamics UI in the whole survey.
- **Length:** 808: hold CLEAR + last step button; PRE-SCALE sets steps per beat.
  909: hold LAST STEP + button; SCALE cycles resolutions; SHUFFLE per pattern.
- **Live recording:** TAP (808) / Tap Write with metronome (909), quantized to the
  current scale; real-time erase by holding CLEAR + instrument key.
- **Edit while running:** yes — writing *is* editing the running loop.
- Sources: [TR-808 manual (archive.org)](https://archive.org/details/synthmanual-roland-tr-808-owners-manual),
  [TR-909 manual (synthfool PDF)](https://synthfool.com/docs/Roland/TR_Series/Roland%20TR-909%20Owners%20Manual.pdf)

---

## Current hardware

### Elektron Digitakt

- **Entry:** three modes. GRID (toggle trigs on 16 keys; hold-vs-tap distinguishes
  edit from remove), LIVE (real-time, quantize toggleable, and **non-destructively
  re-quantizable afterwards** via a 0–127 strength dial), STEP REC (cursor advances;
  NO = rest).
- **Ties/slides:** none — per-trig **LEN** parameter instead (up to INF).
- **Per-step data — the maximum:** **parameter locks**: hold a trig + turn any knob —
  that step gets its own value of *any* sound parameter (up to 72 per pattern). Plus
  per-trig micro-timing, retrigs/ratchets, sound locks (different sound on one step),
  and **trig conditions** (X% probability, FILL, 1st-play-only, A:B cycle counts).
- **Length:** up to 64 steps (4 pages); PER TRACK mode gives every track its own
  length *and* speed multiplier — full polymeter.
- **Edit while running:** everywhere; live erase in time with the sequencer.
- **The ONE thing:** hold-a-step-and-turn-any-knob. Per-step automation as a single
  gesture.
- Source: [Digitakt manual OS 1.51 (PDF)](https://www.elektron.se/wp-content/uploads/2024/09/Digitakt_User_Manual_ENG_OS1.51_231108.pdf), ch. 10

### Arturia BeatStep Pro / KeyStep

- **BSP entry:** step buttons toggle; **16 encoders = knob-per-step** (KNOBS button
  cycles the row's meaning: Pitch / Velo / Gate; drums: Shift / Velo / Gate).
  Encoders are touch-sensitive: touching *shows* the value without changing it.
- **BSP ties/slides — the elegant model:** the per-step **Gate value runs 1–99 % and
  then two special top values: TIE and SLIDE** — staccato→legato→glide as one
  continuum on one knob. "Fast Ties": hold step A + press step B ties everything
  between.
- **BSP extras:** per-step velocity 1–127; drums get per-step micro-shift ±50 %.
  Per-track lengths (polyrhythm mode walks a 3/4/5 example). Live recording is
  hard-quantized.
- **KeyStep entry — the typewriter, perfected:** play key(s), releasing advances the
  step (chords: hold several). **One button ("Rest/Tie") next to the keyboard:**
  pressed after release = rest; pressed *while holding* = tie; held down while
  playing = legato run. Three meanings, one button, no screen. Length additive up to
  64; Append/Clear-Last work during playback.
- Sources: [BSP manual 2.0 (PDF)](https://aadl.org/files/catalog_guides/beatstep-pro_Manual_2_0_EN.pdf),
  [KeyStep manual 1.1 (PDF)](https://downloads.arturia.net/products/keystep/manual/KeyStep_Manual_1_1_0_EN.pdf)

### Behringer TD-3

- Kept the TB-303's two-pass workflow verbatim (pitch list, then note/tie/rest per
  step) — reviewers call it charm and curse; the decoupling of pitch from rhythm is
  what generates acid lines. Modernized: **note LEDs show the current step's pitch**
  (the original was blind), BACK button, MIDI/USB + SynthTribe app editing.
- Per-step storage is exactly 4 concepts: pitch(+octave), accent flag (one global
  depth knob), slide flag, note/tie/rest. No live recording.
- Sources: [TD-3 quick start (PDF)](https://files.kraftmusic.com/media/ownersmanual/Behringer_TD-3_Quick_Start_Guide.pdf),
  [TD-3 programming guide (third-party)](https://airainfo.org/files/TD-3-Programming.pdf)

### Korg volca bass / volca keys

- **volca bass:** real-time + step recording (release advances; REC = rest) + STEP
  MODE (16 buttons as toggles, **per VCO** — three enable rows over one shared pitch
  line). **Slide on/off per step** (EG/LFO not retriggered = true 303 slide). No
  accent, no velocity.
- **ACTIVE STEP** (both volcas): steps can be **skipped** (removed from time, not
  silenced) live — restructures the loop's meter with one finger; editing gesture =
  performance gesture.
- **volca keys:** real-time only; **motion sequencing** (knob movements recorded into
  the loop, per-knob) and **FLUX** (fully unquantized recording on a 16-step box).
- Sources: [volca bass manual](https://cdn.korg.com/us/support/download/files/2f40ef4723d56c66090c59ef38ea2a1b.pdf),
  [volca keys manual](https://cdn.korg.com/us/support/download/files/93f9f953416733d196f8d817bb3bc668.pdf)

---

## Software

### Tracktion Waveform (Step Clips)

- Two first-class MIDI clip types: piano-roll clip and **Step Clip** (grid), with
  conversion both ways. Grid: click a cell to toggle; rows are fixed notes
  (reassignable); default 16 × 1/16.
- **The combined Velocity/Gate bar:** each step shows one bar — **height = velocity,
  width = gate** — so dynamics and articulation are shaped in one drag pass. Shift+
  drag paints velocities across steps. Newer versions add per-hit probability.
- Pattern *variations* with own step count/length, chained in a clip footer
  (song-mode-in-a-clip). Per-row shift/randomize/fill tools; groove templates.
- Step input in the piano roll: MIDI notes land at the cursor, cursor auto-advances
  by the snap grid.
- Editing while playing is documented as the intended way to build variations.
- Source: [Waveform manual — Step Clips](https://tracktion.github.io/waveform_manual/step-clips/)

### Ableton Live 12

- Piano roll only (no grid in the desktop UI). **Draw Mode (key `B`)** + **Pitch
  Lock** = paint one pitch row like a step grid; **Fold** hides empty rows. Velocity
  lane (with ramps, randomize, per-note re-randomizing deviation), **Chance lane**
  (per-note probability), MPE editor for per-note expression.
- **Step recording — the canonical grammar:** hold keys + **Right arrow commits and
  advances one grid step; Right with nothing held = rest; keep holding and press
  Right again = tie; Left arrow deletes the last entry.** MIDI-mappable.
- Loop brace = length; Ctrl+D doubles loop and contents.
- Source: [Live 12 manual — Editing MIDI](https://www.ableton.com/en/live-manual/12/editing-midi/)

### FL Studio (Channel Rack)

- The reference software step grid: **left-click = on, right-click = off** — entry
  and erasure never need a mode or modifier. Patterns 1–512 steps ("Auto" sizes to
  content); channels can diverge in length (polyrhythm); one swing knob per pattern.
- **Graph Editor (Ctrl+K):** per-step bar lanes for note, velocity, release, fine
  pitch, pan, Mod X/Y, **Shift** (sub-grid micro-offset) and **Rep** (ratchets).
  Right-drag across columns interpolates a ramp — the fastest per-step parameter
  editor surveyed. **Keyboard Editor:** a piano-key strip under the row sets each
  step's pitch without opening the piano roll.
- Steps are internally **zero-length notes** — the documented re-import path is
  "discard note lengths" (gate does not survive an FL step round trip).
- Source: [FL Studio manual — Channel Rack](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/channelrack.htm)

---

## Modular / boutique

### Intellijel Metropolix

- **A stage is not a step:** each of 8 stages owns 1–8 clock pulses plus a **gate
  type: REST / SINGLE (gate = % of pulse) / MULTI / HOLD (tie)** — one integer plus
  one enum turn a fixed grid into phrasing. Slide per stage with an 'Acid' (303)
  mode; ratchets 1–8; probability per stage *or* per pulse; **accumulator** (per-stage
  transpose in scale degrees that grows each pass).
- Length = stages × pulses (truncation mid-stage = instant polymeter); 18 playback
  orders; per-track clock div and swing; 8 MOD lanes with their own length/order.
- **LOOPY:** hold one or two stage buttons to loop a region momentarily — releasing
  returns to where the sequence *would have been* (non-destructive, spring-loaded).
- Editing grammar: ALT+turn = edit all stages at once; long-press = reset stage.
- Source: [Metropolix manual v1.2 (PDF)](https://intellijel.com/downloads/manuals/metropolix_manual_v1.2_2021.04.25.pdf)

### Make Noise René 2

- **Pattern entry is decoupled from note entry:** 16 knobs hold a pool of voltages;
  separate button pages decide **which locations are visited (ACCESS), which fire
  gates (GATE), which glide (GLIDE)** and by which of 16 snake paths. Mute steps,
  re-route order, change scale — the melody survives every one of those edits.
- Rests come in both flavors: skipped (SEEK — removed from time) or slept (SLEEP —
  rest that consumes a clock). Gate ties via Glide+Trig combination.
- **LATCH:** touched locations override access/gate live, non-destructively; with the
  clock stopped, touching a location plays it — the manual recommends exactly this
  for tuning notes (select, turn, move on).
- Source: [René 2 manual](https://makenoisemusic.com/modules/rene)

### Torso T-1 (honorable mention)

- **You don't place steps — you dial a density:** Steps / Pulses / Rotate spread hits
  Euclidean-evenly; per-pulse repeats (accelerating/decelerating ratchets), accent
  with a curve, per-parameter randomization amounts. Docs recommend the hybrid: let
  the generator lay the pattern, then hand-place the few hits that define the groove.
- Source: [T-1 docs — Euclidean rhythms](https://docs.torsoelectronics.com/t1/core-concepts/euclidean-rhythms/)

---

## Comparison table

| Device | Entry | Rest | Tie / slide | Accent / per-step mod | Length | Live rec | Edit while running |
|---|---|---|---|---|---|---|---|
| Korg SQ-10 | knob per step | none (patch) | global portamento only | 2 free CV rows (cutoff!) | 12/24 | no | inherently |
| Roland TB-303 | two blind passes | explicit time value | tie value + slide flag | accent flag, global depth | ≤16 | no | **no** |
| TR-808 | grid vs running loop | unlit | n/a | shared accent row + depth knob | ≤16(+part) | TAP | yes |
| TR-909 | grid + tap write | unlit | flam | **2nd press = accent (LED dim/bright)** + total row | ≤16, scale | yes | yes |
| Digitakt | grid / live / step-rec | no trig; NO-key | LEN param (no tie) | **p-locks: any param per step** + conditions, ratchets, µtiming | 64, per track | yes, requant dial | yes + live erase |
| BeatStep Pro | toggle + **knob per step** | toggle off | **gate 1–99 %→TIE→SLIDE** | velocity row, drum µshift | 64, per track | quantized | yes |
| KeyStep | typewriter | **Rest/Tie button** | same button while holding | velocity (capture only) | 64, additive | quantized replace | append/chop live |
| Behringer TD-3 | 303 two-pass + LEDs | time value | tie value + slide flag | accent flag, global depth | ≤16 | no | stepping only |
| volca bass | step-rec + per-VCO toggles | REC = rest | slide per step | motion seq; no accent | 16, active-step skip | quantized | yes |
| volca keys | live only | n/a | n/a | **motion seq**, FLUX unquantized | 16, active-step skip | yes | overdub |
| Waveform Step Clip | cell toggle | unlit | gate width (bar width) | **velocity+gate in one bar**, probability | steps × div, variations | step input | yes (intended) |
| Ableton Live | draw / dbl-click / step-rec | empty / **arrow = rest** | edge drag / **arrow while holding** | velocity+chance lanes, MPE | loop brace | yes | yes |
| FL Studio | **L-click on, R-click off** | unlit | none in grid (0-length notes) | **Graph Editor: 9 lanes incl. ratchets, µshift** | 1–512, auto | piano roll side | yes (core workflow) |
| Metropolix | slider per stage | REST gate type / SKIP | **HOLD gate type**, acid slide | ratchets, probability, accumulator, MOD lanes | stages × pulses, 18 orders | no | yes + LOOPY |
| René 2 | knob pool + route pages | SEEK skip / SLEEP rest | glide + gate-tie | access/gate/glide layers | = enabled locations | no | yes + LATCH |
| Torso T-1 | **Euclid: steps/pulses/rotate** | implicit | n/a | repeats, accent curve, per-param random | per track | n/a | yes |

---

## MIDI round-trip conventions (for STEP SEQ ⇄ MIDI)

- **Position** is lossless: step starts land on PPQ/4 ticks (16ths at 480/960 PPQ);
  micro-shift exports as off-grid tick offsets (naive grid importers destroy them).
- **Velocity = accent.** MIDI has no accent flag; accent rows are encoded as a few
  discrete velocity classes. Confirmed by this project's own measurement: the Los
  Niños reference MIDI carries the SQ-10 accent row as exactly two classes —
  v98/long vs v80/short. **An importer should cluster velocities, not read them
  continuously.**
- **Gate/tie/slide = note duration:** duration ≤ 1 step → gated step; spanning N
  steps → tie; **overlapping the next note-on → slide/legato** (the 303 convention).
- **Degenerate exports exist:** FL's step mode stores zero-length notes (gate does
  not round-trip); Waveform's Step↔MIDI conversion has documented gate/variation
  losses. Design rule: **treat SMF as position+velocity ground truth; reconstruct
  gate/tie/slide from duration as best effort.**
- Rests are never encoded — absence of a note in the step's tick window.

---

## Mechanisms that presuppose features JASS lacks

Flagged per story 15.6 AC1 — these cannot be adopted without new per-step storage
(preset FormatVersion 7):

1. **Accent** (TB-303/TD-3 flag, 808/909 rows, 909 per-step double-press, BSP
   velocity row) — needs one flag or value per step. Reserved as **story 15.2**; the
   Los Niños and Roboter presets are both waiting for exactly this.
2. **Ties / slides / per-step gate** (BSP's gate→TIE→SLIDE continuum, Metropolix
   HOLD, 303 slide) — needs a per-step gate/mode value. Today JASS has one global
   GATE.
3. **Parameter locks / motion sequencing / MOD lanes** (Digitakt, volca, Metropolix)
   — the maximal version of 15.2; per-step values routed as a matrix source.
4. **Ratchets, probability, micro-shift** (Digitakt, FL Graph Editor, Metropolix) —
   further per-step fields; cheap once the per-step container exists.
5. **Skipped steps** (volca ACTIVE STEP, Metropolix SKIP, René SEEK) — a *third*
   step state (removed from time, vs. JASS's current off = silent-but-time-consuming).

Everything in this list points the same direction: **the per-step data container
(format v7 step objects) is the enabling investment; each mechanism is then one
added field.**
