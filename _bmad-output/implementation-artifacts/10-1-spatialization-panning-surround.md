# Epic 10 — Spatialization: per-generator panning & optional surround

Status: done (implemented 2026-07-26/27, Epic 10 merged & released)
Type: **DSP + I/O architecture change** (the largest sanctioned so far — touches the output bus).

> User intent: "Umstellung des Mono-Synthy auf Stereo oder noch besser Surround (4.1 / 5.1),
> d.h. jedes Generator-Modul einem Kanal zuordnen — falls das Sinn ergibt."
>
> **Channel naming (user):** FL (Front-Left), FR (Front-Right), **FM (Front-Mid = the center
> channel, i.e. true mono/center)**, RL (Rear-Left), RR (Rear-Right), plus the optional `.1` LFE.
> "FM" here == the standard front-**center** speaker (carries centered/mono content).

---

## 1. Does it make sense? (honest assessment)

**Per-generator spatial placement: yes, strongly.** JASS has 6+ independent sound sources
(OSC 1–3, SUB, NOISE, KARPLUS, WAVETABLE). Placing each in its own spot in the stereo/surround
field is a genuinely expressive feature and fits the "movement layer" direction (it even becomes a
future mod-matrix **target**: Pan).

**Full 4.1/5.1 surround: makes sense as an OPT-IN, not the default.** The hard constraint is
**output hardware/host**:
- **Standalone (Windows):** needs an audio device that actually exposes ≥4 / ≥6 output channels
  (multi-out interface, or HDMI/receiver). Most users have 2-channel output — for them a 5.1 bus is
  either unavailable or silently down-mixed by the OS.
- **VST3:** the host must instantiate the plugin with a surround output bus. Many DAWs support 5.1
  buses, but the track must be configured for it; the common case is a stereo track.

So surround can't be the *only* mode without regressing every stereo user. Recommendation: **make
the engine channel-count agnostic, ship STEREO first (benefits everyone), and expose SURROUND as an
optional output-bus mode** the user selects when their setup supports it, with an automatic
stereo down-mix fallback.

**Cost/risk:** medium-high. The engine is mono today (`SynthVoice` sums all generators into one
mono sample; the STEREO module is a *pseudo*-stereo widener on that mono sum, `DSP/StereoWidth.h`).
Real spatialization means the per-voice mix must become **multi-channel** (route/pan each generator
into a channel bus), and we must decide **where the effects sit** relative to panning (pre-pan mono
FX vs. per-channel FX — the latter multiplies DSP cost). This is why it is staged.

---

## 2. Hard invariant: the current mono + pseudo-stereo must stay (switchable)

**User constraint (2026-07-21):** "die bisherige Lösung mit Mono und Pseudo-Stereo darf nicht kaputt
gemacht werden — irgendwie umschaltbar Mono / 4.1?"

→ Spatialization is an **output-mode switch**, not a replacement. A global **Output Mode** selector:

| Mode | Behaviour | Availability |
|---|---|---|
| **Mono** | today's mono sum, same to both channels | always |
| **Pseudo-Stereo** (default) | today's mono sum through the STEREO width/Haas stage (`DSP/StereoWidth.h`) | always |
| **Stereo Pan** | per-generator equal-power L/R pan (Story 10.1) | always |
| **Surround 4.0 / 4.1 / 5.1** | per-generator channel assignment (Story 10.2) | only when the device/host grants the bus; else auto stereo down-mix |
| **Binaural (Kunstkopf)** | the surround/pan placement rendered to **stereo via HRTF**, so it sounds 3D on ordinary headphones (Story 10.3) | always (stereo out) |

The legacy Mono + Pseudo-Stereo paths are kept **verbatim** as two of these modes (regression-free,
default = Pseudo-Stereo as today). PAN / channel-assignment only take effect in the Stereo-Pan /
Surround modes. The mode is a persisted, append-only param (missing ⇒ Pseudo-Stereo, so every existing
preset is unchanged).

## 3. Recommended staged plan

### Phase A — Story 10.1: Per-generator STEREO pan (ships to everyone)
Give every generator a **PAN** control (−1 L … +1 R). The per-voice render becomes 2-channel: each
generator's contribution is panned (equal-power) into L/R before the effect chain. Output stays the
existing stereo bus (`PluginProcessor.cpp:7-8`). The current pseudo-stereo STEREO module still
applies as a width stage on the panned stereo signal (or is re-scoped — see open questions).

**Why first:** delivers ~80% of the creative value, works on every device/host, no bus negotiation,
and it forces the mono→N-channel voice refactor that surround also needs — so it's the foundation.

### Phase B — Story 10.2: Optional SURROUND output (4.1 / 5.1) + per-channel assignment
Add a user-selectable output mode (Stereo / Quad 4.0 / 4.1 / 5.1). When surround is active and the
host/device supports it, expose a per-generator **channel target** (FL, FR, FM/center, RL, RR, +LFE for .1),
generalizing the Phase-A pan. Provide an automatic **stereo down-mix** when the surround bus isn't
available, so a surround preset still plays (folded to L/R) rather than going silent.

### Phase C — Story 10.3: Binaural (Kunstkopf) rendering over stereo headphones
Render the generator positions to **binaural stereo via HRTF**, so the spatial/surround placement is
heard in 3D on ordinary headphones — no surround hardware needed. This is likely the *most useful*
delivery of "surround" for typical users. **JUCE reality:** no built-in HRTF renderer or HRTF data;
two implementable routes:
- **HRIR convolution** — `juce::dsp::Convolution` per position with head-related impulse responses
  from a public dataset (MIT KEMAR / SADIE; SOFA `.sofa` is the standard format but JUCE has no SOFA
  loader → embed a small converted HRIR set). Most accurate; heavier; needs asset licensing check.
