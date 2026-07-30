Plays your own **recordings** (WAV/AIFF, up to 60 s, max 32 loaded) as a sound source — through the
whole JASS chain: filter, wavefolder, mod matrix, arpeggiator, PAN and the binaural output modes.

- **LOAD** copies the file into your Samples folder (`%AppData%\JASS\Samples`) and loads it; **SET**
  picks among loaded samples. Presets remember the sample **by name** from that folder.
- **ROOT** — the key at which the recording plays at its original speed; every other key transposes
  it, tape-style (formants shift with pitch: usable range is roughly ±1 octave — that is the nature
  of single-sample playback, not a defect).
- **START / END** trim the played region; **MODE**: One-Shot, Loop (click-free crossfaded join),
  Reverse, Rev-Loop. **SPEED** (0.25×–4×) multiplies the playback rate on top of the key,
  tape-style — pitch moves with it.
- **Stereo files stay stereo**: left/right render as two placed sources around PAN; in the Mono and
  Pseudo-Stereo output modes they collapse to a mono downmix.

Unlike WAVETABLE (a pitch-locked single-cycle timbre), the SAMPLER plays the recording through
time — the recording's own envelope and character are the sound.
