Plays your own **recordings** (WAV/AIFF) as a sound source — through the whole JASS chain: filter,
wavefolder, mod matrix, arpeggiator, PAN and the binaural output modes.

- **LOAD** copies a file into your Samples folder (`%AppData%\JASS\Samples`) and loads it; **SET**
  picks among loaded sets. Presets remember the set **by name** from that folder.
- **Multisampling (FOLDER):** load a whole folder as ONE set. Files named like `Piano_C3.wav` /
  `Pad_A#4.wav` (note name with C4 = middle C, or a MIDI number) are spread across the keyboard,
  each covering the range halfway to its neighbours — so instruments stay natural over more than
  one octave. LOAD also accepts an **.sfz** file (its key mapping is imported; read opcodes:
  `sample`, `key`, `lokey`/`hikey`, `pitch_keycenter`, `default_path` — everything else,
  including velocity layers, is ignored).
- **ROOT** — the key at which a **single** recording plays at its original speed; every other key
  transposes it, tape-style (formants shift with pitch: usable range is roughly ±1 octave — the
  nature of single-sample playback, not a defect). For multisample sets each zone brings its own
  root, so ROOT is inactive (dimmed).
- **START / END** trim the played region; **MODE**: One-Shot, Loop (click-free crossfaded join),
  Reverse, Rev-Loop. **SPEED** (0.25×–4×) multiplies the playback rate on top of the key,
  tape-style — pitch moves with it.
- **STRETCH** decouples pitch from time: the key sets only the pitch, SPEED only the tempo —
  a loop keeps its rhythm on every key, and all loop voices stay beat-locked regardless of
  pitch. The engine works ~60 ms ahead internally; that lead is pre-computed at note-on, so
  playing stays immediate. The trade-off is a subtle phase-vocoder character on transients.
  Off = classic tape behaviour.
- **Stereo files stay stereo**: left/right render as two placed sources around PAN; in the Mono and
  Pseudo-Stereo output modes they collapse to a mono downmix.
- **Limits:** 60 s per file, 5 minutes of audio per set, 32 sets.

Unlike WAVETABLE (a pitch-locked single-cycle timbre), the SAMPLER plays the recording through
time — the recording's own envelope and character are the sound.
