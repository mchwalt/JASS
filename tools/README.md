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
