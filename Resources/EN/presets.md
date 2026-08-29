Twelve quick-access slots (**F1–F12**) for switching presets on the fly — ideal for live playing.

Each slot maps to the matching function key on your keyboard.

**Load a preset**

- **Tap** a function key **F1–F12** — the assigned preset loads instantly, even while notes are sounding.
- Or **click** a button. An empty slot does nothing.
- The loaded preset's name appears in the header, as usual.

**Assign / change a slot**

- **Double-press** a function key (twice in quick succession), or
- **Double-click** a button.

A file dialog opens in your Presets folder — pick the `.jass` preset to put on that key.

**On the button**

- The assigned preset name is shown; a **blue dot** marks a filled slot.
- **Hover** a button and the full name scrolls across it if it is too long to fit.

The assignments are a **global** setting (stored in `PresetBanks.json`), independent of the loaded preset — they stay put when you switch patches and survive a restart.

If a slot's preset file was renamed or deleted on disk, pressing the key says so once and clears the slot. The header **DELETE** button removes a preset file entirely (to the recycle bin) and clears its key with it.

Out of the box, **F1–F4 come pre-assigned with the four demo presets** (Matrix Demo, Matrix Demo 2, FX Motion, Helikopter). The header **RESET** button restores this factory bank.

**MIDI through the same dialogs.** The header **LOAD** also opens `.mid` files: the figure is imported into STEP SEQ — accents from velocity, note lengths as gate/TIE/SLIDE, a looped transcription comes in as its cycle — and starts playing on its root. **SAVE** with a `.mid` name writes the current figure as a MIDI file for the DAW. Presets themselves stay `.jass`.