- **Parametric binaural** — per position: interaural time difference (fractional-delay), interaural
  level difference, and a head-shadow low-pass. No dataset, light CPU, "good enough" for a synth.
  Recommended starting point.

**Why last:** it consumes the same per-generator position data as Phase A/B, so it layers on top once
the spatial model exists; it's a rendering *mode*, not new routing.

---

## 4. Story 10.1 — acceptance criteria (Phase A, stereo pan)

**Given** the mono voice engine (`SynthVoice` sums generators to one mono sample)
**When** each generator gets a `<gen>Pan` param (append-only, default 0 = center) and a PAN knob in
its module body
**Then** each generator's contribution is panned equal-power into a 2-channel per-voice mix before
the effect chain, and the default (all pans centered) is **audibly identical** to today's mono-summed
output folded to both channels (regression gate)
**And** the pan params round-trip append-only in `.jass` (missing ⇒ 0/center), no `FormatVersion`
bump, old presets unaffected
**And** it is RT-safe (no alloc/lock in the callback; equal-power gains computed per block)
**And** PAN becomes available as a future mod-matrix target (wired later, not in 10.1).

## 5. Story 10.2 — acceptance criteria (Phase B, surround, opt-in)

**Given** Phase A's per-generator panning + a channel-agnostic voice mix
**When** the user selects a surround output mode AND the device/host grants a ≥4/≥6-channel output bus
(`isBusesLayoutSupported` accepts quad/5.0/5.1; `PluginProcessor` declares the alternative
`BusesProperties`)
**Then** each generator can be assigned a discrete channel (FL, FR, FM/center, RL, RR; SUB/LFE for
`.1`), routed into the corresponding output channel
**And** when the surround bus is **not** available the engine renders internally and **down-mixes to
stereo** (documented fold coefficients) so no preset is silent
**And** the effect-chain placement decision (§6 Q3) is implemented and documented
**And** stereo remains the default mode; surround presets note their channel layout; no regression for
stereo users (NFR2/NFR3 preserved, persistence append-only).

---

## 6. Open Design Questions (resolve before dev)

1. **Stereo-first vs. surround-first.** Recommend stereo-first (10.1) as the shared foundation, surround
   (10.2) opt-in. Confirm.
2. **Discrete channel routing vs. continuous pan.** Phase A = continuous L/R pan (musical, universal).
   Phase B = discrete channel assignment (user's original ask) *or* a continuous surround panner
   (angle + distance). Discrete is simpler and matches the request; a panner is more expressive. Pick.
3. **Where do the effects run?** (a) FX on the pre-pan mono sum, pan only the dry generators (cheap,
   but FX aren't spatial); (b) FX per channel after routing (spatial, but N× DSP — reverb/delay×5);
   (c) hybrid (per-generator dry pan + one shared stereo/surround FX bus). Recommend (c).
4. **Existing pseudo-stereo STEREO module** (`DSP/StereoWidth.h`) — RESOLVED by §2: it is **kept** as
   the "Pseudo-Stereo" output mode (the default, unchanged). Remaining sub-question: should its
   width/Haas stage ALSO be selectable on top of Stereo-Pan/Surround, or only in Pseudo-Stereo mode?
5. **The `.1` / LFE.** Auto low-pass a channel to LFE, route SUB there, or expose an LFE send? Optional.
6. **Standalone device config.** How to request a multi-channel output device in the JUCE standalone
   wrapper, and how to surface "surround unavailable → stereo" to the user.
7. **Per-voice vs. per-module placement.** Pan is per *generator module* (all its voices share the
   placement) — confirm (vs. per-voice spread, a different feature).
8. **Binaural HRTF approach.** Parametric (ITD/ILD/head-shadow, no assets — recommended first) vs.
   HRIR convolution with a real dataset (`juce::dsp::Convolution`, needs embedded HRIRs + a licence
   check; SOFA has no JUCE loader). Also: does binaural apply only in its own mode, or as an optional
   "headphone" toggle over Surround?

---

## 7. Dev context (anchors)

- **Output bus (stereo today):** `Source/PluginProcessor.cpp:7-8`
  (`BusesProperties().withOutput("Output", AudioChannelSet::stereo(), true)`). Surround = alternative
  layout + `isBusesLayoutSupported`.
- **Mono voice sum:** `Source/Audio/SynthVoice.{h,cpp}` — the generators are summed into one mono
  sample here; this is the block that must become channel-aware (per-generator contribution → pan →
  channel buffer). Confirm the exact sum point before editing.
- **Pseudo-stereo:** `Source/DSP/StereoWidth.h` + the STEREO module (`Source/Modules/StereoSpecs.h`).
- **Param/spec pattern:** add `<gen>Pan` per generator via each `Source/Modules/*Specs.h` (OSC via the
  `osc(i)` factory, plus SUB/NOISE/KARPLUS/WAVETABLE) — one line per param, PAN knob in the body.
- **Persistence:** append-only in the nested `.jass` (missing ⇒ default), no `FormatVersion` bump
  (same contract as the mod-matrix / enable-bool additions).
- **Constraints:** APVTS single source of truth; no alloc/lock on the audio thread; verification =
  clean build + running app + ear (no unit tests).

---

## 8. Scope guard

This is a **DSP + I/O** change, not UI-only. It is the first change to the output bus. Keep Phase A
surgical (pan the existing mono contributions into 2 channels; default byte-identical). Do NOT start
Phase B until a surround-capable test setup exists to verify on. A pragmatic outcome is that **Phase A
(stereo pan) is the deliverable** and Phase B stays a documented option unless the user has surround
hardware to validate against.
