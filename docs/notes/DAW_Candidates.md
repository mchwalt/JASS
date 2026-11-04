# DAW candidates for JASS

JASS is a VST3 instrument; the song, the tracks and the transport belong to a
DAW ([AD-13](../ARCHITECTURE.md)). This note is the running record of which
DAWs were looked at, what they were tested with, and why each one is in or
out. It is kept so that a DAW is not re-evaluated from scratch every few weeks;
a rejected entry stays, with its reason and date, because reasons expire.

What JASS needs from a host (the test list, in the order it bites):

1. Loads a VST3 instrument on Windows and shows its editor at 1920 px width.
2. Passes MIDI from a track or clip and from a MIDI input device to the plugin.
3. Passes tempo, transport state and the song position to the plugin every
   block (Story 18.6: STEP SEQ and PERC follow the host clock and survive
   STOP/PLAY).
4. Keeps calling a plugin that has no input and no audio tail (self-playing
   sequencers; see the Bitwig "Suspend" entry below).
5. Saves and restores plugin state, including the ~150 parameters and the
   pattern data blocks (Story 18.5).
6. Linux build available, so the Linux VST3 can be checked too.

## Status as of 2026-11-04

| DAW | Status | One-line reason |
|---|---|---|
| **Bitwig Studio** (Essentials, 99 EUR) | **In the race, tested first** | Clearest UI for the maintainer; 18.6 host-clock work was done against it. Trial ends ~2026-11-09. |
| **Cubase Elements** (99 EUR) | In the race | Same clarity, more classic UI; VST3 units shown as folders. Trial ends ~2026-12-09. |
| **Waveform Free 14** | In the race as the cross-check | Free, Linux, edit round-trip proven; UI and font scaling criticised. |
| REAPER (60 USD) | Not pursued | Not intuitive out of the box, uneven scaling. Was the early reference host in `DEVELOPER_GUIDE.md`. |
| Ableton Live 12 Intro (79 EUR) | Not pursued | Windows only, 16 tracks. |
| Renoise (~75 EUR) | Not pursued | Tracker paradigm, not what the Roboter song needs. |
| Cakewalk Sonar (free) | Not pursued | Windows only. |
| LMMS | **Out** (2026-10-11) | No VST3; 1.3 is alpha; UI not intuitive either. |
| Ardour | Out | gtkmm; not considered further. |
| Avid Pro Tools | **Out** (2026-11-04) | Hosts AAX only, no VST3. See below. |
| SoundCraft (storytold) | **Not a DAW candidate, but a candidate test rig** (2026-11-04) | Pre-alpha, no MIDI device layer; fully scriptable (CLI, JSON, MCP), transport reaches VST3. See below. |

Open-source engines looked at before Waveform was adopted (2026-10-05):
Tracktion Engine (JUCE, GPL3, the engine under Waveform; kept as the fallback
for a thin front end of our own), OpenDaw (Qt6 + Tracktion, one author, dead
since 2026-03), Zrythm 2 (AGPL, Qt/QML, alpha, crashes), Helio 3.18
(JUCE-only MIDI sequencer, no audio DAW), Element (no timeline), Qtractor
(Linux only). Requirements at that time: open source, forkable, no Qt, Linux
build, no video, no AI assistant.

## The three candidates in detail

### Bitwig Studio 6.1 (Essentials)

- Demo installed 2026-10-10; JASS.vst3 scanned and found. Maintainer's
  verdict: "clear and intuitive", the most modern UI of the three.
- VST3 parameter list is **flat**: Bitwig ignores VST3 units as folders but
  honours `kCanAutomate = false`, so the grid cells vanish from the list and
  the module prefix in the host name is the only ordering (Story 18.4).
- **Suspend "Trust plug-in"** (default): a VST3 with no input and tail 0 is no
  longer called once the track is idle, so a self-playing PERC went silent.
  Fix: JASS reports an infinite tail (`getTailLengthSeconds`).
- **STOP rewinds to where PLAY started**; JASS shifts its phase anchor by the
  playhead jump so the figure continues instead of restarting (Story 18.6,
  accepted 2026-10-25).
- Clips in the **Launcher** are what the maintainer uses; the Roboter song is
  being built there.

### Cubase Elements 15

- Trial installed 2026-10-10 (one installer for all editions, the licence
  decides). Maintainer: "as clear as Bitwig, Bitwig looks more modern".
- VST3 units **are folders**, sorted by the numeric unit id, which is why the
  group ids carry a `#n` suffix that puts the hashes in rack order.
- Cubase lists every parameter that is not `kIsHidden`; JUCE never sets that
  flag, so the ~4000 grid parameters were visible until AD-14 moved the
  patterns out of the APVTS. The JUCE MIDI-CC emulation (2080 entries) is
  switched off for the same reason.
- An old project saved before 18.5 still folds its figure and grid back in
  correctly (checked 2026-10-17).

### Waveform Free 14

- Same stack as JASS (Tracktion Engine + JUCE). Two instances on two tracks,
  host tempo, edit round-trip and host-side mute were all validated
  (2026-10-05), on Windows and on a Lubuntu VM (2026-10-10).
