Movement — sources that change other parameters over time.

> **Modulation vs. Processing:** no audio passes through this group. These modules
> output *control signals* and *routing*, not sound. PROCESSING is the opposite —
> audio flows through it. The MOD MATRIX is the bridge: it lets a modulation source
> drive a knob that lives in another group (e.g. a filter cutoff or a delay time).

- Holds ENVELOPE-ADSR, the LFOs, ARP, GLIDE, PITCH ENV and the MOD MATRIX.
- Route these to targets in the MOD MATRIX; a knob shows a moving ring when a source drives it.

**Group header controls**
- **Enable** — bypasses/enables every visible module in this group at once.
- **Reset** (circular arrow) — puts this group back to your **default layout** (`RackLayout.json` in the JASS folder — **Save as default** in the MODULES panel writes it). It does not change any knob values.
- **Info** (circled i) — this help.
