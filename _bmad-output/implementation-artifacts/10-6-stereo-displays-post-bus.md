# Story 10.6: Stereo displays — OSCILLOSCOPE + SPECTRUM tap the bus AFTER the master stages

Status: done

<!-- Raised 2026-07-29 while ear-testing Story 10.4: the user expected the ROOM effect to show in
     the displays and (correctly) diagnosed why it cannot — the capture point predates the whole
     master-bus section. -->

## Story

As a user shaping the master bus,
I want the OSCILLOSCOPE and SPECTRUM to show the **stereo signal I actually hear**, so that the
master-bus stages (compressor, widener, Binaural/Kunstkopf, ROOM reflections) are visible, not
invisible, in the displays.

## Context

`WaveformCapture` is written in `processBlock` BEFORE the master-bus section (deliberate, Story 2.3:
"the scope shows the dry mono mix"). Everything after it — COMPRESSOR, Pseudo-Stereo widener,
Binaural/Kunstkopf rendering and the Story-10.4 ROOM stage — bypasses both displays. With the
engine now genuinely stereo at the output (Epic 10), a mono pre-bus tap no longer represents what
comes out of the headphones.

## Scope (to be refined before dev)

1. **Stereo capture, post-bus:** `WaveformCapture` (or a second instance) carries L+R, written after
   the master-bus stages. Decide: pre or post master VOLUME (leaning pre-volume, so the display does
   not collapse when the user pulls the volume down; note the old capture is pre-compressor too).
2. **OSCILLOSCOPE:** two traces (L/R, colour-separated — mind the red-green-colorblind rule: use
   blue/neutral pair, not red vs green). In the 1-channel modes (Mono/Pseudo-Stereo-off) L==R —
   draw a single trace to avoid doubled lines.
3. **SPECTRUM:** decide between two curves or the L/R **power sum** (power sum is honest — a plain
   L+R sample sum would show phantom cancellations the ear does not hear).
4. **RT safety:** capture stays lock-free/alloc-free (Epic 11 rules); WaveformCapture is sized once.
5. Regression: displays in Mono mode should look the same as today (modulo the tap point moving
   behind the compressor — decide whether that change is wanted or the compressor stays outside).

## Decisions (resolved 2026-07-29)

- **Tap point: the TRUE FINAL output**, after master volume (user, quoting Helmut Kohl: "wichtig
  ist, was hinten rauskommt" — the displays must show exactly what reaches the ears, nothing
  else). Accepted consequence: pulling MASTER down shrinks the picture.
- **SPECTRUM: two curves** (L/R) — the L/R differences ARE the visible fruit of the spatial
  stages; a power sum would hide them.
- No dry-mono toggle — the old view is gone, simplicity first.

## Dev Agent Record

### Implementation (2026-07-29, same session as 10.4)

- **`WaveformCapture` → stereo:** two rings sharing ONE atomic write index (channels stay
  sample-aligned in the ring); `isStereoContent()` = max |L−R| over the aligned recent ring data
  > 1e-3 — drives the displays' mono collapse. GUI-side scratch buffers are members (no per-frame
  allocation). Audio-thread cost: one extra store per sample.
- **Trigger evolution (ear/eye-iterated):** plain zero-cross on the sum → "nervous" (post-FX
  ripple crossings from reflections/compressor) → **hysteresis** (arm below −5 % peak, fire above
  +5 %) → still jumped when L and R carried UNRELATED signals (sine left, saw right: the sum has
  no stable period) → final: **per-channel trigger** (each channel cut at its own hysteresis
  trigger, like a two-channel scope in alternate-trigger mode). The two plots are then not
  mutually time-aligned — for unrelated signals no meaningful common alignment exists.
- **Tap moved** from pre-compressor (Story 2.3's dry mono view, deliberately superseded) to the
  very end of `processBlock`, after the master-volume ramp.
- **OSCILLOSCOPE:** L and R as **two plots side by side** (user layout decision) — **L blue**
  (existing colour), **R orange** (`#fb923c`), a red-green-colorblind-safe pair (project rule);
  shared y scale (labels once, far left), per-plot time ticks (coarser in stereo), L/R tag per
  plot; single full-width plot when content is effectively mono. **Emergency fallback** (user:
  "nur notfalls"): if a plot half would drop below 160 px, both traces overlay two-coloured in ONE
  diagram instead — never triggers at the current W12H2 size.
- **SPECTRUM:** two FFTs (only one when mono), L violet / R orange, fills at reduced alpha,
  same legend + mono collapse.
- Help texts EN/DE rewritten (final-output tap + stereo colours), CHANGELOG.

### File List
- `Source/DSP/WaveformCapture.h` (stereo rewrite)
- `Source/PluginProcessor.cpp` (tap moved to final output)
- `Source/UI/WaveformDisplay.h`, `Source/UI/SpectrumDisplay.h` (two traces/curves + legend)
- `Resources/{EN,DE}/{oscilloscope,spectrum}.md`, `CHANGELOG.md`

### Verification
- Clean rebuild green (WaveformCapture struct grew → `/t:Rebuild` per the ODR lesson), then
  incremental for the UI iterations.
- **✅ User-verified visually (2026-07-29, "perfekt"):** side-by-side stereo plots, two-coloured
  spectrum, and the per-channel trigger holds both traces still even with sine-L + saw-R.
  L/R tags bumped to 13 pt bold on request.
