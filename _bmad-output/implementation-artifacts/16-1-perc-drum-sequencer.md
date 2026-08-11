# Story 16.1: PERC — a drum sequencer on the master bus

Status: **draft** — raised 2026-08-10 by the maintainer: the `DAF Bass` wants a hard percussion beat
underneath it. The first question was whether that needs a second STEP SEQ. It does not, and the
reason decides the whole design.

## Story

As someone playing the sequencer bass,
I want a percussion pattern running underneath it, on its own four tracks,
so that JASS plays a complete piece by itself instead of a bass line looking for a drummer.

## Why a second STEP SEQ cannot do it

**JASS is monotimbral.** `SynthVoice::startNote` starts *every* enabled generator for *every* note,
and the whole effect chain — filter, distortion, phaser, delay, chorus, reverb, formant — lives
**per voice** as a `ChannelStrip` (`Source/Audio/SynthVoice.h:99-107`). Only the compressor and the
master gain are global.

So a drum note emitted by a second note sequencer would come out as *sawtooth plus drum sample*,
through the bass's resonant lowpass, swept once per step by the tempo-synced LFO. Exactly what a
hard beat cannot survive. The sequencer is the cheap part; separating the sound layer is the
expensive one.

Three ways were weighed with the maintainer:

| | verdict |
|---|---|
| Two JASS instances in REAPER | Works **today**, and is the boundary Story 15.1 drew on purpose. Kept as the fallback, not as the answer — the standalone is the focus. |
| **PERC renders straight to the master bus** | Chosen. Separation is *structural*: no shared filter, no shared FX, because it never enters a voice. "Dry" is the construction, not a setting. |
| Real bi-timbrality (track A/B, generators per track, second strip) | **Buried, and not for reasons of effort.** See below. |

### Why real bi-timbrality stays buried (decided 2026-08-10)

"Bi-timbral" is three independent problems: *which generators a note starts*, *which notes reach
which layer*, and *which signal path each layer takes*. The third one kills it. Filter, distortion,
phaser, delay, chorus, reverb and formant live **per voice**, so a second timbre with its own chain
means every processing module twice — twice the parameters, twice the controls. Two numbers decide
it:

- the rack has **245 px of height left** after PERC (measured, see below) — a second set of modules
  is not physically representable;
- the alternative, an A/B switch on every module that redirects its knobs to a second parameter
  set, is a rework of the module system **and a preset format break**.

The chosen design does not answer that question — it **removes** it: layer B goes to the bus
**dry**, so there is no second chain to argue about. That is not half of bi-timbrality; for the case
being built it is all of it.

### PERC is layer B, not a drum machine (maintainer's framing, 2026-08-10)

The maintainer's own reading — "a second SAMPLER instance, only for the percussion" — is the better
one and is adopted. One precision matters in code: **that instance must not be a voice generator.**
Today's SAMPLER sits inside `SynthVoice`, so a second one there would be started by every note and
we would be back at the beginning. It lives at **processor level**, a bus instrument the PERC
sequencer strikes.

Consequence for the implementation: build the playback on the **existing SFZ/sampler substrate**
rather than as special-purpose drum code. Sets, velocity layers, background loading and the parser
come along for free, and layer B can later carry any sampled instrument instead of only a kit. For
the player it is a drum machine; for JASS it is a dry second layer.

## Scope

**In:** one pattern · 4 tracks · 32 steps with LEN · one instrument and one level per track · its own
kit · its own clock on the shared tempo · runs without a held key · rendered dry into the master bus
· stored in the preset.

**Out:** per-step velocity or accents, flams, per-track choke groups (that is Story 12.7), swing,
pattern chaining, and any routing of PERC through the voice effects. Reverb on the snare is a
separate ingredient and not this story.

## Acceptance Criteria

1. **New module `PERC`**, `persistObject "Perc"`, enable param `percOn` **default off**,
   `defaultVisible = false`. A preset saved before this story loads bit-identical.
2. **Four tracks.** Each carries 32 step switches, one **NOTE** (which instrument of the kit it
   fires) and one **LEVEL**. Tracks sound **simultaneously** — that is the whole reason for four of
   them; kick and hat on the same step must both be heard.
3. **32 steps with `LEN` (1…32, default 16)**, exactly like STEP SEQ. Reasoning, since it differs
   from the 16 that were first considered: more steps cost *width*, not height (a step is a small
   box, not a 62 px knob), so 32 buys the two-bar pattern — the fill in bar two — for nothing. LEN
   16 on sixteenths is the classic one-bar drum loop.
4. **Its own clock**, a `SYNC` combo fed verbatim from `SyncDivision::kNames` (default **1/16**)
   plus a free-running `RATE`. Same tempo source as the LFOs, DELAY and STEP SEQ, so a bass figure
   on eighths and a beat on sixteenths cannot drift apart.
