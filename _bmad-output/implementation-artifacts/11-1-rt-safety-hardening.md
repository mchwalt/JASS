# Epic 11 — Robustness & RT-Safety Hardening

Status: done (implemented 2026-07-26 in PR #14, stories 11.1-11.4 complete)
Type: **Correctness / real-time-safety hardening** — no new features; make the existing engine
allocation-free & lock-free on the audio thread, fix latent crashes and a couple of DSP bugs.

> Source: adversarial full-codebase review (Blind Hunter + Edge Case Hunter, 8 file groups) run
> after the MOD MATRIX MODULE→PARAM work. The MOD MATRIX feature itself reviewed clean; the items
> below are **pre-existing** issues surfaced across the whole codebase. None blocked the v2026.07.x
> releases, but they are the sensible next hardening pass. Line numbers are approximate (pre-fix).

---

## Story 11.1 — Make the audio thread allocation-free & lock-free (CRITICAL)

As a JASS user (esp. in a DAW at low buffer sizes),
I want no heap allocation, locking, or data races on the audio thread,
so that the synth never xruns/dropouts or crashes under automation and fast playing.

**Findings**
- **`applyToVoice` builds `juce::String`s per voice/block** — `Parameters.h` (`ID::oscOn(i)` etc. do
  `"osc"+String(i)+"On"`), called 8× per block from `PluginProcessor.cpp:~576`. ~100+ heap allocs
  per audio callback. Fix: precompute all parameter `std::atomic<float>*` pointers once in
  `prepareToPlay` (cache by index), read the cached pointers in `processBlock` — no `String` on the
  audio thread.
- **`parameterChanged` / `updateMatrixModuleEnables` / `syncCrossModEnables` run on the audio thread**
  under host automation (VST3 `process()` → `setValueAndNotifyIfChanged`), building `std::set`/
  `std::map`<String> and calling `setValueNotifyingHost` re-entrantly — `PluginProcessor.cpp:160-299`.
  Fix: mark the auto-enable work message-thread-only (e.g. set an atomic "dirty" flag in
  `parameterChanged`, do the enable reconciliation on the existing GUI/`Timer` tick), or guard with a
  try-lock and skip on the audio thread.
- **Unsynchronized `std::map` shared across threads** — `matrixAutoEnabled`, `crossModAutoEnabled`
  (`PluginProcessor.h:~153,163`). Resolved for free once the above is message-thread-only; otherwise
  needs a lock/lock-free structure.
- **`KarplusStrong::pluck()` heap-allocates every note-on** — `KarplusStrong.h:~26` (`buffer.assign`),
  from `SynthVoice::startNote` (audio thread). Fix: allocate the max-length buffer once in `prepare`,
  keep a `length` cursor, `std::fill` the active region on pluck (no realloc).
- **`WavetableBankStore::resetToBuiltIns()` = use-after-free** — `WavetableBank.h:~343` frees banks a
  voice may still be reading via a cached `const WavetableBank*` (`WavetableOscillator::bank`). Fix:
  don't destruct in place while audio may hold the pointer — swap under a message-thread guarantee,
  keep old banks alive until the next block boundary, or reference-count / atomic-swap the bank ptr.
- **Fresh `juce::MidiBuffer` per block** for arp + mono-glide — `PluginProcessor.cpp:~618,710`.
  Fix: make them members `reserve()`d in `prepareToPlay` (like `arpHeldScratch` etc.), `clear()` per
  block.
- **`dynamic_cast<SynthVoice*>` in the per-voice hot loop** — `PluginProcessor.cpp` (several). Fix:
  `static_cast` (only `SynthVoice` is ever added).

**Acceptance Criteria**
**Given** a debug/instrumented build (or manual audit)
**When** notes are played, voices stolen, and parameters automated from the host
**Then** `processBlock` (and everything it calls on the audio thread) performs **no heap
allocation, no lock, and no `juce::String` construction**; the auto-enable/cross-mod reconciliation
runs on the message thread only; `matrixAutoEnabled`/`crossModAutoEnabled` are never touched
concurrently; and Karplus/Wavetable no longer allocate or free memory that the audio thread uses.

---

## Story 11.2 — DSP correctness fixes

As a JASS user,
I want the noise generator, foldback distortion and effect edge-cases to behave correctly,
so that patches sound as intended and can't hang or click.

**Findings**
- **Pink/Blue noise is broken** — `NoiseGenerator.h:~59-76`: the Voss-McCartney row selection always
  exits on row 0 (`+1` always flips bit 0), so `pinkRows[1..15]` are dead → output is attenuated
  white, not −3 dB/oct pink; Blue (derived from pink) inherits it. Fix: correct the trailing-zero /
  changed-bit row index (proper Voss-McCartney update).
- **Foldback distortion can hang** — `Effects.cpp:~5-14`: `while (x>1||x<-1) x=±2-x;` has no cap;
  `±Inf` loops forever, large finite input is O(n). Fix: guard non-finite input, cap iterations (or
  use a closed-form triangle fold).
- **Dry signal hard-clipped even when effect ~bypassed** — Distortion/Wavefolder/Bitcrusher clamp the
  *crossfaded* output (`Effects.cpp:~32,50,72`), so at `mix<1` (esp. `mix==0`) the dry path is
  clipped when `|input|>1`. Fix: clamp only the wet path (match Delay/Chorus/Reverb).
- **Reverb: no empty-buffer guard; `reset()` permanently disables** — `Effects.cpp:~306-340`. Fix:
  guard degenerate sample rates like the sibling effects; have `reset()` clear state without leaving
  `initialized=false` forever.
- **Delay: `static_cast<int>(time*sr)` before clamp** can be UB at near-zero host tempo —
  `Effects.cpp:~92`. Fix: clamp in `double` first, then cast.
- **Oscillator/WavetableOscillator freeze phase when `amplitude<0.001`** — `Oscillator.cpp:~21`,
  `WavetableOscillator.h:~33`: skips the phase increment, so AMP-modulated-to-zero (tremolo) causes
  pitch/phase drift. Fix: advance phase before the amplitude early-out (or don't early-out on the
  phase update).

**Acceptance Criteria**
**Given** each affected DSP unit
**When** driven with the relevant edge input (Pink/Blue selected; ±Inf/large into foldback; effect at
`mix=0` with `|input|>1`; degenerate sample rate; amplitude modulated to ~0)
**Then** Pink/Blue produce the correct −3 dB/oct-ish spectrum; no effect hangs; a near-bypassed
effect passes the dry signal without clipping; reverb never indexes an empty buffer nor stays
disabled after `reset()`; and a tremolo'd oscillator keeps correct pitch.

---

## Story 11.3 — UI/lifetime & threading robustness

As a JASS user,
I want the editor to never crash on teardown or read torn scope/spectrum data,
so that closing/reopening the plugin window (routine in a DAW) is safe.

**Findings**
- **Async callbacks capture raw `this`/`Rack&` without a lifetime guard** — MODULES `CallOutBox`
  (`PluginEditor.cpp:~594`), right-click title `PopupMenu::showMenuAsync` (`~1108`), `FileChooser`
  completion in `ModuleFrame.cpp:~258`, processor-ctor `MessageManager::callAsync`
  (`PluginProcessor.cpp:~61`). Fix: `juce::Component::SafePointer` / weak refs (the standalone
  title-bar path already shows the pattern).
- **`EnvelopeDisplay::timerCallback` null-derefs `pA/pD/pS/pR`** while `paint()` guards them —
  `PluginEditor.h:~53` vs `.cpp:~308`. Fix: guard in `timerCallback` too (or resolve via `ID::`).
- **`rackOwned` destroyed before `rackBody`** — `PluginEditor.h:~189-190`: frames hold raw `Component*`
  to editor-owned Display components freed first. Fix: declare `rackBody` after `rackOwned` (reverse
  member order) so frames die first.
- **WaveformCapture data race** — `WaveformCapture.h:~19-51`: audio-thread `writePos`/`ring` vs GUI
  `updateSnapshot`. Fix: atomic `writePos` + acquire/release, or a lock-free double-buffer/SPSC.
- **Spectrum reads the oldest samples → visible lag** — `SpectrumDisplay.h:~176`: FFT from
  `snapshot[0..fftSize)` of a 9600-sample forward-from-trigger window. Fix: read the trailing
  `fftSize` samples.
- **Rack layout landmine: infinite loop if module footprint > rack cols** — `Rack.cpp:~674`
  (first-fit `for(;!found;++fr)`). Also negative cell width for very small widths (`~560`). Fix:
  clamp `fcols`/guard the loop; floor the grid width.
- **`setStateInformation` never `markPresetClean()`** → DAW session always reports "modified" —
  `PluginProcessor.cpp:~825`. Fix: snapshot the clean baseline after `replaceState`.

**Acceptance Criteria**
**Given** a DAW (or standalone) session
**When** the editor is closed/reopened while a popup/file dialog is open, the plugin state is
restored, and the scope/spectrum run
**Then** no use-after-free/crash occurs on teardown; scope/spectrum reads are race-free and the
spectrum tracks live audio; the rack never hangs on layout; and a freshly loaded host project does
not spuriously read as "modified".

---

## Story 11.4 — Low-priority consistency (opportunistic)

Bundle when touching the above: global "Alle OSC" Amplitude unclamped (`SynthVoice.cpp` — clamp like
the per-OSC path); LFO phase has no negative-rate wrap (`LFO.h`); `kNumSources` has no `static_assert`
vs `ModSource`; arpeggiator rebuilds/sorts the sequence every block even when unchanged
(`Arpeggiator.h:~56`); PresetIO unverified backup/`saveToFile` return values + basename-only backups +
no `FormatVersion > kFormatVersion` guard (`PresetIO.h`); LFO hidden "Target" combo persists
non-canonical strings (`LfoSpecs.h:27`); stale comments (ModMatrix.h implicit-routing, Formant cost).

---

## Notes
- **RT-safety verification has no unit tests here** — verify by audit + a debug build with a
  no-malloc-on-audio-thread guard, and by exercising in the running app / a DAW ([[feedback_ui_verification]]).
- Suggested order: **11.1 first** (the real risk), then 11.2, then 11.3; 11.4 opportunistically.
- After struct-size-affecting header edits, remember the **clean-rebuild rule** (see the build lesson
  in MEMORY.md — incremental builds caused an ABI/ODR crash during the MOD MATRIX work).
