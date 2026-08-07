# Story 12.6: asynchronous sample loading

Status: **done** — implemented + user-verified 2026-08-08 ("alles OK").

## Story

As a player,
I want JASS to open immediately instead of waiting for its sample library,
so that starting the app does not cost ten seconds just because a grand piano is installed.

## Context (what it was)

`preloadSamples()` ran in the processor's constructor and decoded EVERY installed set before
the window appeared — with both four-layer grand pianos that is ~1.2 GB. On top of that, the
restored LiveState resolves its own set by name, which decoded that set a second time on the
message thread. Measured before: **10 s to a visible window** with a piano patch restored.

Noted as deferred work after the 12.2/12.3 review ("a proper fix is async loading with
progress — its own story if it ever bites"). It bit.

## What was built

1. **Background loader** (`SynthyProcessor::SamplePreloadThread`) — owns the folder scan. Started
   in the constructor, stopped first in the destructor.
2. **Priority queue of one**: `PresetIO::requestSamplerSet` is a hook the processor installs.
   `applyVar` no longer decodes a set itself; it hands the NAME to the loader, which pulls that
   set to the front, then selects it via `selectSamplerSet` on the message thread. A generation
   counter drops the result if a newer preset asked for something else meanwhile.
3. **LiveState guard** (`PresetIO::pendingSamplerSetName`): while a requested set is in flight the
   SET index still points at another set, and the LiveState is written every 1.5 s — `toVar`
   persists the pending NAME instead, so a save mid-load cannot rewrite the patch to a different
   instrument. Cleared on selection, on a failed lookup, and when the user picks a set by hand.
4. **Publication-only lock** in `SampleBankStore`: `writerLock` is taken by `append()`, never
   across a decode. Holding it across a set's decode deadlocked the UI in practice (message
   thread waiting for the loader's piano). Concurrent decodes of the same name are resolved at
   publication — first one wins, the loser is dropped.
5. **Per-zone abort**: `loadFromEntries` polls `shouldAbort` per zone, so closing the app stops a
   running piano decode in milliseconds instead of after the whole set.
6. **SET combo follows**: the editor's timer re-lists it when the set count changes (12.6 needs
   this — entries now appear over time).

## Measured (this machine, both pianos installed)

| | before | after |
|---|---|---|
| window visible, no sampler patch | ~10 s | **0.7 s** |
| window visible, LiveState = SalamanderPiano | 10.0 s | **0.6 s** |
| close during load | ignored (bulk ran to completion) | **0.2 s** |

Trade-off accepted by the user: for a few seconds after startup the sampler is silent while its
set loads; it then sounds without any interaction.

## Verification

Release rebuild, no warnings. Startup timings as above. LiveState round-trip checked end to end
(`SalamanderPiano` → start → 35 s → clean close → `SalamanderPiano`). Closing early was compared
against a baseline build without this story: the JUCE standalone ignores a WM_CLOSE in the first
second either way — not a regression, and the second close request always worked.

## Notes for later

- The loader stays alive and idles (`wait(500)`) so a set imported later in the session can still
  be requested by a preset.
- No progress UI. If loading ever needs to be visible, that is the place to add it.
