# Changelog

All notable changes to JASS are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
JASS uses **CalVer** versioning: `YYYY.MM.MICRO` (e.g. `2026.07.0`), where `MICRO`
increments for additional releases within the same month. This is the app/release
version and is independent of the preset **`FormatVersion`** (an integer schema
contract — currently `6`; see [`docs/JASS_Preset_Format.md`](docs/JASS_Preset_Format.md)).

## [Unreleased]

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
