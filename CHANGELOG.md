# Changelog

All notable changes to JASS are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
JASS uses **CalVer** versioning: `YYYY.MM.MICRO` (e.g. `2026.07.0`), where `MICRO`
increments for additional releases within the same month. This is the app/release
version and is independent of the preset **`FormatVersion`** (an integer schema
contract — currently `6`; see [`docs/JASS_Preset_Format.md`](docs/JASS_Preset_Format.md)).

## [Unreleased]

### Added
- **STEP SEQ and PERC hold up to 192 steps, shown as four 48-step pages (story 16.3).** Two
  songs in a row had hit the same wall: pop music phrases in 2/4/8 bars — 32/64/128
  sixteenths — and the old 48-step maximum (a UI-geometry number, not a musical one) could
  not hold a 4-bar figure. The grids stay exactly as they were; **< / >** in the module
  headers page through the pattern (the read-out says shown page / pages — prev/next rather
  than one button per page, so raising the cap never widens the header), a dot marks the
  page that is playing, and a
  **FOLLOW** latch flips the view with the playhead (stepping a page by hand pauses it — the
  display must never flip away under an editing cursor; latching again jumps back). Writing
  a figure crosses pages by itself, PERC's COPY stamps across pages, and MIDI import/export
  carries the full length. 192 = 4×48 is a deliberately generous ceiling: the parameter list
  of a plugin is fixed at construction, so "as many as the song needs" has to be "more than
  any song will ask for". Old presets load and sound bit-identically (steps beyond their
  saved length default to silence — LEN rules, as always). Known trade-off, same as 16.2:
  the LEN ranges changed, so normalized VST3 automation of LEN from older sessions remaps;
  the standalone is unaffected.

## [2026.08.13] – 2026-08-31

### Changed
- **JASS is licensed under the AGPLv3 now** (was GPLv3). JUCE 9's free tier is AGPLv3 — the
  README had still described the old GPLv3 dual licence — and relicensing the app itself keeps
  the combination clean. For a desktop instrument the AGPL behaves exactly like the GPL; the
  network clause only matters for hosted services. The README also states the verification
  model plainly (no test rig — by eye and ear against measured references), and the internal
  planning artifacts left the repository for its public life.

## [2026.08.12] – 2026-08-31

### Added
- **STEP SEQ shows where the figure ends.** A red line after step LEN — the knob-row
  counterpart of PERC's dimmed beyond-LEN cells (dimming was no option here: a grey knob
  already means a rest, and two grey reasons cannot be told apart). It moves live with the
  LEN knob.