5. **It runs without a held key** (maintainer: "hier brauchen wir wieder die auto-drone, die auch
   ohne gedrückte Taste läuft"). Structurally free: the module renders to the bus and needs no
   voice, hence no root note and no key.
6. **The bass enters on the next pattern start of the drums.** With PERC running, a STEP SEQ that
   starts from silence waits for the next step 0 of the PERC pattern instead of starting where the
   key happened to fall. At LEN 16 on sixteenths that is at most one bar. Rejected alternatives:
   starting immediately (the two never line up again) and restarting both on every key (the drums
   stumble at each note). With PERC off, STEP SEQ starts immediately, exactly as today.
7. **Dry into the master bus.** Mixed into the buffer after `synth.renderNextBlock` and *before* the
   compressor, so the master glue and MASTER level still apply and nothing else does.
8. **Its own kit**, independent of the SAMPLER's set — otherwise a sampled instrument and a drum kit
   could not coexist. Same sample store and the same background loading as Story 12.6; a kit that is
   still loading simply does not sound yet.
9. **RT-safe:** no allocation on the audio thread, the pattern is a fixed array, voice-free
   one-shot playback per track (a new hit on a track replaces the one still ringing).
10. **Persistence** append-only, no `kFormatVersion` bump. ~140 new APVTS params (128 steps + 8 per
    track + 4 globals) — the largest single addition so far, in their own parameter group.
11. **Help texts** `Resources/{EN,DE}/perc.md`, terse, both languages in one pass.
12. **The acceptance test is the beat under the bass**: `DAF Bass` on F9 plus PERC running, judged
    by the maintainer's ear. There is no test rig.

## Rack footprint — measured, not estimated

One rack unit is 114 px (`Rack::kHu`), the grid is 30 columns at a 1920 px design width. Measured on
the maintainer's machine (5120×2160 at 150 % ⇒ 3413×1440 logical, full work area):

| | px |
|---|---|
| Rack with STEP SEQ visible | 1732 |
| Budget at the 1:1 floor (`minScale = 1/1.5`) | 1929 |
| **with OSCILLOSCOPE + SPECTRUM hidden** | **1446** |
| free | **483** |
| PERC as `H2` | 238 |

So PERC is built **normally, `W20H2`** — no squeezing it into a single unit, and ~245 px still
spare afterwards. That headroom exists only because hiding the two visualizers now frees their
height for real (`ModuleDescriptor::visualOnly`, PR #43) — **this story depends on that landing.**

Row 1 is the step grid, row 2 the eight per-track knobs plus KIT, SYNC, RATE and LEN.

## The body cannot use the standard cell grid

STEP SEQ fits the module grid because 32 knobs at ≥62 px per cell are what the grid is for. 128 step
switches are not: at 30 px per step a track row is ~960 px wide and ~22 px tall. The grid would give
each switch a 62 px cell and the module would need six rack units.

So the step field is a **custom body component** delivered as a `Display` element (the same route the
on-screen keyboard takes), which **paints** the 4 × 32 field and hit-tests clicks rather than owning
128 child buttons. It writes the APVTS params directly, like any other attachment.

## Decided with the maintainer (2026-08-10)

1. **A track picks its instrument with a NOTE knob**, displayed as a note name (C1, D#1, …). A combo
   reading "Kick / Snare / Hat" would be nicer, but SFZ regions carry no names — only key numbers —
   so the list would have to be invented per kit and would lie for every other one. `Drum Pattern`
   already picks its instruments by note.
2. **The module's enable switch IS the transport.** One lit module, one running pattern, no second
   state to explain. (Rejected: a separate RUN button leaving the module enabled but silent.)
3. **Zone MODULATION**, next to STEP SEQ. A `PERCUSSION` zone was considered and dropped: a zone for
   one module is an empty drawer, and the whole point of the layer-B framing is that PERC is not a
   new category but a second instance of something that exists. If layer B ever carries more than a
   kit, that is the moment for a zone — it will have contents then.

**Not this story, deliberately:** STEP SEQ could equally "play when switched on" (it does not today —
it hunts its root on channel 1 only, so the channel-16 auto-play drone never triggers it). That would
make both sequencers behave alike, but it changes the trigger model of 15.1 which is already
accepted, and it collides with the entry quantisation above. Revisit once PERC is playing.

## Dev Notes

- **Where it renders:** `PluginProcessor::processBlock`, after `synth.renderNextBlock(...)`
  (~line 1059) and before `compressor.process(buffer)` (~line 1088).
- **Clock:** resolve the step interval once per block from `SyncDivision` / the free RATE, exactly
  as STEP SEQ does; do not run a second time base.
- **Quantised bass entry (AC6):** PERC must expose how many samples remain to its next pattern
  start; `StepSequencer` gains a "start pending" state that consumes it. Keep the arithmetic in one
  place — two independently rounded step intervals are how sequencers drift apart.
- **Sample playback:** reuse `SampleMapping` / the sample store and its background thread (12.6).
  A track is a one-shot voice: retriggering it cuts the previous hit on that track.
- **No `#include` of the drum kit into the repo.** SamsSonor stays a README link (CC BY-SA 4.0).
- Combo item order MUST equal the enum order (`ComboBoxAttachment` maps by index).
- Do not touch global parameter defaults — missing ⇒ default hits old presets retroactively.
- New `.md` resource ⇒ CMake reconfigure + `/t:Rebuild /nodeReuse:false` of the help targets, or the
  link fails with LNK1236. Build with `/t:Rebuild` in any case (structs embedded by value in
  `SynthVoice`).
- Files (expected): `Source/DSP/PercSequencer.h` (new), `Source/Modules/PercSpecs.h` (new),
  `Source/UI/PercGrid.{h,cpp}` (new, the custom body), `Source/Audio/Parameters.h`,
  `Source/PluginProcessor.{h,cpp}`, `Source/UI/PluginEditor.cpp`, `Resources/{EN,DE}/perc.md`,
  `README.md`, `CHANGELOG.md`.
- Feature branch, **no push, no merge** — the maintainer decides.

## Dev Agent Record

Implemented 2026-08-11 on `feat/perc-module`. New: `Source/DSP/PercSequencer.h`,
`Source/Modules/PercSpecs.h`, `Source/UI/PercGrid.h`, `Resources/{EN,DE}/perc.md`. Touched:
`Source/Audio/Parameters.h`, `Source/Modules/AllModules.h`, `Source/DSP/{SampleBank,StepSequencer}.h`,
`Source/PluginProcessor.{h,cpp}`, `Source/UI/PluginEditor.{h,cpp}`,
`Source/UI/rack/{ModuleDescriptor.h,ModuleFrame.cpp}`, `README.md`, `CHANGELOG.md`.

Decisions taken while building, beyond the story:

- **Velocity 127, not the sequencer's 100.** An SFZ tracks velocity at the spec default, so 100
  costs 4.2 dB for nothing — the `Drum Pattern` preset had to compensate exactly that. Balance
  belongs on LEVEL, where it is visible.
- **A zone now remembers its name** (`SampleZone::name`, the sample's filename stem). The mapping
  knew it all along and dropped it. That is what lets a lane read "Kick"; the stored parameter is
  still the note number, so a kit arriving late or a different kit cannot move a lane to another
  drum.
- **`Knob::textFromValue`** added to the rack framework for that read-out — presentation only, the
  parameter is untouched. `valueFromText` has to be supplied with it, or typing into the box would
  parse "Kick" as 0.
- **Dragging across the grid paints**, in the state the first click chose. Writing sixteen hats one
  click at a time is what a grid is meant to spare you.
- **Steps past LEN are drawn dimmed** rather than hidden — the same honesty as the greyed-out knobs
  elsewhere: the pattern is still there, it just does not play.

**User-verified 2026-08-11** at the instrument: "klingt jetzt auch schon ganz ordentlich — er war
vorher einfach viel zu leise". The loudness was the whole complaint, and it was gain staging, not
the pattern or the kit.

### What using it changed, all on the maintainer's report

- **AMP is 0..1 like every level knob in the rack**, with the headroom inside (`kAmpScale = 4`).
  A knob that reads 0..4 in one module and 0..1 everywhere else teaches nothing but mistrust.
- **A global AMP beside the per-lane ones.** Balance and level are different jobs; without the
  second, changing the kit's loudness meant moving four knobs and wrecking the balance on the way.
- **PAN per lane**, constant power — PERC has to do its own, it never reaches the STEREO stage.
- **Lane names in the grid**, uppercase and sized off the row height. Four unlabelled rows of boxes
  were the first thing that made the module unusable ("mir ist unklar, wie ich das programmiere").
- **Left click sets and sounds a step — including one already set — right click clears.** One
  preview per cell, so a drag does not machine-gun the sample.
- **KIT lists only mapped sets, and 0 means no kit.** That needed a framework addition:
  `rack::Combo::itemValues`, a value list parallel to the items. A filtered list whose POSITION is
  the value is exactly how this project has twice loaded the wrong sample; with the value list the
  parameter keeps meaning store-index-plus-one whatever the list shows.

### The lesson worth carrying to the next module

PERC inherited the sampler substrate but none of Epic 12's retrofits, and paid for each one
separately: persist the set **by name**, keep the loader's queue and generation **per selector**,
and **refresh the combo** as sets arrive. All three were bugs the maintainer found by using it —
the last one had the module reading "(no kit)" while the drums were audibly playing.

## Follow-ups (not this story)

- **Story 12.7** — SFZ choke groups. Without it the kit's open and closed hi-hats ring over each
  other, which a four-track beat makes more audible, not less.
- A demo preset combining `DAF Bass` with a beat, once the sound is agreed.
- Per-step velocity / accents, if the flat 100 turns out to sound mechanical.
