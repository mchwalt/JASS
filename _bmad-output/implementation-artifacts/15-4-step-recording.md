# Story 15.4: Write the figure by playing it

Status: **draft** — raised by the maintainer 2026-08-11: "statt beim Programmieren der Note am Knopf
zu drehen, könnte man die Steps von Anfang an durchgehen und die Note per Keyboard einspielen."

## Story

As someone writing a figure into STEP SEQ,
I want to play the notes on the keyboard while the sequencer walks through the steps,
so that I write the figure the way I hear it instead of translating it into semitone numbers first.

## Why

Story 15.3 made a step **audible**; this makes it **playable**. Turning 32 knobs is an act of
translation — you know the interval you want, and you convert it into a number the knob understands.
Every hardware sequencer since the SQ-10 solves this the same way: the machine offers a step, you
play a note, it moves on. The conversion is what the machine is for.

## The model, as the maintainer specified it

- **The reset button ↺ starts the recording.** It clears the figure and arms step entry at step 1,
  and the recording ends by itself once all steps up to **LEN** have been written. No new control:
  the button that empties the pattern is exactly the moment one wants to fill it again.
- **A click on a step's knob selects it**, highlights it, sounds its current note, and makes it
  writable from the keyboard for as long as it is highlighted. That is 15.3's click plus a
  highlight — the audition is already there.
- **The highlight is the only new visual.** One cell, one ring.

## Acceptance Criteria

1. **↺ clears the figure and arms recording** from step 1, for LEN steps. Reset keeps doing
   everything it does today (it is the rack-wide reset: every parameter of the module back to
   default) — arming is an addition, not a replacement.
2. **A played note writes the highlighted step** and advances the highlight to the next one. The
   value written is `note − 12 × kbBaseOctave` clamped to ±24 — the **same reference the 15.3
   preview plays**, so what you hear when turning a knob and what you get when playing a key agree,
   and both follow the Up/Down octave keys.
3. **Writing a step switches it on** (its rest switch goes to on), so a figure entered by playing
   sounds without a second pass.
4. **The keyboard writes, it does not trigger.** While recording is armed the sequencer must not
   take the played notes as its root — otherwise every entered note restarts the figure and
   transposes it. Recording and playing are separate acts: write first, then hold a key and listen.
5. **A rest is a step you skip.** SPACE advances the highlight without writing. (Chosen over "a key
   that means silence" because every key already means a note.)
6. **Recording ends** when the step after LEN would be reached, or when the module is switched off,
   or when the user clicks a knob (which selects that step instead — see 7).
7. **Clicking a step's knob highlights it, sounds it, and makes it writable.** Playing a note then
   replaces that step and moves on to the next, so a correction can run on into its neighbours.
8. **The highlight is visible at a glance** — a ring around the cell, in the module's own colour,
   not a colour used for anything else in the rack.
9. **No new parameter.** The cursor is UI state; nothing about it belongs in a preset.
10. Help texts `Resources/{EN,DE}/stepseq.md` extended, both languages in one pass.

## Dev Notes

- **The highlight** wants a per-knob predicate polled in the frame timer, next to `activeWhen` and
  `toggleParamId` — e.g. `rack::Knob::highlightWhen`, injected by the editor which owns the cursor.
- **Capturing the note**: the editor becomes a `juce::MidiKeyboardState::Listener` (the processor
  already is one). That catches both the computer keys and a MIDI keyboard, which is the point —
  entering a figure on the Roland is the whole appeal.
- **AC4 needs a flag the processor reads**: while armed, treat the STEP SEQ's held-note search as
  finding nothing. An atomic set by the editor, read in `processBlock`.
- Reuse `auditionStep` for every preview here; do not build a second sound path.
- No unit-test rig: build, run, and the maintainer's ear.
- Feature branch, **no push, no merge** — the maintainer decides.

## Follow-up, not this story

- The same treatment for PERC would mean playing the *kit* into the grid (hit a pad, place a hit).
  Worth having, but PERC's grid is a different widget and the lane/step cursor is two-dimensional.
