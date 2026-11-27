The on-screen keyboard for playing notes with the mouse or the computer keyboard.

**88 keys, A0–C8** — the full piano range, the same layout as a grand or an 88-key digital piano. MIDI input is not limited by it: notes outside the visible range still sound.

Hide this module (via the **MODULES** menu or the customization panel) when you play through an external MIDI keyboard — it never affects the sound, only the display.

**Computer keyboard**

Play across the full width of the letter keys: the **home row** (`a s d f g h j k l ö ä #`) are the white keys, and the **top row** above them (`w e t z u o p +`) are the black keys — about 2½ octaves.

- **↑ / ↓ arrow** — shift the whole range one octave up / down
- **Space** — re-pluck the Karplus string

The keyboard must have focus for the letter keys to sound (it grabs focus on launch; click it once if the keys stop responding).

**Chords**

Ordinary keyboards drop some three-key combinations — the third note never arrives. Shift the octave, or play chords over MIDI; the letter keys also send one fixed velocity.

**MIDI controller (knobs and faders)**

Knobs and faders of a MIDI controller move JASS in the standalone app (in a DAW, map them there).

- **Header combo** — **Auto** picks the profile from the MIDI input name (MiniLab 3, otherwise Generic (GM)); or pick one by hand
- **MiniLab 3 knobs 1–7** — the controls of the marked module, left to right: knobs, combos (step through their items) and switches (on from half way); skipped: step cells and the list combos SET, BANK, KIT and the MOD MATRIX's MOD and PARAM
- **Nothing marked** — knobs 1–7: Cutoff, Resonance, LFO 1 Rate, LFO 1 Depth, Chorus, Delay, Reverb
- **Mark a module** — with the MiniLab 3 profile: click its title bar or one of its controls, or turn knob 8 (it jumps on first touch); the marked module has a white frame and its controls carry the number of their knob; a greyed-out control has none and is not moved
- **On / off** — press the main encoder: switches the marked module on or off
- **Unmark** — click the marked module's title bar again; loading a preset or RESET unmarks too
- **Banks** — a module with more than 7 controls: hold Shift and turn knob 8; the title bar shows the bank, e.g. 2/3
- **Programs** — same in the ARTURIA and the DAW program (Shift + Pad 3)
- **MiniLab 3 faders** — MASTER VOL, TEMPO, A LVL, B LVL, whatever layer you edit
- **Fine tempo** — Shift + main encoder moves TEMPO by 1 BPM per click
- **Generic (GM)** — CC 74 Cutoff, 71 Resonance, 73 Attack, 72 Release, 91 Reverb, 93 Chorus
- **First touch** — a knob or fader sets its value at once, so the sound jumps to where it stands; the MASTER VOL fader too (top = full level, the default is half)
- **Layer** — a control moves the layer you edit: A, B, or both in A+B
- **Files** — one `.json` per controller in `%AppData%\JASS\Controllers`; edit `cc` and `param` there (MiniLab 3 knobs 1–7: `fallback`), delete a file to get the factory version back on the next start
