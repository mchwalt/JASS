# Story 12.5: SAMPLER velocity layers

Status: implemented 2026-08-04 — USER EAR TEST DEFERRED (MIDI keyboard not connectable at the
time; AC1/AC2/AC5 need real velocity input). Packs-v2 zips + piano-pack release update follow
after the test. Both 4-layer sets are installed locally (Splendid 1461 s / Salamander 1671 s,
together ≈1.2 GB decoded — watch the startup preload time; if it hurts → lazy loading = 12.6).

## Story

As a player,
I want the SAMPLER to pick the sample matching my key velocity (soft hit → soft recording),
so that multisampled instruments respond with the real timbre change of the acoustic
instrument — a hard-struck piano string is brighter, not just louder — instead of playing
one fixed layer at every touch.

## Context (what exists today)

- The .sfz parser already READS `lovel`/`hivel` — but only to rank overlapping layers and
  keep the LOUDEST one (12.2 dedupe: "velocity layers would otherwise load N copies that can
  never be reached"). That dedupe is exactly what this story removes.
- Playback is velocity-DEAF: `SamplePlayer::trigger(transposeRatio, midiNote)` has no velocity
  input; LEVEL is a knob; `noteVelocity` exists per voice but only as a mod-matrix source.
  (Interim workaround documented for users: mod matrix Velocity → SAMPLER LEVEL.)
- Both shipped pianos have full layer sets upstream: Splendid PP/Mp/MF/FF (226 FLACs, ~557 MB
  decoded), Salamander v1–v16 (~2.3 GB decoded for all 16 — curation required).
- Bounds today: 60 s/file · 900 s/set · global byte budget ≈ 646 MiB (12.1 worst case, the
  never-free store's RAM guarantee). Splendid×4 layers ALONE ≈ 557 MB ⇒ caps must move.
- The A3/A4 source quirk (FF layer only) becomes nearly invisible with layers: FF sounds only
  at the hardest touch.

## Scope

1. **Parser** (`SampleMapping.h`): keep velocity layers as SEPARATE entries — `Entry` gains
   `loVel` (hiVel exists). Replace the loudest-wins dedupe: entries are duplicates only when
   BOTH key range and velocity range coincide; the shadow-drop must become (key ∧ velocity)
   aware so a true subset region still drops but a layer never does.
2. **Zones** (`SampleBank.h`): `SampleZone` gains `loVel/hiVel`; `SampleSet::zoneFor(note,
   velocity)` picks by key AND velocity (nearest-zone fallback stays for gaps). Unmapped/folder
   sets: one full-range layer — one code path, as always.
3. **Voice** (`SynthVoice`/`SamplePlayer`): `trigger()` gains the note velocity (0–1 → 1–127);
   the zone pick uses it. Retrigger declick already handles cross-zone blends (dkZone).
   Mid-note SET switches re-pick with the stored velocity.
4. **`amp_veltrack`** opcode (+ sensible default): velocity scales the layer's gain so the
   128-step velocity axis is continuous inside each discrete layer band. DESIGN DECISION
   below — the SFZ spec default is 100 (full tracking), but JASS sets are velocity-deaf today.
5. **Small opcodes while the parser is open**: `volume=` (dB, per region — layer balancing)
   and `tune=` (cents → rate factor 2^(cents/1200) — piano stretch tuning). Both trivial now,
   both used by the upstream pianos.
6. **Caps** (DESIGN DECISION below): per-set seconds and the global byte budget must grow for
   layered pianos; the global budget stays the hard RAM protector, and startup preload time
   must be measured (600+ MB of FLAC decode — if it hurts, lazy loading becomes story 12.6,
   NOT scope creep here).
7. **Curated packs v2**: `tools/piano-packs/*.sfz` regenerated with layers (Splendid: all 4;
   Salamander: a curated subset, e.g. v4/v8/v12/v16 — 16 is RAM-pointless); rebuild zips,
   publish `piano-pack-v2`, README/release notes updated. A3/A4 quirk note kept on the FF
   regions only.
8. Append-only params (if any), help EN/DE, CHANGELOG, story record.

## Out of scope

- Round robins, release samples, string resonance, half-pedalling (sister-project territory).
- Crossfading BETWEEN velocity layers (`xfin_lovel` …): upstream pianos switch discretely too.
- Lazy/streamed sample loading — candidate story 12.6 if preload time demands it.

## Design decisions (settled by the user, 2026-08-04)

- **D1 — velocity→gain default (`amp_veltrack`): SFZ-spec default 100 for .sfz-imported
  sets** (they finally respond to touch); folder/single sets stay at 0 (legacy — no surprise
  for Drums/Loops; the mod matrix remains their velocity route).
- **D2 — bounds: global budget 646 MiB → 4 GiB** (user pick — headroom for bigger libraries;
  needs a 64-bit-literal-safe constant). Per-set seconds 900 → 3600 (a 4-layer piano is
  ~1700–2100 s; the GLOBAL budget stays the actual RAM protector). Measure preload time.
- **D3 — Salamander curation: v4/v8/v12/v16** (even spread), velocity bands from the original
  16-band table merged to four.

## Acceptance criteria

- AC1: An .sfz with velocity layers plays different recordings for soft/hard hits on the same
  key (verified by ear on Splendid: PP vs FF timbre).
- AC2: Velocity is continuous: within a layer band the gain follows velocity (amp_veltrack),
  no jumps at layer borders louder than the recordings themselves differ.
- AC3: Single-file and folder sets behave EXACTLY as before (velocity-deaf unless the user
  routes the mod matrix); old presets bit-identical.
- AC4: The 12.2 error contract holds: over-cap sets are rejected whole, message names file and
  reason; the global budget still caps RAM.
- AC5: `volume=`/`tune=` applied when present (Salamander's retuned data audible as cleaner
  octaves/unisons vs. equal temperament).
- AC6: piano-pack-v2 zips reproduce via `tools/build_piano_zips.py`, licenses unchanged.

## References

- [Source: 12-2-sampler-multisampling.md] — parser, store mechanics, caps, error contract
- [Source: 12-4-sampler-amp-envelope.md] — SamplePlayer state, gateOff/tail plumbing, the
  Störton elimination chain (why stereo spread is mode-dependent)
- [Source: Source/DSP/SampleMapping.h, SampleBank.h, SamplePlayer.h, Audio/SynthVoice.cpp]
- Upstream layer data: SplendidGrandPiano `Data/{PP,Mp,MF,FF}.txt`, SalamanderGrandPiano
  `Data/vel_NN.txt` + `Data/region.txt` (offsets are CC-driven ⇒ stay 0 for us)
