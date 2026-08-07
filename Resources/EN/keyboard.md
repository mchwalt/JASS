The on-screen keyboard for playing notes with the mouse or the computer keyboard.

Hide this module (via the **MODULES** menu or the customization panel) when you play through an external MIDI keyboard — it never affects the sound, only the display.

**Computer keyboard**

Play across the full width of the letter keys: the **home row** (`a s d f g h j k l ö ä #`) are the white keys, and the **top row** above them (`w e t z u o p +`) are the black keys — about 2½ octaves.

- **↑ / ↓ arrow** — shift the whole range one octave up / down
- **Space** — re-pluck the Karplus string

The keyboard must have focus for the letter keys to sound (it grabs focus on launch; click it once if the keys stop responding).

**Chords on the computer keyboard**

Ordinary keyboards cannot report every three-key combination: their keys sit in a scan matrix without one diode per key, so certain triples are ambiguous and the controller reports nothing rather than a phantom key — the third note simply never arrives (you can reproduce it in any text editor). Which combinations are affected depends on the keyboard model. Shift the octave with the arrow keys to move the notes onto different physical keys, or play chords through a MIDI keyboard — which is what the computer keys are a stand-in for. They also send one fixed velocity, so velocity-layered instruments need real MIDI input.
