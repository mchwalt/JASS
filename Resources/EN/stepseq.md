Plays a figure of up to 192 steps you write yourself, transposed by the key you play. **The first key starts it and it keeps running** — you do not have to hold anything. A new key moves the figure to that root, the Up / Down octave keys shift it by an octave, and **SPACE stops it**. Switching the module off stops it too.

- **1 … 48** — each step's note. The box shows the real pitch (E1, C3 …) the step sounds at the keyboard's current octave — the figure still transposes with the key you play. The switch in a knob's corner cycles the step: **on → accented (filled: plays harder, filter opens) → off (a rest)**.
- **GATE button (header)** — flips the same 48 knobs to each step's **length**: 5–100 % of the step, then **TIE** (held into the next step, which takes over without a new attack) and **SLIDE** (the same, gliding into the next note — the 303). Flip back to PITCH with a second click.
- **SYNC** — step length as a note division. Set it to *Free* to use RATE instead.
- **RATE** — steps per second when SYNC is *Free*.
- **LEN** — how many steps before the pattern repeats.
- **GATE** — note length, for the whole pattern. **1 = legato**: each note is held into the next step, no gap. Lower values shorten every note; a step's own gate value scales on top of it.
- **ACCENT** — what an accented step does: how much louder it plays and how far the filter opens. At 0, accents change nothing.

Clicking or turning a step's knob sounds it, so you can write the figure by ear. A **rest keeps its note**: its knob is dimmed but still turns, so a silent step's pitch can be set — and heard — before switching it on. A **double-click** puts a knob back to the loaded preset's value. Preview, entry and the note names all follow the root the figure sounds at: the latched key while one is running, otherwise the keyboard's current C (Up / Down shifts it).

**Play the figure in instead of turning knobs.** The **Reset** button (the circular arrow in this module's title bar) empties the pattern and starts writing at step 1: a ring marks the step waiting for a note. Play a key — on the computer keyboard or a MIDI keyboard — and it is written there, switched on, and the ring moves to the next step. **SPACE** skips a step and leaves it as a rest. **← / →** move the ring without changing anything, **BACKSPACE** takes back the last note (one step back, switched off), **ESC** ends writing. Writing stops by itself after LEN steps. A **click on any step knob** moves the ring there, so a wrong note is corrected by clicking it and playing again. While the ring is showing, keys write instead of starting the pattern.

Replaces the ARPEGGIATOR: only one of the two can run, so switching this on switches that off.

**MIDI import/export — the LOAD MIDI / SAVE MIDI buttons in this module's title bar.** LOAD MIDI opens a `.mid` transcription: velocities become accents, note lengths become gate/TIE/SLIDE, the loop's cycle is detected, and the figure latches to its most frequent note and plays. SAVE MIDI writes the figure back out. One asymmetry: a TIE that changes pitch exports as the 303's overlap and comes back as SLIDE — MIDI cannot carry the difference.

A **red line** after a step marks the pattern's end (LEN): everything right of it is kept but does not play.

**Pages (A / B / C / D, header).** The 48 knobs show one page of the pattern; the buttons switch between four pages of 48 (LEN up to 192 = 12 bars of sixteenths). A dot on a button marks where the pattern is playing right now. With **FOLLOW** latched the shown page flips with the playhead; picking a page by hand pauses FOLLOW — latch it again to jump back and resume. Writing with the ring crosses pages by itself.