- Rejected on 2026-10-10 for its handling (pop-up help, controls spread out,
  unreadable text on a large screen), back in the race on 2026-10-11 because
  "it works and has what JASS needs". Its CPU meter shows 100 % for an idle
  JASS while the task manager is relaxed; treated as a display quirk, not
  pursued.

## Rejected with reasons worth keeping

### Avid Pro Tools (checked 2026-11-04)

Out, for one hard reason: **Pro Tools loads AAX only.** VST3 and AU bundles
are ignored, still true in Pro Tools 2026.4. Three things would have to change
before JASS could even be scanned there:

- JUCE can build an AAX target, but that needs the AAX SDK from the Avid
  developer programme, and a release Pro Tools only loads AAX plugins signed
  with PACE/iLok. Unsigned builds run in the developer build of Pro Tools only.
- The AAX SDK is proprietary. JASS is AGPLv3; an AAX build could not be part
  of the public repository.
- Third-party VST-to-AAX wrappers exist but are reported as unstable, and they
  would hide exactly the host-clock behaviour that 18.6 depends on.

Editions for the record: Intro is free (8 instrument tracks), Artist ~99 USD
per year, Studio ~299 to 399 USD per year, Ultimate ~599 USD per year; all
subscription or expensive perpetual. Without an AAX build none of it helps.
Revisit only if Avid ever adds VST3 hosting.

### SoundCraft by storytold (checked 2026-11-04)

Open-source, clean-room re-implementation of Pro Tools in Rust (egui), MIT or
Apache-2.0, <https://github.com/storytold/soundcraft>. Repository created
2026-10-07, releases almost daily (v0.17.0 on 2026-11-03), self-declared
**pre-alpha**. Roughly 43 % of its commits carry Claude Opus 5.5 as co-author,
and the app is built to be driven by agents (MCP server, JSON control channel,
CLI). Unlike the original it hosts **CLAP and VST3** and ships signed Windows
builds (MSI and portable, x64/x86/ARM64). Not to be confused with the Harman
mixing-desk brand of the same name; search with "storytold" or "Rust DAW".

Why it is out for now:

- **No MIDI device layer.** No keyboard input, no live playing, no MIDI
  recording; listed in its roadmap as an alpha blocker.
- Transport and tempo to third-party plugins are not in the docs, but the
  source has them: the mixer builds a `Transport` (playing, tempo, meter,
  samples, seconds, quarters, bar start) per block and the VST3 host maps it
  to the VST3 `ProcessContext` (`crates/vst3-host/src/plugin.rs`,
  `ffi.rs`). Whether the values are right during bounce and after locate is
  untested; that is the first thing to check.
- MIDI and instruments are rated by the project itself at ~30 % readiness;
  there is a piano roll and SMF import, but no clip launcher.
- A release with overflow and crash fixes every few days means test results
  are not comparable week to week; 300+ open issues after four weeks.

What is promising, and why it stays on the list:

- VST3 hosting since 2026-10-06; plugin editor windows on Windows since
  2026-10-12, verified by the project with a JUCE VST3 effect. JASS would
  most likely load and show its rack.
- VST3 parameters are handed to the controller before state is saved, IMessage
  and IAttributeList are supported, multi-out instruments are accepted. The
  ~150 JASS host parameters would be a fair stress test for their host.

**Why it stays interesting anyway: everything is a command.** The README
section "For agents and scripts" says it, and the sources confirm it: every
UI action has a command id with JSON parameters, reachable from the CLI
(`soundcraft-cli run --cmd ...`), a JSON control channel on a port, and an
MCP server (`claude mcp add soundcraft -- soundcraft-cli mcp`). The commands
JASS would need all exist: `engine.vst3_plugins` (discovery), `track.new` and
`mix.instrument` with a `vst3:` id, `midi.new_clip` and `midi.note_add`,
`transport.play`/`locate`/`stop`, `file.bounce_mix` (returns peak and LUFS),
`session.inspect`, `session.save`/`open` (plugin state round-trip), and
`ui.screenshot` in the desktop app. Programmatic calls never open dialogs.

That makes SoundCraft a possible **automated test rig for the plugin build**,
which JASS has never had (no unit tests, verification is build + ears): an
agent can load JASS.vst3, put a figure in a MIDI clip, bounce, and measure
the WAV (the audio-measure tooling already exists), or save and reopen a
session and inspect the restored parameters. The 18.6 host-clock path could
be checked the same way, if the bounce transport proves trustworthy.

Fork for this purpose: `mchwalt/soundcraft`, clone `D:\Projects\Rust\soundcraft`
(Rust 1.99 installed 2026-11-04). Not a DAW for the maintainer's own work.
Anything that breaks in the host is a bug report for SoundCraft, not a JASS
story.

Revisit as a DAW when the MIDI device layer has landed and the release
cadence has settled. The project's own estimate for alpha is 110 to 180
hours of work.

Sources (2026-11-04): project README and ROADMAP.md, GitHub releases page,
guide at pasqualepillitteri.it (release history, co-author count), Antares
and Vector DSP format overviews for the Pro Tools AAX-only statement.
