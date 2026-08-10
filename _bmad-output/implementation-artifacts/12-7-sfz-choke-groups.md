# Story 12.7: SFZ choke groups — a closed hi-hat that actually silences the open one

Status: ready-for-dev — raised 2026-08-10 while picking a drum kit for the STEP SEQ demo.
Independent of that pack: the kit plays fine without this, the hi-hats just pile up.

## Story

As someone programming a drum pattern,
I want a step that triggers the closed hi-hat to cut the open one that is still ringing,
so that a kit sounds like an instrument instead of two cymbals played at once.

## Context

On a real kit an open hi-hat cannot keep ringing once the pedal closes. SFZ expresses that with two
opcodes: a region declares `group=N` ("I belong to group N") and another declares `off_by=N` ("when
I sound, silence everything in group N"). Every drum kit uses it for the hats, most also for a
choked crash.

`SampleMapping` reads `lokey / hikey / lovel / hivel / pitch_keycenter / offset / ampeg_release /
amp_veltrack / volume / tune` and **ignores everything else, silently** — including `group` and
`off_by`. So the open hat keeps ringing under the closed one. It does not read as a groove, it reads
as a mistake.

The reason it was never built is structural, not an oversight: a choke acts **across voices**.
`SamplePlayer` lives by value inside `SynthVoice` and knows only its own note — which is exactly what
makes it RT-safe and simple (Story 11.1). No voice can see its siblings today.

## Acceptance Criteria

1. **Parser**: `group=` and `off_by=` are read into `SampleZone` as two ints (0 = none), inherited
   from `<group>`/`<global>`/`<master>` scopes like the existing opcodes. Unparsable values are
   ignored, matching how the file already treats bad input.
2. **Cross-voice choke at note-on**: when a starting note's zone declares `off_by=N`, every OTHER
   voice whose currently-playing zone has `group == N` is silenced. Allocation-free and lock-free on
   the audio thread — a loop over the voice list, nothing more. The voices need sibling access for
   this (the processor owns the `juce::Synthesiser`); pick the narrowest form that works rather than
   handing every voice the whole synth if something smaller will do.
3. **It fades, it does not cut.** A hard stop clicks — that lesson is already paid for (12.4, the
   retrigger declick). Reuse the existing release ramp at a few milliseconds; do not add a second
   mechanism.
4. **A zone with no `off_by` behaves exactly as today** — byte-identical for every set we ship and
   both piano packs, which use neither opcode.
5. **Self-choke is not a thing**: a region whose own `group` equals the `off_by` it triggers must not
   silence itself, or a repeated closed hat would cut its own tail on every hit.
6. Verified by ear on the drum pack: open hat, then closed hat a fraction of a beat later — the open
   one must stop, without a click. And a full pattern where hats alternate fast, to check that the
   ramp never becomes the audible artefact.

## Dev Notes

- `SampleZone` is embedded in the never-freed store and its pointers are cached across blocks by
  voices — adding two ints is safe, but rebuild with `/t:Rebuild`: core structs are embedded by value
  in `SynthVoice` and a stale TU has produced 0xC0000005 at startup before.
- `zoneFor()` is already audio-thread callable (linear scan, no allocation); the choke check can use
  the zone the voice is actually playing rather than re-resolving anything.
- Timing: the choke should land on the same sample as the note-on that caused it. `startNote` is the
  sample-accurate hook; doing it per block in the processor instead would quantise the choke to the
  block size, which is precisely the wrong place to be sloppy.
- Files (expected): `Source/DSP/SampleMapping.h`, `Source/DSP/SampleBank.h` (the `SampleZone`
  fields), `Source/DSP/SamplePlayer.h`, `Source/Audio/SynthVoice.{h,cpp}`, `Source/PluginProcessor.cpp`,
  `docs/ARCHITECTURE.md` (the SFZ subset is documented there), `CHANGELOG.md`.
- No unit-test rig: verification is build + running app + the maintainer's ear.
- Feature branch, no push, no merge.

## Dev Agent Record

_Not started — story recorded 2026-08-10._
