# Story 15.3: Hear the step you are editing

Status: **draft** — raised by the maintainer 2026-08-10, right after 15.1 shipped: "um den
Sequenzer programmieren zu können, muss man am jeweiligen Step den Ton hören."

## Story

As someone writing a figure into STEP SEQ,
I want the step to sound while I turn its pitch knob,
so that I can find the interval by ear instead of writing the pattern blind and playing it back to
find out what I wrote.

## Why

A step's parameter is a *number of semitones*, not a note. Today the only way to learn what
`+15` sounds like over `+12` is to hold a key and listen to the whole pattern go by — which is fine
for checking a figure and useless for writing one. Every hardware sequencer this module is modelled
on solves it the same way: the step you touch, you hear.

## Scope

**In:** the pitch knob of a step sounds its note while being edited; the step's rest switch sounds
the note when it is switched back on.

**Out:** a general "audition" facility for other knobs, a step-advance button, playing the pattern
without holding a key. This is one gesture on one module.

## Acceptance Criteria

1. **Turning a step's pitch knob sounds the step.** The note re-triggers on every semitone, so
   dragging across the range scrubs a chromatic scale, and it stops when the knob is released.
   **A click sounds it too**, without changing the value (added by the maintainer 2026-08-10) —
   asking what is on a step is the more common question than changing it.
   **The wheel counts in single semitones**: at ±24 the knob was wide enough to fall through
   `SynthySlider`'s discrete-range branch and moved two semitones per notch, reachable in ones only
   with Shift held. A semitone knob is discrete however wide it is.
2. **Reference pitch is the computer keyboard's current C** — `12 × kbBaseOctave`, i.e. **C3 (MIDI
   48)** by default, and it **follows the Up/Down octave keys** (maintainer's call 2026-08-10:
   "C3, C2, C4 wandert mit der Hoch/Runter Taste mit"). With the ±24-semitone range that is C1…C5 at
   the default octave.
3. **It sounds while STEP SEQ is enabled** — which is the whole point, and the one thing that does
   not work by itself: the processor drops *every channel-1 note* while the sequencer runs
   (`PluginProcessor.cpp` ~line 895), so an audition sent down the normal keyboard path would be
   swallowed exactly when it is needed. It rides its own MIDI channel, as the auto-play drone does.
4. **Switching a rest back on sounds that step once**, so re-enabling a step tells you what you just
   brought back. The note releases itself (no gesture to end it).
5. **No note is ever left hanging**: a safety cutoff releases the audition ~0.8 s after the last
   change, and the editor releases it on teardown.
6. **A preset load must not play anything.** The knobs move when a preset arrives (host automation
   does the same), and `Slider::onValueChange` cannot tell that from a drag — so the audition fires
   only when the mouse is actually on that knob.
7. **The audition is not a played note**: it must not register as a held key (it would become the
   sequencer's root), and it must not drive the FREQ-knob display. It *does* silence the auto-play
   drone, like any other note — otherwise you audition against a droning C4.
8. **No new parameter, no preset change, no format bump.** This is UI behaviour only.
9. Help texts `Resources/{EN,DE}/stepseq.md` get one line each, both languages in one pass.

## Dev Notes

- **Channel.** The drone owns 16 (`kDroneChannel`), played notes are 1. Audition takes **15**. Only
  channel 1 is filtered by ARP/STEP SEQ, so 15 passes through untouched. Add it to the keyboard's
  display mask (`setMidiChannelsToDisplay`) so the audited key lights up on the on-screen keyboard.
- **Velocity 100/127**, the same velocity `StepSequencer` emits, or the audition is louder than the
  step it previews (velocity tracking is on by default in SFZ sets — the `Drum Pattern` preset had
  to compensate 4.2 dB for exactly this).
- **Where the hook goes.** `rack::Knob` gets an optional `audition` callback, injected by the editor
  in `buildRack` next to the existing `toggleParamId` injection — the editor is the only place that
  knows both the keyboard state and the current octave. `ModuleFrame` chains it onto the existing
  `onValueChange` (which writes the parameter back — do not replace it) and releases on `onDragEnd`.
- **Timer.** The editor already runs at 30 Hz; the safety cutoff is a tick counter, no new timer.
- Build with `/t:Rebuild` (core structs are embedded by value in `SynthVoice`).
- No unit-test rig: verification is build + running app + the maintainer's ear.
- Feature branch, **no push, no merge** — the maintainer decides.

## Dev Agent Record

Implemented 2026-08-10 on `feat/step-audition`. Files: `Source/PluginProcessor.{h,cpp}`,
`Source/UI/PluginEditor.{h,cpp}`, `Source/UI/rack/{ModuleDescriptor.h,ModuleFrame.cpp}`,
`Resources/{EN,DE}/stepseq.md`, `CHANGELOG.md`.

Two things the plan did not spell out and the implementation had to decide:

- **The rest switch shares the pitch knob's cell**, so its audition reads the knob's value rather
  than a parameter of its own — the same `audition` callback, called with the knob's current
  semitones and no matching release.
- **`isMouseOver` is the gesture test**, not `isMouseButtonDown`: the mouse wheel changes a knob
  without a button ever going down, and dragging keeps the button captured even when the pointer
  leaves the cell. Both cases are covered by asking for either.
- **A click may not be released at the end of its gesture.** Press and release are milliseconds
  apart, so closing the note on `onDragEnd` would make a click inaudible. The gesture remembers
  whether the value ever moved: a drag closes its own note, a click rings on into the timeout. The
  right mouse button never sounds — `SynthySlider` opens the value box there and never starts a
  drag, so no hook fires.
