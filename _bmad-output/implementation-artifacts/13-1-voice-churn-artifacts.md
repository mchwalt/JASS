# Story 13.1: Investigate audio artifacts under sustained voice churn (arp + long release)

Status: draft (investigation)

<!-- Raised 2026-07-29/30 while ear-testing the "Kopfkino" demo preset (Story 10.4 follow-up). -->

## Symptom

With the ARP running (6 steps/s, gate 0.4) and ADSR release 0.6 s, holding a note produces audible
disturbances ("zeitliche Überlappungen") that set in after ~2.5 s. User-bisected facts:

- **Not Karplus** — occurs with KARPLUS disabled (OSC-only pad).
- **Onset scales with NOTE COUNT, not time:** halving the arp rate (6 → 3/s) moves the onset from
  ~2.5 s to ~5 s — both ≈ 15 notes ≈ two full cycles through the 8 voices.
- **Release-dependent:** ADSR release 0.6 → 0.1 makes it disappear entirely.

## What is already ruled out (code reading, 2026-07-30)

- `Arpeggiator` emits note-off strictly before the next note-on (gate countdown + safety release
  at step time) — no note-off leaks.
- `AdsrEnvelope` release is LINEAR over exactly `release` seconds (no long exponential tail):
  voice lifetime = gate (~67 ms) + 0.6 s ⇒ only 4–5 of 8 voices concurrent — plain voice-pool
  exhaustion/stealing does NOT add up (and at 3/s concurrency is only ~2, yet the artifact still
  appears, just later).
- Voice free condition: `!noteOn && envelope.getStage()==Idle` → `clearCurrentNote()` — looks sound.

## Hypotheses to test (instrument, don't guess)

1. JUCE `Synthesiser` same-note handling: `noteOn` tail-offs the still-releasing same-note voice —
   does the interaction of re-released voices with `findFreeVoice` degrade after full pool cycles?
2. Voice stealing does occur after all (count `stopNote(allowTailOff=false)` calls — a hard stop is
   an audible click).
3. Deterministic-phase overlap: `osc.reset()` at startNote gives every same-pitch voice identical
   phase; tails at fixed offsets comb — but this is steady-state by ~1 s, which does not explain
   the 15-note onset by itself.
4. Something reused-voice-related that is NOT reset in `startNote` (per-voice ChannelStrip filter
   state, …) — second reuse cycle ≈ note 15 matches the onset.

## Suggested instrumentation

Debug build with atomic counters (steals / same-note tail-offs / max concurrent voices), dumped
once per second; reproduce with the Kopfkino settings (arp 6/s, release 0.6). The counter values
at the 2.5 s mark should discriminate the hypotheses immediately.

## Workaround shipped

The "Kopfkino" demo preset ships with release 0.3 (tails no longer stack deep enough to trigger
it); the room feel comes from the ROOM reflections anyway. The engine question stays open here.