- **PERC carries 48 steps now too (story 16.2).** The grid grows to 3 bars of sixteenths —
  matching the sequencer, and two Los Niños cycles line up against a 48-step drum pattern.
  The module takes W24, four columns less than STEP SEQ: boxes pack denser than knobs, and
  the lane-name column gave up the air it kept between the names and the module edge
  (maintainer, by eye, 2026-08-31). Steps 33–48 register after every existing parameter (the same
  append-only discipline as the sequencer's); old presets load bit-identically, COPY/x2 and
  the lane strings in preset files simply span 48 now.
- **STEP SEQ carries 48 steps now (story 16.2).** The module grows from 20 to 28 rack
  columns: two rows of 24 step knobs — 3 bars of sixteenths, or exactly two Los Niños
  cycles — with the knobs their usual size and the spacing a hair tighter than before
  (maintainer's pick over a full-width variant whose spacer cell read as wasted air). Steps 33–48 are
  REGISTERED after every existing parameter — the append-only contract, old DAW state keeps
  its indices — but DISPLAYED in musical order via the new `ModuleSpec::bodyOrder`, which
  exists precisely to decouple those two orders. Old presets load bit-identically (new steps
  default to rests); the accent row, the GATE/TIE/SLIDE row, MIDI import/export and the write
  cursor all extend across the new steps.
- **The ADSR curve folds away — and starts folded (story 16.2).** A CURVE latch in the
  module's title bar shows or hides the envelope display; folded, the module is just its
  knob row, and the rack re-packs live — the modules below shift up, and back down when the
  curve returns. Since the sequencers took the width, ADSR lives among the flat modulators
  and ships folded (the curve is a look-up, not a thing to stare at). The fold is view
  state, never part of a preset, and the window's height budget always counts the expanded
  size, so unfolding can never overflow it.

## [2026.08.11] – 2026-08-30

### Added
- **PERC steps copy with one click.** Two new buttons in the module's title bar, both visible
  — no context menu, the house rule. **COPY** latches a stamp mode: click the source step (the
  whole column, all four rows) and every further click plants it on another step; right-click
  re-picks the source, a second COPY ends the mode. **x2** appends the pattern behind itself
  and doubles LEN (capped at the 32-step grid) — the classic drum-machine "same bar again,
  then vary the copy". Both write plain parameters, so presets, LiveState and the grid's own
  painting see the result exactly like hand edits.

## [2026.08.10] – 2026-08-30

### Added
- **STEP SEQ loads and saves MIDI now (story 15.8).** Two buttons in the module's own title
  bar — **LOAD MIDI / SAVE MIDI**. LOAD MIDI opens a `.mid` transcription as the figure: the
  grid is 1/16 anchored on the first note, velocities become the accent row by clustering
  (never read continuously: a transcription's wobble is noise, its two classes are signal),
  note lengths become per-step gate up to TIE and SLIDE with the 303's overlap convention,
  and a looped transcription is folded to its detected cycle, so a 240-note Los Niños rip
  comes in as its 24-step figure with velocities and lengths averaged across the passes. The
  figure latches to its most frequent note and simply starts playing, like a loaded sequencer
  patch. SAVE MIDI writes the figure back out (480 PPQ, one cycle, engine velocities 127/100,
  TIE chains merged into one note) for the DAW. One documented asymmetry: a pitch-changing
  TIE exports as the 303 overlap and reimports as SLIDE — MIDI cannot carry the difference.
  Errors are as loud as the preset path's. The design's first pick — teaching the preset
  LOAD/SAVE dialogs `.mid` — fell to a platform fact: the native chooser shows one combined
  filter entry, so the dialog could not separate "whole patch or just the figure?" up front;
  the module's own buttons answer that before any dialog opens (maintainer, 2026-08-30).

## [2026.08.9] – 2026-08-29

### Added
- **Presets store the drum pattern and the mod routings as structures now (FormatVersion 9).**
  The same cure v7 applied to the STEP SEQ figure, extended to the two remaining knob dumps:
  PERC's 140 flat keys become `"Lanes"` — one object per lane with `Note`, the generated GM
  drum `Name`, `Amp`, `Pan` (omitted when centred) and the step row as one readable string
  (`"Steps": "X...X...X...X..."` — exactly what the grid shows) — and MOD MATRIX's flat
  `Slot1Source`/… keys become `"Slots"`, one object per routing with `Source`, `Module`, the
  canonical `Param` index plus its generated `ParamName`, `Amount`, and `Quant` (omitted at
  Off). Values are untouched — this is structure only, so a re-saved preset sounds
  bit-identical — and every older file still loads: the flat keys remain readable forever,
  and the loader feeds the new arrays through the same spec-driven mapping as before, so the
  choice-label and legacy-key logic keeps living in one place.
- **Preset files write every field now, defaults included (FormatVersion 10).** v7–v9 kept
  the common case terse — a plain step omitted `Accent`, a full-length step its `Gate`, a
  centred lane its `Pan`, an unquantized slot its `Quant`. Terse is only a virtue for the
  writer: whoever *reads* the file has to know every default by heart (maintainer,
  2026-08-29). Loading still accepts omissions (missing ⇒ default, unchanged since v3) —
  only the writer stopped producing them.
- **Los Niños has its drums now — and a kit of its own.** The demo preset's percussion
  used to be the acoustic SamsSonor kick and snare stamped on every beat; against the
  record that read as one undifferentiated thump. A percussion stem of the original
  (separated with lalal.ai, measured with the audio-measure workflow) showed why no
  amount of level-tweaking could fix it: the record's kick is a *Synare-style electronic
  boom* — a sine that starts at ~105 Hz, holds, and glides to ~45 Hz over 300 ms — and the
  backbeat "Tschak" is not a hi-hat at all but a gated noise burst centred at 200–550 Hz
  ("more a car door slamming than a hi-hat", and the measurement agreed: nothing above
  9 kHz). Both sounds are now **synthesized from the measured curves**
  (`tools/synth_losninos_kit.py`) and ship in a new seeded kit, **LosNinosDrums**,
  alongside the top velocity layers of SamsSonor (CC BY-SA 4.0) for snare, toms and hats.
  The preset plays the classic alternation the ear expects — kick on the downbeats, slam
  plus snare on the backbeats, a quiet low tom keeping the record's continuous low pulse —
  which the stem justifies: the two beat classes differ almost only above 900 Hz, so the
  alternation is written into the *sounds*, not faked by dropping the low end.
  (`Samples/*.flac` now embeds alongside `*.wav`/`*.sfz` so a kit can seed FLAC.)
- **Every step has a length now — up to TIE and SLIDE (story 15.7).** The new GATE button in
  STEP SEQ's header flips the 32 step knobs to their second meaning: the step's gate, one
  continuum from 5–100 % of the step up to two values past the top — **TIE** (the note holds
  through the boundary and the next step takes over *without a new attack*) and **SLIDE** (the
  same, but the pitch glides — the 303's slide, the last mechanism every classic acid box has
  that JASS lacked). The BeatStep Pro's model, chosen in the 15.6 market survey: staccato →
  legato → glide on one knob. The global GATE still scales all plain steps, a step's default
  is 100 % — so every existing preset sounds bit-identical. Preset files store the value in
  each v8 step object as `"Gate": 36` / `"Gate": "TIE"` / `"Gate": "SLIDE"`, omitted at 100.
  Per-step gate knobs were tried and thrown out in 15.1 ("they were all sitting at 1") — what
  changed is a measured preset that needs them: the Los Niños original separates its two
  accent classes by *length* as much as by level (87 % vs 36 % of a step), which no accent
  flag can express. **Los Niños now carries exactly those measured lengths.**
- **STEP SEQ has an accent row (story 15.2).** The corner switch of every step now cycles
  off → on → **accented** — the TR-909's "press the same button again" gesture, so 32 accents
  cost no rack space — and the new ACCENT knob sets what an accent does: the step plays louder
  and the filter opens for it (the TB-303/SQ-10 recipe; at 0, accents change nothing). Under the
  hood an accented step is simply emitted **hot** (MIDI velocity 127 against the plain 100), and
  the voice maps velocity onto gain and cutoff — which means a MIDI keyboard now also plays
  touch-sensitively exactly as far as ACCENT is turned up, and a future MIDI export carries the
  accents for free (the Los Niños reference MIDI encodes its accent row as exactly two velocity
  classes). The market survey behind story 15.6 showed this per-step second class is the one
  mechanism every classic sequencer has and JASS lacked — it is what the Los Niños preset
  fakes with rests, and what parked the Kraftwerk attempt.
- **Presets store the figure as music now (FormatVersion 7).** The STEP SEQ block writes its
  steps as an array of note objects — `{ "On": true, "Note": 46, "Name": "Bb1", "Accent": true }`
  — instead of 64 flat knob fields. The absolute MIDI note is canonical (resolved against the
  latched root; the figure still transposes with the played key), the spelled name is generated
  for the reader's eyes and ignored on load, so the two can never diverge. Older presets keep
  loading unchanged, and are auto-migrated with a backup, as always.
- **Los Niños plays the real figure now.** The MIDI transcription of the original shows all
  24 sixteenths sounding, in two classes — long/loud versus short/quiet, the Korg SQ-10's
  accent row. Until 15.2 the preset could only fake that by leaving the quiet class out; now
  the rests are gone and the loud class carries the accent flag (the three high fills sit
  *between* the two classes in the original and stay plain). The two shipped sequencer
  presets (Los Niños, DAF Beat) are re-saved in the new v7 step-object format; DAF Beat's
  figure itself is unchanged — whether it wants accents is an ear decision, not a data one.
- **The write cursor can move backwards now: ← / → navigate, BACKSPACE takes back the last
  note.** Writing a figure by playing it (15.4) only ever ran forward — one slip meant reaching
  for the mouse, clicking the step knob and re-aiming. The arrow keys move the ring without
  touching the figure, BACKSPACE steps back *and* switches that step off — the pitch survives
  (the SPACE rule), so re-writing or re-enabling the step restores it. All three keys are only
  claimed while the ring is showing; outside of writing they keep their meaning (the market
  survey for story 15.6 showed this is the one gesture every step-entry grammar has and ours
  lacked — the 303's BACK button, Ableton's Left arrow). The help pages now also name ESC as
  the way out of writing, which shipped in 2026.08.8 but was nowhere to read.

### Fixed
- **Format upgrades no longer rotate the MOD MATRIX targets.** The v5→v6 param-order remap
  guarded itself only by "does the file carry `SlotNModule`?" — true for every v6+ file too, so
  each FormatVersion bump since v7 re-applied the permutation to already-sorted indices and
  quietly moved routings one step (Alle OSC: FREQ→FB→DETUNE→AMP). Found during the v9 bump when
  a converted preset's drift slot came back pointing at feedback. Both migration passes are now
  gated on the file's actual version (`< 5` / `< 6`), and the seeded demo presets in AppData were
  restored from their repo originals (the drift slot of DAF Beat, Chaos Melody, Matrix Showcase
  and Helikopter had been rotated by the v7/v8 upgrades).
- **A rest's pitch knob is editable again.** Switching a step off — with the corner switch,
  SPACE while writing, or BACKSPACE — greyed its knob *and* took away the mouse, so re-pitching
  a rest meant switching it back on first (and hearing it) just to turn the knob. But a rest
  deliberately keeps its pitch (that is why SPACE and BACKSPACE don't clear it), so the knob now
  only dims: a rest's note can be dialled in and auditioned while the step stays silent in the
  figure. Mode-irrelevant knobs (say, STEREO's WIDTH outside Pseudo-Stereo) still lock the mouse
  out — there the value genuinely does not apply.
- **The step switch is easier to hit.** Cycling a step (off → on → accented) demanded pixel
  aim at the little box. Its clickable area now extends into the cell corner's empty air —
  roughly double the target — while the drawn box stays exactly where and what it was.
- **Double-clicking a knob now returns it to the loaded preset's value** — instead of wiping
  it to the factory default, which is what silently happened before: a JUCE automatism, never
  a JASS decision, and with click-to-audition a trap (two quick clicks on a step knob reset
  its pitch to 0). The preset baseline is the safe version of the same idea: an untouched knob
  already sits on it, so the gesture cannot fire by accident — it only ever un-does your own
  twisting, the one-gesture way back after comparing. On an unsaved working state (no clean
  baseline) a double-click does nothing. Typing an exact value stays on right-click.
- **Step preview, entry and the note boxes follow the root that actually sounds.** The figure
  transposes with the latched key, but clicking a step previewed it over the keyboard's C — with
  Los Niños latched on Bb, the click sounded a different note than the loop played at that very
  step, so a melody could not be assembled by ear (maintainer, 2026-08-23). Preview, write-by-
  playing and the boxes' note names now all resolve over one reference: the latched root while a
  figure runs, otherwise the keyboard's current C as before. That also makes playing a figure in
  over a running latch WYSIWYG — the keys played are the notes the figure then plays.
- **Loading a preset now lands it exactly as saved — every time, on the first try.** Presets
  sometimes arrived scrambled and had to be loaded two or three times "until everything fits".
  The cause was structural: applying a preset writes ~700 parameters (factory reset + the file's
  values), and every single write fired the parameter couplings meant for hand gestures — the
  MOD-MATRIX auto-enable re-evaluated on each of its ~24 slot fields, the ARP/STEP-SEQ exclusion,
  the CROSS-MOD operand coupling — all reacting to half-applied intermediate states. Worse, the
  couplings' auto-enable memories ("*we* switched this module on, so we may switch it off again")
  survived the preset change, so the result of a load depended on which patch was loaded BEFORE —
  repeated loading merely converged on the right state. A saved preset is a snapshot taken after
  every coupling already ran, so it is now applied verbatim: the couplings stay silent during the
  load and their memories are cleared with the outgoing patch. Hand gestures behave exactly as
  before.
- **Loading a preset silences the previous patch's voices.** Nothing ever did: a note held (or
  still ringing) across the switch kept its voice alive and playing through the NEW patch's
  parameters — and when the new patch runs the STEP SEQ, the sequencer's chord filter swallows
  the key's later note-off, so the leftover voice hung forever. Switching from GrandPiano to
  DAF Beat left a saw drone running under the beat; the rack looked exactly like the preset,
  because it *was* the preset — the dirt was a voice from the patch before. Every load now
  requests a hard all-voices stop, executed at the top of the next audio block, before the new
  patch plays a note. The last piece was intermittent — sometimes a saw C4 still hung until the
  next keypress chased it away: every load also fires the "generator newly enabled" edge that
  re-arms the auto-play drone, racing the latch start by a few audio blocks. That edge now yields
  to a latched figure and to held keys — the drone only re-arms when the instrument is otherwise
  silent, which is the only time it was ever wanted.

## [2026.08.8] – 2026-08-23

### Added
- **New demo preset "Los Ninos" (F12).** Liaisons Dangereuses' "Los Niños del Parque" (1981,
  MS-20 + SQ-10) as a sibling to DAF Beat — same school, different trick: where Mussolini
  hammers straight 8ths, this figure is a **24-step (6-beat) loop running polymetrically over
  the 4/4 drums**, so bass and beat realign only every three bars. Measured from the record
  (54 folded cycles): 114 BPM, Bb1 pedal with the beat-3 note displaced onto the 16th after
  the beat, Db3/Ab2 accents, and the Db3→Eb3 pickup into the cycle's downbeat; saw through a
  dark resonant lowpass (≈330-420 Hz, Q 2.6 — the MS-20 squelch), swept per 8th by the same
  LFO→cutoff trick DAF Beat uses. Figure refined against two ear-played covers and the
  maintainer's ear — the analysis alone misread one position (Gb1's 3rd harmonic sits exactly
  on Db3's fundamental; the ear broke the tie).
- **The KEYBOARD help names the range.** 88 keys, A0–C8 — the full piano layout, matching a
  grand or an 88-key digital piano — and the note that MIDI input is not limited by the
  visible range. It was nowhere to read; now it is where a player would look, on the
  module's own help page.
- **QUANT — a per-row scale mask in the MOD MATRIX.** Pitch modulation had one failure mode this
  synth already paid to learn (story 14.1): continuous detune across notes reads as *out of tune*,
  not as analog. Quantized jumps are a different animal — they are not detune, they are **notes**.
  QUANT snaps a row's pitch contribution to a scale (Chromatic / Major / Minor / Pentatonic), so
  S&H or Chaos on FREQ stops being random wobble and becomes a random-but-in-key melody over the
  held note — the modular classic S&H → quantizer → VCO. It sits on the ROW, not on the target,
  and snaps each row *before* the rows sum: a smooth vibrato row keeps gliding right next to a
  quantized melody row. Off by default; existing presets are untouched. The MOD MATRIX takes the
  full rack width for the extra combo — the whitespace the 30-column grid once "only gained" is
  exactly what the fifth control now spends.
- **CHAOS — a Lorenz attractor as a modulation source.** Deterministic chaos instead of
  randomness: the orbit never repeats, but it is not noise — it wanders with intent, which is
  exactly what a periodic LFO cannot do and a random generator overdoes. The module exposes
  **two** matrix sources, **Chaos X** and **Chaos Y**, and that is the point: both ride the same
  orbit, so two routings (say X → FILTER CUTOFF, Y → WT POS) drift *together but not alike* —
  one gesture in two colours, something two independent random sources can never produce. One
  global attractor drives all voices (correlated movement reads as intent; per-voice chaos would
  just read as blur), it free-runs — notes do not restart it — and the rings show the very same
  values the voices modulate with. RATE sets how fast the orbit turns.
- **Two new LFO shapes: S&H and One-Shot.** Until now every LFO wave was smooth and periodic —
  fine for vibrato and wah, useless for two other kinds of movement. **S&H** (sample & hold)
  draws a new random level each cycle and holds it: stepped motion, the classic modular
  step-randomizer, and with SYNC it steps in time. Every voice draws its own sequence — eight
  voices stepping in lockstep would read as one mono effect, not as life. **One-Shot** runs a
  single full-to-zero sweep per note and then holds, which makes modulation *event-based*
  instead of cyclic: a per-note gesture (a pluck-style cutoff drop, a pitch fall-in) without
  burning the pitch envelope. One honest limit: the little modulation rings — and master-bus
  routings — are fed by a free-running display LFO that never sees a note-on, so a One-Shot
  ring parks dark at idle. One-Shot is a per-note concept; the master bus has no notes.

- **DELETE — presets can finally be deleted from the app.** SAVE and LOAD existed; removing a
  preset meant leaving the app for the file explorer. The new DELETE button next to them opens
  the same chooser, asks once, and moves the file to the **recycle bin** — never a hard delete.
  Any F-key still pointing at the deleted preset is cleared along with it.
- **Self-FM (FB) reaches the WAVETABLE and SUB generators.** The FB knob has lived on OSC 1–3
  since July, but the two generators where it pays off most went without: on a wavetable the
  same feedback depth sounds different on every table and POS (the feedback works on the frame's
  own spectrum), and on the SUB it morphs the clean sine toward a brighter, saw-like bass without
  needing a second oscillator. Both knobs are mod-matrix targets (`WT FB`, `Sub FB`), so an
  S&H or envelope can now drive the growl. Existing presets are untouched — the new knobs
  default to 0.

### Fixed
- **Saved presets write clean numbers again.** Parameters live as 32-bit floats; casting one
  straight to double dragged its binary error into the JSON — a knob set to 0.6 was saved as
  0.599999964237213, which made presets unreadable and diffs noisy. The writer now emits the
  SHORTEST decimal that parses back to the very same float ("0.6"), so the file is cosmetic-
  clean while every value stays bit-identical. Deliberately NOT blanket rounding: two fixed
  decimal places would have silently doubled a 5 ms attack to 0.01.
- **Figure-writing mode now has real exits: ESC ends it, and loading a preset clears it.**
  Entry into writing mode is deliberately cheap (15.4: touching any step knob moves the write
  cursor there), but the only exits were writing past the pattern's end or switching the module
  off — so tuning one step by ear left the sequencer armed indefinitely, with every played key
  overwriting the figure, and a freshly loaded preset inherited that armed cursor from the
  previous patch. ESC now stops writing (matching its "stop editing" meaning everywhere), and
  the shared preset-load path drops the cursor — a loaded figure is exactly what the next
  played key would have overwritten.
- **Loading a preset no longer arms the STEP SEQ write cursor.** Every preset with an active
  step left the sequencer in figure-writing mode (ring on a step, keys overwriting the
  pattern instead of playing it). The step ON/OFF switch previews its step on click — but the
  preset loader replays every switch through the same notification path, so "the preset
  switched a step on" was indistinguishable from "the player clicked it", and previewing is
  what arms the cursor. The knob had learned this lesson in 15.3 (only a gesture with the
  mouse on the control may sound); the switch now carries the same guard.
- **The WAVETABLE BANK picker actually picks the bank now.** Choosing any built-in except
  Basic played *Spectral*: the combo's stock attachment spreads its six items across the
  parameter's full 0–63 range ("Digital" wrote 13, "Vocal" 38 …), and the bank lookup
  clamped every one of those onto the last bank. The display mirrored the same error
  backwards, so it *looked* right while sounding wrong. The SET combo had this exact bug
  and its fix (item index == value) — BANK was simply missed; found by ear when the
  new FB knob was auditioned per bank and every bank growled identically. Renaming or deleting a preset file on disk left
  its F-key erroring on every press, forever. The error message now says what happened — and then
  removes the assignment, so the second press does nothing instead of failing again.
- **A loaded sequencer patch now always enters on the drums' downbeat.** Loading a preset with a
  stored STEP SEQ latch sometimes played the bass permanently off the PERC beat. Two causes, one
  fix: the quantised entry read the drum transport state *before* that block had refreshed it
  (first block after a load saw the previous patch's drums), and a latch loaded over a
  still-running one produced no silence-to-figure edge at all, so nothing re-quantised. The drum
  clock is now resolved before the sequencer block, and a loaded latch explicitly restarts the
  figure at step 0, quantised to the pattern's next bar — the drums are the clock; the bass joins
  them, not the other way round.

### Added
- **The keyboard names the key under the mouse.** A fixed readout in the keyboard's top-right
  corner shows the hovered key as name and MIDI number ("A3 · 57") — a fixed place to look,
  deliberately not a tooltip chasing the cursor. It exists for transcribing: sheet music names
  notes, GM drum tables and SAMPLER ROOT count numbers, and until now only the C keys carried
  a label, so everything in between was counted off by eye.
- **PERC's NOTE knob shows the MIDI number beside the instrument** ("Kick · 36"). Drum
  transcriptions and the GM map speak in numbers; the knob said only "Kick", so transferring
  a template meant guessing which name was which number. The stored value is unchanged —
  the number was always there, now it is visible.
- **Hovering a named read-out shows its full text — and tooltips work at all now.** The rack
  had `setTooltip` calls for years, but no `TooltipWindow`; JUCE shows nothing without one, so
  every tooltip was silent. With the window in place: a value box narrower than its name
  (PERC NOTE "HH Closed · 42") shows the full text on hover, STEP SEQ boxes add the MIDI
  number to their note name ("E1 · 40"), and the module ↺/ⓘ button hints finally appear.
- **SAMPLER ROOT reads as a key, not a number.** The box says "C4" instead of "60" — a root
  is a key, and everything else in JASS already names keys — and the hover adds the MIDI
  number ("C4 · 60"). The stored value is unchanged; typing a number still works.

### Changed
- **AMP and PAN sit together now, at the end of every generator.** OSC 1–3, WAVETABLE, SUB
  and KARPLUS scattered the two across the row (OSC read WAVE·FREQ·AMP·…·PAN); SAMPLER,
  NOISE and PERC already kept them paired. Level and placement are one decision — where the
  generator sits in the mix — so the knobs stand side by side as the module's output stage,
  after the sound-shaping controls. Layout only: no parameter, preset or routing changes.
- **Every level knob is called AMP now — on the knob, in the matrix, and in the preset.**
  Most generators already said AMP (OSC 1–3, WAVETABLE, NOISE, KARPLUS, PERC's own knobs),
  but SUB and SAMPLER said LEVEL, so the rack taught two words for one thing. Both knobs
  and their MOD-MATRIX param labels read AMP now, and the preset fields follow (`Sub.Amp`,
  `Sampler.Amp`, `Perc.Amp1..4` — PERC's knobs were renamed on the panel back in August but
  the file still said Level), because a format that says one thing while the knob says
  another is the same confusion moved into a file. Old presets keep loading: the reader
  falls back to the old key (a new `legacyPersistKey` mechanism, available for any future
  rename), and the shipped demo presets are rewritten. Mod-matrix routings keep their
  slots — nothing saved changes its meaning.
- **STEP SEQ shows notes, not offsets.** A step's value box used to read "+7" — a number you had
  to resolve in your head while the preview already played the actual pitch. The box now shows
  that pitch by name (E1, C3 …), resolved over the same reference the preview sounds: the
  keyboard's current C, following the Up/Down octave keys live. Nothing about the figure changed —
  the stored value is still the offset, and the pattern still transposes with the key you play.
  The knob stays a knob (maintainer's condition), only its read-out grew up.
- **RANDOM draws from the new material too.** A random patch can now pick the S&H/One-Shot
  waves, route Chaos X/Y (which auto-enables CHAOS, so the routing is actually heard), and land
  on a QUANT scale — S&H → FREQ → Major is exactly the kind of happy accident the button is for.
  Random patches will *feel* different than before; that is the feature working, not a bug.
- **Self-FM is damped DX-style (all oscillators).** The feedback loop now averages the last two
  samples instead of feeding back the raw previous one. The raw one-sample loop flips sign at the
  loop's own Nyquist and erupts into broadband noise well before the knob's end — the classic FM
  synths average two samples precisely to cancel that alternating component. High FB values now
  stay a playable growl instead of a hiss; low and mid settings are essentially unchanged.

## [2026.08.7] – 2026-08-12

### Added
- **Choke groups: a closed hi-hat now silences the open one.** On a real kit an open hat cannot keep
  ringing once the pedal closes, and every drum kit says so in its `.sfz`: a region declares
  `group=N` ("I belong to group N") and another `off_by=N` ("when I sound, silence group N"). JASS
  read neither, so the two hats sounded over each other — it did not read as a groove, it read as a
  mistake. Both opcodes are parsed now and the choke acts **across voices** and across PERC's four
  tracks. It **fades** over a few milliseconds rather than cutting: through the very same release
  ramp a note-off uses, because a hard stop clicks and that lesson was already paid for. A kit that
  uses neither opcode — both grand pianos, every set shipped with JASS — behaves exactly as before.

- **The STEP SEQ latches: the first key starts it, and it keeps running.** Holding a key down for a
  whole piece is not playing, it is standing still. A new key moves the figure to that root, the
  Up / Down octave keys shift it (there is no held key left for them to retune, so the latched root
  takes the octave instead), **SPACE stops it**, and so does switching the module off — its switch
  is still the transport. The on-screen keyboard **shows the note the pattern is playing**, in the
  MODULATION colour so it reads apart from the key you are holding. Those notes deliberately never
  enter the keyboard state: the sequencer looks there for its root, and it would have kept
  re-rooting itself on its own output.
- **STEP SEQ shows the step it is playing**, the way PERC's grid does: a lit dot on the step's own
  number, beside its on/off box. It reads apart from the write cursor's ring on purpose — the two are
  visible at once and mean opposite things, what is sounding versus what your next key will overwrite.
- **A patch can be saved while its figure is running, and comes back running.** The new
  `StepSeq.LatchRoot` field carries the note the pattern is latched to, so a sequencer preset plays
  itself the moment you load it instead of waiting to be touched — `DAF Beat` starts on C3. A preset
  without the field loads silent, exactly as every preset written before it, and clears whatever the
  previous patch left playing.
- **Write a STEP SEQ figure by playing it.** A step's value is a number of semitones, so authoring a
  figure meant knowing the interval and then converting it into a number — the conversion is exactly
  what a sequencer is supposed to do for you. The module's **Reset** button now empties the pattern
  and starts writing at step 1: a ring marks the step waiting for a note, and every key you play —
  computer keyboard, on-screen keyboard or a MIDI keyboard — is written there, switched on, sounded
  once for confirmation, and the ring moves on. **SPACE** leaves a step silent, which is how a rest
  is written, since every key already means a note. Writing stops by itself after LEN steps. A
  **click on any step knob** puts the ring there, so a wrong note is corrected by clicking it and
  playing again — and the correction runs on into its neighbours. While the ring shows, the keyboard
  writes instead of starting the pattern: otherwise every note entered would restart and transpose
  the figure under your hands. The reference pitch is the keyboard's current C, the same one the
  step preview plays, so what you hear when turning a knob and what you get when playing a key agree.
- **PERC — four percussion tracks on a 32-step grid**, with their own kit, their own clock and a
  level per track, played **dry into the master bus**. It runs the moment you switch it on: no key,
  no root, because it never becomes a note. **Left click sets a step and sounds it**, right click
  clears one, and dragging carries on — a row of hi-hats is one gesture, and you hear what you
  place. Each row of the grid is named after the instrument it plays; the NOTE knob names it too
  (Kick, Snare, HH Closed) instead of showing a number, from the General MIDI drum map or the kit's
  own sample names. Per track an AMP and a PAN, plus one AMP for the whole kit — balance and level
  are different jobs, and the module's own AMP reaches +12 dB because a drum kit is mastered with
  headroom while three oscillators with unison are not. While a preset's kit is still loading in
  the background, PERC stays **silent** rather than play whatever set the stale index points at,
  and its KIT list offers **only mapped sets** — a single recording would put the same file on all
  four tracks at four pitches. Nothing is selected until you choose something. LEN 16 on 1/16 is the
  classic one-bar drum grid, 32 leaves room for the fill in bar two. And the drums are the clock:
  with PERC running, a STEP SEQ figure started from silence enters on the **next start of the drum
  pattern** rather than wherever the key happened to fall.
  It is deliberately **not** a second note sequencer. JASS is monotimbral — a voice starts every
  enabled generator, and filter, distortion, delay and reverb all live inside the voice — so drums
  sent as MIDI would come out through the bass's resonant lowpass. PERC renders after the synth and
  before the compressor instead, which makes "dry" the construction rather than a setting. Under
  the hood it is a second SAMPLER at processor level: sets, SFZ parsing, velocity layers and
  background loading all come from the existing sampler, so what is a drum machine today can carry
  any sampled instrument later.
- **Demo preset `DAF Beat`** on F11 — the `DAF Bass` patch with the drums underneath it, and the
  first preset that plays a whole piece by itself. The pattern is the one the record's drum
  transcription shows, on the grid the measurement supports: SYNC 1/8, so a step is an eighth and
  the 16-step loop is two bars, exactly as long as the bass figure. Kick on 1/5/9/13, snare on
  3/7/11/15 plus a pickup at 16, hi-hat on every step. Needs the free SamsSonor kit in
  `%AppData%\JASS\Samples`; the preset restores it **by name**, so it finds the right kit on any
  machine rather than whatever set happens to sit at that index.
- **A STEP SEQ step sounds while you edit it.** A step's value is a number of semitones, not a
  note, so writing a figure meant guessing an interval and then holding a key to find out what you
  had written. Turning a step's knob now plays it — re-triggered on every semitone, so a drag
  scrubs the scale — and so does simply clicking one, which is the quickest way to ask what a step
  holds without changing it. Switching a rest back on sounds it once, too. The reference pitch is the
  computer keyboard's current C (C3 by default), so it moves with the Up / Down octave keys and the
  preview is in the octave you are playing in. It rides its own MIDI channel: while the sequencer
  runs, every played note on channel 1 is deliberately swallowed so that only the pattern sounds —
  which is exactly when the preview is wanted.
- **STEP SEQ — a 32-step note sequencer** (two rows of sixteen). Hold a key and an authored figure plays,
  transposed by that key (the lowest held note is the root). Each step carries its own
  semitone offset and a switch — off is a rest — while note length is one GATE for the whole
  pattern, 1.0 holding each note into the next step;
  step length is a note division on the same clock the LFOs and DELAY ride on, or a free
  rate in steps per second. This is the thing the ARPEGGIATOR could never do: it can only
  re-order the notes you are already holding, and it runs free in Hz rather than in time.
  Both replace the held chord, so only one of the two can run — switching one on switches
  the other off. Hidden by default (rack height is a budget); a preset that enables it
  reveals it. Legato is the point of the gate design: at 1.0 the previous step's note-off
  is emitted after the next note-on, so the notes overlap instead of leaving a hole.
- **SFZ `#define` macros are understood.** A library that names its drum map once
  (`#define $KEY_KICK 36` … `key=$KEY_KICK`) used to load as *nothing*: an unresolved
  macro made every key unparsable, and an unparsable key drops its region. Kits like
  SamsSonor now load straight from their own `.sfz`, with no curated copy in between.
- **Generated SFZ sources no longer break a load.** SFZ reserves sample values starting
  with `*` — `*silence` and the built-in waveforms — for sources it synthesises rather
  than reads from disk. JASS looked them up as filenames, so one `*silence` region was
  enough to fail an entire kit with “*silence is missing”. Those regions are skipped now.
- **Free-running knobs grey out while tempo-synced.** With SYNC on a note division the
  DSP ignores the LFO/STEP SEQ RATE and the DELAY TIME entirely, so those knobs now dim
  the way any other inactive control does instead of sitting there looking live.
- **Demo preset `Drum Pattern`** on F10 — the sequencer driving a drum map instead of a
  melody: the step offsets pick the instrument (kick, snare, hats) rather than a pitch, and
  two steps are switched off so the pattern has real gaps. Needs the free SamsSonor kit in
  `%AppData%\JASS\Samples`; without it the SAMPLER simply finds no set.
- **Demo preset `DAF Bass`** on F9 for fresh installs — the sequencer showing what it is for,
  with the 16-step figure and the tone measured off the record it was built against: a
  sawtooth through a resonant lowpass whose cutoff is swept once per step by a tempo-synced
  LFO, plus a slow, shared pitch drift of about ±14 cents. Hold a low B and it plays.

### Changed
- **`DAF Bass` (F9) and `Drum Pattern` (F10) are retired.** They introduced the STEP SEQ: a bass
  figure measured off a 1981 record, and a drum map driven step by step because there was nothing
  else to play drums with. `DAF Beat` on F11 does both better — the same bass with PERC underneath
  it — and PERC replaced the drum-map workaround outright. F9 and F10 are free for your own patches
  now; existing bank assignments are untouched. `DAF Beat` also drives its kit at **full AMP**.
- **One knob size for the whole rack, and modules built around it.** The rack drew six different
  rotary sizes — 34, 40, 44, 45, 46 and 53 px — none of them chosen: a knob simply took what its
  cell happened to leave. Every rotary is now **40 px**, capped by its cell the way every combo box
  is capped to one width. 40 is the measured ceiling, not a preference: SAMPLER packs its row
  tightest and offers a 48 px cell, which holds exactly 40 once the rotary's own margin is taken —
  anything larger would have meant widening a module. Module heights now follow **from** the knob
  instead of the other way round. That needed a finer raster: a rack row was a whole 114 px, sized
  for one content row *plus the header*, so a two-row module paid for the header twice and 238 px
  was the only height it could have. The vertical unit is now a **quarter** of that (21 px), which
  reproduces 114 and 238 exactly — every existing module stands where it stood — and makes the
  steps between them sayable. **MOD MATRIX, STEP SEQ, PERC and ADSR are 207 px instead of 238**, all
  captions and value boxes intact. And the dead strip along MOD MATRIX's right edge — the 16 px that
  32 cells of 54 px leave over — is now spent as three narrow bands **between** its four routing
  slots, in the same dim tone the inactive slots use, so the grouping reads as grouping.
- **STEP SEQ and PERC start visible.** They were hidden until switched on, on the argument that rack
  height is a budget. The budget got cheaper, and a sequencer you have to go looking for is a
  sequencer you forget you have.
- **A knob now fills the cell it sits in.** Its block was a constant 81 px — caption, a 46 px rotary
  and a value box — which fits a one-row module exactly and leaves a two-row module's cell a fifth
  empty. In MOD MATRIX that showed as a band of nothing between the two routing rows. The cause was
  derived from the layout code twice and guessed wrong twice, so it was finally **measured**: the
  cell there is 104 px tall but only 62 px wide, and since the rotary is capped by the narrower
  side, the row's height simply went to waste. The diameter is therefore taken from the cell now —
  bounded by the height left after caption and value box, by the cell width, and clamped so the old
  46 px stays the **floor** and nothing in the rack gets smaller. MOD MATRIX's AMT additionally
  claims a second body slot, which is what makes its cell wide enough: **46 → 65 px**, and the gap
  between the rows falls from 26 px to 4. STEP SEQ and PERC reach 53 px, where the cell width is the
  limit. No module changes its footprint, so the window, the height budget and the fit scale are
  untouched.
- **The mouse wheel counts in single steps on a discrete knob.** A knob whose interval is one
  whole unit was only treated as discrete up to 24 positions; wider ones fell through to the
  proportional feel, which on a STEP SEQ step (±24 semitones) meant **two semitones per notch** —
  an interval you cannot aim at, and reachable only by holding Shift. Up to 48 positions the wheel
  now moves exactly one unit, whatever the modifiers. Genuinely long integer ranges (SAMPLER ROOT,
  24…96) keep the proportional feel, or crossing them would take a hundred notches.
- **Builds target AVX** (`/arch:AVX`, `-mavx`) instead of the SSE2 default. Measured on
  the HRIR convolution: 303 ns per sample at SSE2, 94 ns at AVX — and 94 ns at AVX2, which
  buys nothing here, so the lower of two equally fast baselines was taken. It does raise
  the floor: a CPU older than 2011 (Sandy Bridge / Bulldozer) will no longer run JASS.
- **`Drum Pattern` plays at a sensible level.** The sequencer sends velocity 100 and an SFZ
  without `amp_veltrack` tracks velocity per the spec default, which alone costs 4.2 dB; the
  preset's SAMPLER level now compensates.

### Fixed
- **PERC's playhead marked the wrong step.** It showed the sequencer's step counter, which is
  advanced to the *next* step the instant one fires — so the marker ran a whole cell ahead of the
  beat you hear (125 ms on a 1/16 grid at 120 BPM). Both sequencers now remember which step is
  actually sounding, and both mark that one.
- **Hiding the scope or the spectrum actually gives their height back.** The panel's own advice
  when the rack is over budget is "hide a module" — and for almost every module it did nothing:
  the worst-case measurement counts a hidden module anyway if it is factory-visible, because a
  preset enabling it would reveal it again and resize the window. Measured on the maintainer's
  machine, hiding both VISUALIZATION modules moved the number by zero. They are now marked as
  visual-only — the two modules in the rack that nothing can be heard from — so hiding them is
  taken at face value and no preset reveals them behind your back. Measured again afterwards:
  1732 px → **1446 px**, so the two of them were holding **286 px** of a 1929 px budget.
  Deliberately narrow: applying the same rule to a module that makes sound would let a preset
  load without part of its patch.
- **The MODULES panel no longer disappears when you leave JASS.** `CallOutBox::launchAsynchronously`
  runs a timer that dismisses the box as soon as the app is not the foreground process. That is
  right for a menu and wrong for this panel: it carries the rack height budget, a number one reads
  while doing something else in another window. It now closes on a click outside, on ESC, or on the
  MODULES button — not on losing focus.
- **The window fits the screen it is actually on.** The display-fit scale and the budget line both
  asked for the *primary* display, which is the same thing only on a single-monitor desk. Both now
  measure the display under the window, and dragging JASS onto a monitor of a different size or
  scaling re-fits it instead of keeping the old one's scale.
- **The budget line names the display it measured**, on a second row — usable area and scaling — so
  a number that disagrees with what you see can be diagnosed instead of guessed at. The
  over-budget warning moved there too — the panel is 300 px wide and the old one-line form ran off
  the edge unseen.
- **No more mojibake in the English budget line.** `juce::String(const char*)` takes plain ASCII, so
  the raw `·` and `—` in the English literals came out as garbage while the German ones — already
  declared UTF-8 — were fine.
- **Kunstkopf stopped stumbling on busy patches.** Every voice pans all nine generators
  every sample, whether or not their modules are on — a disabled generator just returns 0.
  In Kunstkopf that zero still went through the full 128-tap HRIR convolution, which is not
  free: one render costs ~1.3 % of a core, so nine generators across eight voices spent
  ~94 % of a core filtering silence, and the audio callback started missing its deadline.
  A single held note stayed clean, which is why it only showed up on the drum pattern. The
  panner now skips a render once its history holds nothing but zeros — exact, not
  approximate: measured 303 ns → 1.3 ns per silent sample with bit-identical output.

## [2026.08.6] – 2026-08-10

### Changed
- **The rack got wider instead of smaller, and only pays for the modules it
  shows.** The display-fit scale had run out of headroom: it sat at 0.65, which
  is exactly where the rack stops being readable, and it was derived from *every
  module that exists* — so hiding a module bought nothing while the type kept
  shrinking. Three things changed together. The floor is now derived rather than
  guessed (`1.0 / display->scale`, i.e. never render smaller than 1:1 in physical
  pixels — 0.667 on a 150 % desktop). The worst case now counts only the modules
  that **may appear**; revealing one can only add to that set, so switching
  presets still never moves the window, but hiding one gives its height back. And
  the grid went from 24 columns at 1520 px to **30 columns at 1920 px**: a column
  stays ~53 px, so no module changed physical size, but six more per row pack the
  rack two rows shorter. Height was the scarce dimension while more than half the
  screen width sat unused. Measured on a 3413×1440 desktop: 1980 px of rack at
  scale 0.65 → 1484 px at 0.85.
- **Seven modules start hidden**: SUB, CROSS MOD, GLIDE, PITCH ENV, BITCRUSH,
  FORMANT and PHASER are enabled in none of the presets we ship or keep locally,
  and rack height is now a budget worth spending on what is actually used.
  Nothing is lost — "hidden ⇒ silent" was already the invariant, and a preset that
  switches one of them on reveals it automatically. Show any of them at any time
  via the MODULES panel, which now also states what the current selection costs
  (`Rack 1484 / 1929 px · scale 0.85`) so the trade is visible instead of silent.
- **One width for every combo box.** A combo used to fill its cell, and a cell is
  module width divided by content — so the same control came out 106 px wide in MOD
  MATRIX and 120 px in FILTER, ragged across the rack for no reason. Combos are now
  capped and centred exactly the way every knob is capped at 62 px.
- **Three modules resized to fit their contents.** The LFOs went one column
  narrower, which is what packs MODULATION into three rows instead of four and buys
  most of the rack height above. MOD MATRIX went the other way: its cells were
  53 px, and a knob is capped to its cell, so its AMT knobs were visibly undersized —
  it now spans the 28 columns at which a cell reaches the standard 62 px. The
  SAMPLER's SET combo gave up the extra width it claimed for long set names; the full
  name still shows in the drop-down, and the freed slot makes the module's own knobs
  a little larger.

## [2026.08.5] – 2026-08-09

### Fixed
- **Shifting the computer-keyboard octave no longer interrupts playing.** Up/Down
  used to release every held note first, and worse, notes went permanently dead:
  the release ran through `MidiKeyboardState::allNotesOff()`, which silences the
  state but leaves `MidiKeyboardComponent`'s own `keysPressed` bitmask set for the
  old note numbers. Once the octave moved nothing mapped to those notes again, so
  the bits were never cleared — and returning to that octave found the bit already
  set and skipped the note-on. Every visited octave left dead keys behind.
  Computer-key playing is now handled by JASS instead of JUCE (whose implementation
  keys its state by note number and therefore cannot survive an octave shift): the
  note each PHYSICAL key started is remembered, so held keys FOLLOW the shift —
  their note moves with the octave and keeps playing, and the later release still
  lands on whatever note is actually sounding. Dead keys are structurally impossible
  now, however often you switch. Only notes JASS itself started are ever released,
  so external MIDI hardware and the auto-play drone are untouched.

### Changed
- **MOD MATRIX AMT steps in 0.001 instead of 0.01.** One AMT unit means
  something different per target: FILTER CUTOFF spans ±3 octaves, but FREQ spans
  ±1 octave — so a 0.01 step was 12 cents, and the entire useful range for an
  analog-style pitch drift (±10..25 cents) fell on two knob positions. Stored
  values are unaffected (no migration); the read-out gains a decimal.
- **Documentation caught up with epic 12** — `ARCHITECTURE.md` now describes
  background sample loading (why the store lock guards publication only, and
  what shields the LiveState while a set is in flight), the help panel opting
  out of the display-fit scale, and the sampler-preset trap that a preset load
  deliberately does not run the set-pick automation, so a sampler preset has to
  carry its own `Release`. Store caps in `DEVELOPER_GUIDE.md` corrected to the
  values story 12.5 raised them to; README mentions background loading and the
  GrandPiano slot.

## [2026.08.4] – 2026-08-08

### Added
- **Preset `GrandPiano` on F8** — the plain instrument: SAMPLER with the
  SplendidPiano set and nothing else in the signal path (no envelope, no
  effects, no modulation), output in Stereo-Pan so the stereo recording stays
  stereo. REL is carried in the preset (2.16 s), because a preset load
  deliberately does not trigger the set-pick automation that would otherwise
  set it — without it the notes would cut off.

### Changed
- **Samples load in the background** (Story 12.6). The app used to decode every
  installed set before showing its window — with both four-layer grand pianos
  that is ~1.2 GB, and a restored piano patch pushed the start to ten seconds.
  A loader thread now fills the SET combo while the app runs, and the set a
  preset asks for jumps the queue and is selected as soon as it is there.
  Startup with a piano patch: 10 s → 0.6 s. For a few seconds after launch the
  sampler is silent while its set loads.
- **The SAMPLER's REL knob dims for instruments that govern their own release**
  and then shows the release time actually being played instead of its own
  (inert) value — the same treatment ROOT already gets for multisample sets.
  Salamander carries `ampeg_release` on all 120 zones, so REL cannot do
  anything there; Splendid has it on its lowest regions only, so REL still
  matters for most of its keyboard.

## [2026.08.3] – 2026-08-07

Collects everything released as v2026.08.0 – v2026.08.3 (the automated
per-merge releases; the CHANGELOG is promoted in the release PR, not along
the way).

### Changed
- **Help text for the on-screen KEYBOARD** now explains why some three-note
  chords do not sound on the computer keyboard: ordinary keyboards scan their
  keys in a matrix without per-key diodes, so certain triples are ambiguous
  and the controller reports nothing at all — the third note never reaches any
  application. Shifting the octave moves the notes onto different physical
  keys; chords (and anything velocity-layered) want a MIDI keyboard.
- **JUCE 8.0.14 → 9.0.0.** JASS uses none of the APIs the major version breaks
  (no Drawable/SVG, no typeface-metrics calls, no multi-touch, no OpenGL), so
  the upgrade is a clean submodule bump — full rebuild passes with zero
  warnings. JUCE 9 brings a faster software renderer, which is exactly what
  the rack UI draws with.

### Fixed
- **Help text was too small to read, and long texts were cut off.** The panel
  is a child of the editor and inherited its display-fit down-scale, which
  rendered 14 pt body type at about 9 pt. It now cancels that transform and
  draws at true pixel size. Placement had to be corrected along with it: JUCE
  transforms a component's *whole* bounds, position included, so the magnified
  panel was pushed off the bottom right by the same factor and lost its last
  lines. A long text (MOD MATRIX, KEYBOARD) now gets a wider panel — up to
  760 px, fewer wrapped lines — and scrolls if it still exceeds the window.
- **The window changed size on every preset change.** Loading a preset reveals
  the modules it enables, and the display-fit down-scale was derived from the
  rack that happened to be visible — so a preset with more modules made the
  rack taller, the scale smaller and the whole window (width included) shrink,
  then grow back on the next preset. The scale is now computed once from the
  worst-case rack (`Rack::maxHeight()`, i.e. every module visible) and kept for
  the session: window width and module size are constant, only the height still
  follows the visible content.
- **Stereo samples comb-filtered in the Stereo-Pan output mode** ("metallic" tone on
  some piano keys): the sampler's L/R sub-sources sat at pan ±0.5, so equal-power
  panning mixed 38% of the opposite microphone channel into each ear — a coherent
  partial sum that cancels/boosts partials key-dependently (measured up to ±6 dB on
  the loudest partials of the Splendid Grand's A3). In gain-based stereo mode a
  stereo recording now renders like a stereo track (hard L/R, PAN acts as balance);
  the Binaural and Kunstkopf modes keep the ±0.5 placement — their sub-sources are
  decorrelated by ITD/HRIR, no coherent comb.

### Added
- **SAMPLER velocity layers** (Story 12.5) — `lovel`/`hivel` regions in an imported
  `.sfz` are real zones now: the key velocity picks the layer (soft hit → soft
  recording, with its timbre), and inside a layer the gain follows the touch
  (`amp_veltrack`, SFZ-spec default for .sfz sets; folder/single sets stay
  velocity-neutral as before). New opcodes `volume=` (layer balancing) and
  `tune=` (piano stretch tuning). Bounds raised for layered instruments: 60 min
  of audio per set, 4 GiB global sample budget.
- **Downloadable grand-piano packs** — `JASS-SplendidPiano.zip` (AKAI Steinway,
  samples Public Domain) and `JASS-SalamanderPiano.zip` (Alexander Holm's
  Yamaha C5, CC BY 3.0 with attribution file) as assets of the dedicated
  `piano-pack-v1` release; unzip into `%AppData%\JASS\Samples\`. The curated
  `.sfz` files live in `tools/piano-packs/`, and `tools/build_piano_zips.py`
  reproduces the zips from the upstream repos (nothing heavy enters git).
- **SAMPLER SET menu shows long set names in full** — the SET combo is wider now
  (rack combos can declare their layout width; everything else is unchanged), so
  user-named multisample sets like "SalamanderPiano" no longer truncate.
- **Picking a multisample set sets the SAMPLER up as an instrument** (user gesture
  only — preset loads keep their saved values): MODE → One-Shot (Loop would start
  notes at the shared loop phase, mid-sample), STRETCH → off (buys nothing at a
  couple semitones per zone), REL lifted off 0 to ~2 s (zones without their own
  `ampeg_release` get a fade), and — only when the SAMPLER is the sole active
  generator — ENVELOPE off (nothing cuts the sampler's own tail) and output mode
  → Stereo-Pan (the one mode that renders a stereo recording untouched).
- **SAMPLER release envelope** (Story 12.4) — the sampler fades released notes with
  its OWN release time instead of cutting them: an imported `.sfz` sets it per zone
  (`ampeg_release`, now read), the new **REL** knob covers zones without a value
  (0 = off, the previous behaviour and the default — old presets are unaffected).
  The ring keeps sounding under the fade, so fast playing, same-note retriggers and
  voice steals hit already-decaying material instead of a hard cut. Works with the
  ENVELOPE module off (simplest setup — the sampler governs its whole tail itself);
  with ENVELOPE on the ADSR shapes the voice on top (A 0 / D 0 / S max / R ≥ the
  longest fade). Sustain pedal (CC64) holds notes as before; the fade starts when
  the pedal lifts.
- **SAMPLER multisampling** (Story 12.2) — load a whole folder as ONE sample set:
  files named `<anything>_<note>` (`Piano_C3.wav`, `Pad_A#4.wav`, note names with
  C4 = middle C or MIDI numbers) are spread across the keyboard, each zone covering
  the range halfway to its neighbours. LOAD also imports a minimal **`.sfz`** subset
  (`<group>`/`<region>`, `sample`/`key`/`lokey`/`hikey`/`pitch_keycenter`). ROOT is
  inert (dimmed) for multisample sets — each zone brings its own root. New caps:
  60 s per file (unchanged), 5 min of audio per set, 32 sets, and a global RAM
  budget equal to the previous worst case. Presets reference multisample sets by
  name like single samples; sets live in `%AppData%\JASS\Samples\<SetName>\`.
- **SAMPLER load errors now name the file and the reason** (e.g. which file of a
  set is unreadable) instead of a generic limits message.
- **SAMPLER reads FLAC** (everywhere: LOAD, folders, .sfz references, preload) —
  the big free .sfz libraries (Salamander, Splendid Grand, …) ship FLAC and now
  load without conversion. Per-set audio cap raised 5 → 15 minutes so real
  single-layer chromatic pianos fit (the global memory budget stays the hard
  limit); overlapping velocity layers in an .sfz now keep the LOUDEST layer
  (`hivel` ranking) instead of whichever came first.
- **Two shipped multisample example sets** (seeded to `%AppData%\JASS\Samples`):
  **EPiano** (5 FM e-piano recordings C2–C6, mapped by the `Name_C3.wav` folder
  convention) and **Organ** (3 drawbar recordings mapped by the commented example
  `Organ.sfz` — a template for writing your own).
- **`tools/get_iowa_piano.py`** — builds an optional real-piano multisample set
  (University of Iowa Steinway recordings, free without restrictions) with one
  command; README documents it plus the manual route to the Splendid Grand.
- **SAMPLER STRETCH mode** (Story 12.3) — pitch/time decoupling: the key sets only
  the pitch, SPEED only the tempo, so loops keep their rhythm on every key and all
  loop voices stay beat-locked regardless of pitch (the tape-mode hard resync
  becomes unnecessary by construction). Engine: **Signalsmith Stretch** (MIT,
  vendored under `Source/ThirdParty/signalsmith-stretch/`), chosen by a measured
  bake-off (~35–40 dB spectral SNR at ±7/±12 st vs. negative SNR for a naive
  granular; ~19 % of one core for 16 stereo voices). The engine's ~60 ms
  warm-up is pre-computed at note-on (`outputSeek`, measured 0.57 ms/voice),
  so attacks stay immediate even when playing fast. Off by default — existing
  presets and the classic tape behaviour are unchanged.

- **Developer documentation** — three new docs linked from the README:
  `docs/ARCHITECTURE.md` (layers, signal flow, threading/RT-safety, state, UI),
  `docs/MODULE_SYSTEM.md` (declarative module-spec system + extension recipes)
  and `docs/DEVELOPER_GUIDE.md` (build, dependencies, versioning/CI, all
  configuration surfaces, compile-time tunables, build gotchas).

### Changed
- **SAMPLER STRETCH toggle** moved next to MODE and now renders like every other
  control (name caption above, checkbox below) — the button-side label was
  unreadable between the knobs.
- **SAMPLER shared loop clock runs only while voices are sounding** — during
  silence it parks at the region start, so the first note after a pause always
  plays the sample's attack; simultaneous/overlapping loop notes still join the
  running round (beat-lock preserved).
- `docs/JASS_Preset_Format.md` updated to the current FormatVersion 6 (was
  stale at v4) with the full v3→v6 version history.

### Fixed
- **`Samples/Talkbox.wav` was MS-ADPCM-compressed** — JUCE cannot decode that,
  so the shipped sample had never actually loaded (silently skipped since its
  introduction). Re-encoded as 16-bit PCM; it now appears in the SET list.
- **Rejected folder/`.sfz` imports no longer leave a dead copy** in
  `%AppData%\JASS\Samples` (imports now validate before copying — a leftover
  folder would have silently failed at every startup preload).

### Removed
- `docs/Modul_Architektur_Konzept.md` — the 2026-07-18 design draft is
  superseded by `docs/MODULE_SYSTEM.md`, which documents the *implemented*
  state (the draft's `legacyKey`/array-persistence proposals were never built).

## [2026.07.15] – 2026-07-30

Collects everything released as v2026.07.1 – v2026.07.15 (the automated per-merge
releases since 2026.07.0; the CHANGELOG had not been promoted along the way).

### Changed
- **README** — SAMPLER moved under *Sound sources*; new *Input devices* section
  (on-screen keyboard + MIDI keyboard/controller).
- **Local dev builds** now default to CalVer `2026.7.15` (header display); release
  builds keep getting their exact per-merge CalVer from CI.

### Added
- **SAMPLER module** (Story 12.1) — play your own recordings (WAV/AIFF, ≤60 s, up to 32
  loaded) as a sound source through the whole JASS chain (filter, wavefolder, mod matrix,
  arp, PAN, binaural modes). ROOT/START/END, modes One-Shot / Loop (crossfaded) / Reverse /
  Rev-Loop, 4-point Hermite interpolation (measured: +10 dB SNR over linear). **Stereo
  files stay stereo** — L/R render as two placed sub-sources around PAN (own binaural/HRTF
  placement each); mono downmix only in the mono output modes. LOAD copies files into
  `%AppData%\JASS\Samples`; presets re-resolve the sample **by name** across sessions.
  Ships with an example catalog (`Samples/`, embedded + seeded + pre-loaded into the SET
  combo at startup). A **SPEED** knob (0.25×–4×) multiplies the playback rate tape-style
  on top of the key. LEVEL and PAN are mod-matrix targets. Append-only, FormatVersion
  stays 6.
- **MOD MATRIX: slot LEDs moved** — each routing strand's green activity dot now sits
  directly before its AMT knob (was at the strand's left edge).
- **Stereo displays, final-output tap** — OSCILLOSCOPE and SPECTRUM now show the **true
  final output** (after compressor, stereo/binaural modes, ROOM and master volume) instead
  of the old dry pre-bus mono mix. The scope draws **L and R side by side** (blue /
  orange, a colorblind-safe pair), the spectrum overlays **two coloured curves** in one
  diagram (violet / orange). Effectively-mono signals collapse to a single plot/curve
  automatically — so the spatial stages are literally visible.
- **Kunstkopf externalization: ROOM knob** (STEREO module, Story 10.4) — a shared binaural
  **early-reflection** stage on the bus, active only in Kunstkopf mode. Six non-harmonic
  taps (8–24 ms) rendered through lateral KEMAR ears push the image **out of the head** —
  the cue dry binaural cannot deliver, and the axis on which Kunstkopf is now audibly
  different from the parametric Binaural mode. The knob is a **5-detent room macro**
  (wet level −3…+6 dB **plus** a damping morph 5→10 kHz per step) — deliberately coarse
  and ear-calibrated: the ear's direct-to-room JND is ~5–6 dB (Zahorik 2002), so a fine
  or wide-range knob feels dead. The **centre detent (= the default) is the ear-tested
  optimum**; the upper half goes beyond it (at the stop the room carries twice the direct
  power). Level-neutral by constant-power normalisation with per-detent measured
  constants (±0.35 dB); the other four output modes stay bit-exact (FormatVersion stays
  6, append-only). RANDOM now leaves the output MODE and ROOM untouched (master-bus rule;
  MODE had been missed in 10.1).
- **Spatialization / STEREO output modes** — a per-generator **PAN** feeds a new STEREO
  output stage with five modes: **Mono**, **Pseudo-Stereo** (the existing Haas widener,
  still default), **Stereo-Pan** (true amplitude L/R), **Binaural** (parametric headphone
  3-D: ITD + head-shadow) and **Kunstkopf (HRTF)** — real out-of-head placement by
  convolving each generator with the measured **MIT KEMAR** head impulse response for its
  PAN azimuth (embedded, no external assets; headphones only). PAN is also a mod-matrix
  target, so any source can **auto-pan** a voice in 3-D. Append-only — old presets load
  unchanged (FormatVersion stays 6). See the STEREO module's info for mode details, and
  the [License](README.md#third-party-data) for the KEMAR attribution.
- **MOD MATRIX destination = MODULE → PARAM** — the DEST is now chosen in two steps
  (a **MOD** combo, then a **PARAM** combo whose items follow the picked module). Both lists
  are sorted A→Z. PARAM labels match the target module's own knobs (FREQ, CUTOFF, DRIVE, …),
  removing the old abstract names (e.g. "Pitch" → OSC · **FREQ**).
- **Per-oscillator modulation** — FREQ / AMP / DETUNE / FB / VOICES can target a SINGLE
  oscillator (OSC 1/2/3) instead of all at once; "Alle OSC" keeps the classic global
  behaviour. A per-OSC routing auto-enables just that oscillator and lights only its ring.
- **Full per-module target coverage** — essentially every continuous knob is now a matrix
  destination: WAVETABLE FREQ/AMP/VOICES/DETUNE, FILTER RESO, FORMANT RESO/MIX, WAVEFOLD
  SYM/MIX, DISTORTION MIX, BITCRUSH BITS/RATE, CHORUS RATE/MIX, DELAY FB, REVERB ROOM/DAMP,
  OSC FB/VOICES — each with its own live ring.
- **MOD MATRIX grown to 8 routing slots** (was 6), full-width layout.
- **PHASER added as a matrix destination** (RATE · DEPTH · FB · MIX) — the last effect
  module that was missing from full coverage; auto-enables + rings like the others.
- **FREQ modulation clamped** to ±4 octaves (OSC/Alle-OSC/WAVETABLE) so stacked slots
  can't drive the pitch into absurd, aliased territory.
- Preset **FormatVersion 6** with automatic migration (`.jass` files): the legacy single
  "Target" per slot converts to MODULE + PARAM (global Pitch/Amp/Detune → "Alle OSC"), and
  the A→Z reorder remaps the persisted PARAM index. Older presets are backed up before upgrade.
- **Modulation-matrix target expansion** — 8 new per-voice destinations: Delay Time,
  Delay Mix, Reverb Mix, Chorus Depth, Dist Drive, Bitcrush, Sub Level, Detune. Each
  shows a live modulation ring and (except Detune) auto-enables its module when routed.
- **Single-source target catalog** (`Source/DSP/ModTargets.h`, X-macro) — the enum,
  persist strings, DEST labels and enable-map are generated from one table.
- **Demo preset "FX Motion"** — 4 LFOs breathing delay/reverb/chorus/detune.

### Changed
- **Kunstkopf (HRTF) no longer colours the sound.** The raw MIT KEMAR kernels turned out to be
  unusable as-is: the *frontal* response — the one playing at pan centre, where no spatial effect is
  wanted at all — is a 21.6 dB bandpass (no bass, because the 1994 measurement speaker had none, plus
  average pinna resonances that the listener's own ears then apply a second time over headphones).
  The kernels are now post-processed offline in `tools/gen_kemar_hrir.py`, so the runtime cost is
  unchanged: the frontal response is equalised out (pan centre becomes transparent — 4.2 dB
  peak-to-peak, from 21.6), the low end is replaced by a flat correctly-delayed synthetic one, and
  every azimuth pair is level-normalised. Verified: the localisation cues survive intact (worst ITD
  error 23 µs, worst per-frequency ILD error 1.9 dB).
- **All five output modes are now level-matched**, so switching modes changes the image and not the
  loudness. Kunstkopf was 4.6 dB quiet (pink-weighted) and **Binaural** was 3 dB hot — it drove both
  ears at unity at centre instead of 0.707, which flattered it in any A/B for no reason but level.
- **STEREO WIDTH/TIME are greyed out outside Pseudo-Stereo**, and the seven per-generator **PAN**
  knobs are greyed out in Mono and Pseudo-Stereo — the modes in which the voice renders
  single-channel and the pan value is never read. Previously these knobs looked live while doing
  nothing. (New per-knob `Knob::activeWhen` predicate in the rack descriptor.)
- STEREO help (EN+DE) now states what actually distinguishes the modes — Binaural is deliberately
  exaggerated (everything pans, bass included), Kunstkopf is physically faithful (bass stays centred)
  — and warns that with every generator centred those modes are *identical by construction*, since
  there is no direction to render.
- **DELAY** and **LFO** control order aligned to the module-wide convention
  (selector combos first, then knobs): DELAY = SYNC·TIME·FB·MIX, LFO = WAVE·SYNC·RATE·DEPTH.
- Zone help (EN+DE) explains the Modulation-vs-Processing distinction (audio vs. control).

- **CI release pipeline** (`.github/workflows/release.yml`): on every merge to `main`,
  derive the next CalVer, build **Windows + Linux** artifacts (Standalone + VST3), and
  publish them to a GitHub release. Prominent **Download** link in the README →
  `releases/latest`.
- **Versioned no-direct-push hook** (`.githooks/pre-push`) with `git config core.hooksPath .githooks`.

## [2026.07.0] – 2026-07-20

First versioned release. Summarises the project's notable state up to this point.

### Added
- **19″ rack UI** — every module (sources, modulation, processing, visualization,
  keyboard) rendered from declarative descriptors in zones, with per-module and
  per-zone **enable / reset / info** (context help, EN/DE).
- **Rack customization** — show/hide and reorder modules by drag & drop; the layout
  is persisted in the preset and resettable to the factory arrangement.
- **Modulation matrix** — 6 slots, free source→target routing with amount; routing
  auto-enables the source and the target module.
- **4 LFOs** with tempo sync; **cross-mod** (additive / ring / FM between selectable
  sources); **self-FM** feedback per oscillator; **pitch envelope**; **poly glide**;
  **arpeggiator**.
- **Effects & processing** — biquad filter, formant filter, distortion, wavefolder,
  bitcrusher, compressor, phaser/flanger, delay (tempo-synced), chorus, reverb,
  pseudo-stereo master stage.
- **Wavetable oscillator** — 6 built-in banks plus example WAVs (embedded, seeded on
  first run), position morph, WAV import.
- **3D spinning JASS logo** in the header (toggle via right-click).
- **App version (CalVer)** shown in the header subtitle and in the right-click title
  info menu (alongside the loaded preset's format version).
- **CHANGELOG.md** (this file).

### Changed
- **Module-spec architecture** — "one module = one place": each module declares its
  parameters and rack layout in `Source/Modules/<Name>Specs.h`; APVTS params and rack
  descriptors are generated from those specs.
- **Preset format** moved to a **nested, per-module JSON** layout (`FormatVersion 4`),
  extension `.jass`, under `%AppData%\JASS`. A one-time migration copies an existing
  legacy `%AppData%\Synthy` folder on first run.
- **RANDOM** now includes the modulation matrix (unique targets, tamed frequency/filter
  ranges) and leaves the whole Master Bus untouched.

### Fixed
- Preset migration bugs from the format rework (a migrated modulation slot left muted;
  a cross-mod auto-enable turning a patch into noise).
- Preset **migration is now dependable**: loading an older-format preset via the LOAD
  dialog backs up the original to `PresetsBackup/`, upgrades it in place, and a corrupt
  or non-JASS file now **fails with a visible message** instead of silently resetting to
  defaults.
- Various UI/audio fixes: hung notes on octave switch and with the envelope off,
  proportional mouse-wheel steps, oscilloscope time-base range, formant defaults.

## Releasing

Work reaches `main` **only via pull requests** — no direct pushes (enforced locally by
`.githooks/pre-push`). **Each PR merged to `main` bumps the CalVer**, and the CI pipeline
(`.github/workflows/release.yml`) does the rest **automatically**:

1. Move any `Unreleased` notes into a new `## [YYYY.MM.MICRO] – YYYY-MM-DD` section as part
   of the PR.
2. On merge, the pipeline derives `YYYY.MM.MICRO` (current year/month; `MICRO` = existing
   `vYYYY.MM.*` tag count), builds Windows + Linux artifacts, and creates the tag +
   GitHub release with them attached. No manual tagging needed.

Manual fallback (if the pipeline is unavailable), from `main` after merge:
```powershell
git tag vYYYY.MM.MICRO
git push origin vYYYY.MM.MICRO
gh release create vYYYY.MM.MICRO --title "vYYYY.MM.MICRO" --notes-from-tag
```
The repository is **private**, so releases are visible only to collaborators.
