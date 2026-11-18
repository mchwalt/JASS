# tools

Offline helpers. Not part of the build — outputs are committed, so a normal build needs no Python.

## `gen_kemar_hrir.py` — regenerate the embedded HRTF table

Produces [`../Source/DSP/KemarHrir.h`](../Source/DSP/KemarHrir.h), the MIT KEMAR head-related
impulse responses used by the **Kunstkopf (HRTF)** output mode. You only need this if you want to
change the data (e.g. more azimuths, the 512-tap `full` set, or a different dataset) — the generated
header is already committed.

**Steps**

1. Download the KEMAR *compact* set (≈230 KB) and unzip it:
   <https://sound.media.mit.edu/resources/KEMAR/compact.zip>
2. Run (points at the horizontal-plane folder `compact/elev0`):

   ```sh
   python tools/gen_kemar_hrir.py --input <path-to>/compact/elev0 --output Source/DSP/KemarHrir.h
   ```

3. Clean-**rebuild** (`/t:Rebuild`) — the tap count / table size is compiled into the voice.

The script reads the horizontal plane (azimuths 0–90° in 5° steps; the symmetric head mirrors the
other side at runtime), normalises the 16-bit stereo WAVs to float, and writes a plain `constexpr`
float header (~66 KB). Requires only the Python standard library (`wave`, `struct`).

**Attribution.** MIT KEMAR HRTF measurements — Bill Gardner & Keith Martin, MIT Media Lab (1994),
free with no restrictions on use provided the authors are cited. The citation is baked into the
generated header and noted in the project README. <https://sound.media.mit.edu/resources/KEMAR.html>

## get_iowa_piano.py

Builds the optional **Piano** multisample set for the SAMPLER (Story 12.2) from the University
of Iowa *Musical Instrument Samples* (Steinway & Sons model B, ff layer — freely available
without restrictions). Downloads 13 notes in tritone spacing (C1–C7), strips the ~0.5 s of room
tone at each file's start (untrimmed, the set is unplayable live), trims to 10 s with a
fade-out, and writes `Piano_<note>.wav` files following the JASS folder naming convention plus
an attribution `README.txt`. Standard library only (no `aifc` — parses AIFF by hand, Python
3.13-safe). Usage: `python tools/get_iowa_piano.py [output-folder]`, then copy the folder into
`%AppData%\JASS\Samples\` or import it via SAMPLER → FOLDER.

## `rig/clocktest.py` — 18.6 host-clock test against the SoundCraft rig

Drives the SoundCraft fork's CLI (`D:\Projects\Rust\soundcraft`, see
[`docs/notes/DAW_Candidates.md`](../docs/notes/DAW_Candidates.md)) headless: one or two JASS.vst3
tracks with STEP SEQ on at 1/16, a long clip note as the root, a four-bar bounce, and every onset
measured against the grid. Usage and the meaning of the numbers are in the script's docstring.
Needs numpy and the rig folder `D:\Projects\Rust\soundcraft-rig`.

## `rig/nulltest.py` — bit-exact null test of a refactor (Story 21.4)

Renders presets through `build/Release/JASS_Render.exe` (target `JASS_Render`, a console exe on
the shared code: one preset in, a fixed MIDI script, one 32-bit float WAV out, one process per
preset) and compares two render folders sample for sample. Recipe: build `JASS_Render` *before*
the change and copy the exe aside (e.g. `build/nulltest/ref/`), render the reference twice and
compare (proves determinism), make the change, render again, compare against the reference.

    python tools/rig/nulltest.py render  build/nulltest/ref/JASS_Render.exe build/nulltest/refA
    python tools/rig/nulltest.py render  build/Release/JASS_Render.exe       build/nulltest/new
    python tools/rig/nulltest.py compare build/nulltest/refA build/nulltest/new

`JASS_Render` options (Story 21.5, layer B): `--b <preset>` loads that preset's patch into layer B
and turns B on (LAYER), `--split <note>` splits instead (below the note → B), `--solo a|b` mutes
the other layer, `--roundtrip` checks that every parameter survives a preset and a DAW-state
round trip into a fresh processor (exit 3 otherwise; unused MOD MATRIX slots are skipped — they
are left out of a preset by design), `--save <file>` writes the loaded state back as a preset,
`--info` prints the parameter counts, `--note <n>` plays only that note (held 0.35 s, three
times), `--over-init` loads like the app with A and B lit on top of Init, and a preset named
`init` renders the Init patch itself.

`JASS_Render --selftest` (code review 2026-11-18) checks what the null test cannot reach — all
corpus presets are Single / layer A: the router's destinations (split boundary, a note-off after
the split moved, the pattern channel by TARGET), a live tap above the split while the ARP plays B,
mono GLIDE's note-off channel, and a host All-Notes-Off on a latched STEP SEQ note. Exit 4 on a
failure. Run it after every change to the routing.

Without preset arguments it renders `%AppData%\JASS\Presets\*.jass` (copies — PresetIO migrates
an old file in place). The renderer runs the host code path: no LiveState, no drone, MASTER
locked; standalone-only behaviour still needs the app and an ear. Known non-deterministic
sources: ARP Random, SAMPLER STRETCH.
