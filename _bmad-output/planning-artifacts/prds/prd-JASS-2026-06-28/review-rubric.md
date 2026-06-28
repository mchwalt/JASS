# PRD Quality Review — JASS Standalone UI: Unified 19" Rack Layout

## Overall verdict

This is a tight, well-scoped PRD for a personal-stakes UI refactor. The problem ("cobbled together," three competing module-build paths) is stated honestly and concretely, the goals/non-goals are crisp, and the rack model is a genuine thesis rather than a backlog. It is close to build-ready. The one real risk is **done-ness clarity in the declarative framework spec**: FR3's "parameter ID + label + control type" control model does not account for the non-parameter action controls that actually exist in the code (Karplus PLUCK trigger, Wavetable LOAD WAV + dynamically-populated bank combo). Left unaddressed, a developer hits these mid-migration and either invents an ad-hoc escape hatch (defeating the "from one mold" goal) or stalls. Everything else is minor.

## Decision-readiness — strong

A decision-maker (here, the solo developer) can act on this directly. The central bet is named without hedging: §1 commits to "re-cast every sound source, modulator, filter, and effect as a uniform module," and §5 commits to a concrete rack model with three size classes. Trade-offs are surfaced with what's given up, not smoothed to neutral: §2 explicitly trades away resizing, theming, and any DSP change; §7/NFR4 honestly flags VST3 parity as "currently untested... Not a launch blocker" rather than pretending it's covered. The Open Questions (§8) are genuinely open (exact pixel constants, per-module S/M/L assignment) and correctly punted to the architecture phase rather than being rhetorical.

No findings.

## Substance over theater — strong

No furniture. There are no personas beyond the one real operator (§4 says so explicitly and declines to invent journeys — correct for a single-operator tool). The NFRs are product-specific, not boilerplate: NFR2 names the actual real-time rules (`std::atomic` channels, APVTS attachments, no allocation/locking on the audio thread); NFR3 names the actual artifacts that must not change (`.synthy` format, parameter IDs, APVTS layout). The "from one mold" framing in §3 is the real acceptance bar and is owned as qualitative rather than dressed up as a fake metric.

No findings.

## Strategic coherence — strong

The PRD has a clear thesis: *the UI looks cobbled together because it is built three different ways; unify the build path and the look follows.* Every FR serves that arc — FR1–FR4 build the one framework, FR8–FR11 enforce the size/layout discipline, FR12–FR14 migrate onto it. NFR1 ("no module defines its own `resized()`") is correctly elevated to a success gate because it is the structural root cause of the inconsistency. This is not a backlog with headings.

No findings.

## Done-ness clarity — thin

This is the weakest dimension and the one downstream story creation leans on hardest.

- **FR3's control model is under-specified for the actual control set.** It enumerates "parameter ID + label + control type" but the codebase has controls that are *not* APVTS-parameter-backed: the Karplus **PLUCK** trigger button (`onTrigger` / `triggerButton`, PluginEditor.cpp:238–244) and the Wavetable **LOAD WAV** action button plus its **dynamically populated bank ComboBox** (`wtLoadBtn`, `refreshBankSelector()`, `wtFileChooser`). A "control type" enum that only covers knob/combo/toggle bound to a parameter ID cannot express these. This is the single most likely thing a developer hits mid-migration.
- Several FRs use soft language without a testable bound: FR2 "consistent spacing," FR6 "a glance distinguishes," FR7 "visually unambiguous." For a personal UI redesign these are acceptable as *intent* (the real acceptance is the §3 qualitative "from one mold" judgment), but at least FR7's enabled/bypassed state could carry a concrete rule.
- FR1/FR3 don't state how a module *without* an enable parameter (Master, ADSR — FR5 acknowledges they exist) is expressed declaratively. FR5 covers the geometry; the declarative schema (FR3) should mirror that the enable parameter is optional (it does say "(optional)" — good — but reset is "parameter set," and Master/Stereo/etc. each reset different sets, which is fine but worth a confirming line).

### Findings
- **high** Declarative control model omits action/non-parameter controls (§FR3) — The PLUCK trigger and LOAD-WAV button + dynamic bank combo are not parameter-ID-backed and don't fit "control type + parameter ID." *Fix:* extend FR3's control-type vocabulary to include an *action control* (label + callback, no parameter binding) and a *dynamic combo* (items supplied at runtime), or explicitly list these as the framework's named exceptions so the architecture must design for them rather than discover them.
- **medium** Soft adjectives in FR2/FR6/FR7 (§FR Group B) — "consistent spacing," "at a glance," "visually unambiguous" have no testable bound. *Fix:* acceptable to lean on §3's qualitative bar, but give FR7 one concrete rule (e.g. "bypassed = body dimmed to N% + status LED off") so "done" is checkable.
- **low** Reset-parameter-set per module unspecified (§FR3/FR5) — reset currently restores per-module param lists (`initResetButton(... paramIds ...)`). *Fix:* one line confirming each module's reset set is part of its declarative definition.

## Scope honesty — strong

Omissions are explicit and do real work. §2 Non-Goals and §9 Out of Scope both fence off DSP/parameters/preset-format/theming/resizing, and §9 additionally rules out renaming the internal "Synthy" identifiers — a real, easily-assumed temptation given the project's Synthy→JASS history, so calling it out is exactly right. NFR3/NFR4 fence the technical blast radius. Open-items density (2 Open Questions, both deferred to architecture) is appropriate for the stakes. No silent de-scoping spotted.

No findings.

## Downstream usability — adequate

This PRD does feed architecture and epics (OQ2 promises a first-pass S/M/L mapping "will accompany the architecture/epics"). FR IDs are contiguous and unique (FR1–FR14, NFR1–NFR5, OQ1–OQ2). Sections are self-contained. The §5 size-class table is a clean source for architecture. The main downstream snag is the FR3 gap above (the schema a code-generator/architect would extract is incomplete re: action controls). There is no glossary, but the domain nouns ("module," "rack-unit," "size class," "zone") are used consistently, so a glossary would be ceremony here.

### Findings
- **low** No glossary, terms used consistently (§ whole doc) — acceptable for this size/stakes; note only if the architecture doc will be authored by a different pass. *Fix:* none required.

## Shape fit — strong

Correctly shaped as a single-operator, brownfield capability/UX spec. §4 explicitly declines personas and user journeys and gives the one relevant scan→find→tweak→feedback flow inline — appropriate, not under-formalized. It is not over-formalized (no UJ density, no fake metrics). Brownfield references are accurate: the three-build-path problem (OscillatorPanel / EffectPanel / inline) matches the code exactly, as does the existing GENERATORS/MODULATION/PROCESSING zone grouping (`genHeaderBounds`/`modHeaderBounds`/`procHeaderBounds`).

No findings.

## Module inventory cross-check (FR12 vs PluginEditor.h)

I walked every member of `SynthyEditor` and the three panel classes against FR12's migration list. **FR12 is complete — every module/panel/display in the code is named.** Mapping:

| Code (PluginEditor.h) | In FR12? |
|---|---|
| `osc1/osc2/osc3` (OscillatorPanel) | OSC 1–3 ✓ |
| Mix mode (`mixModeSelector` + hint + "+" label) | Mix-Mode ✓ |
| `EnvelopeDisplay` + attack/decay/sustain/release | ADSR ✓ (+ ADSR curve in §3) |
| Filter (type + cutoff + reso) | Filter ✓ |
| LFO (wave + target + rate + depth) | LFO ✓ |
| Arpeggiator | Arpeggiator ✓ |
| Distortion (type + drive + mix) | Distortion ✓ |
| `delayPanel/chorusPanel/reverbPanel` | Delay/Chorus/Reverb ✓ |
| `wavefoldPanel` | Wavefolder ✓ |
| `bitcrushPanel` | Bitcrusher ✓ |
| `karplusPanel` (+ PLUCK trigger) | Karplus ✓ |
| Noise (type + amp) | Noise ✓ |
| Sub osc (wave + octave + level) | Sub ✓ |
| Wavetable (bank + LOAD WAV + 5 knobs) | Wavetable ✓ |
| `masterKnob` | Master ✓ |
| Stereo width (width + time) | Stereo ✓ |
| `WaveformDisplay` (oscilloscope) | "display modules" ✓ (named in §3/FR11) |
| `SpectrumDisplay` | "display modules" ✓ |
| Preset header (SAVE/LOAD/RANDOM/RESET + name) | FR14 fixed chrome ✓ |
| On-screen keyboard | FR14 fixed chrome ✓ |
| Zone headers (GEN/MOD/PROC) | FR10 ✓ |

**No module, display, or top-level panel is missing from the PRD.** The gaps are not missing *modules* but missing *control kinds within named modules* (already captured as the FR3 finding above):

- **Karplus PLUCK trigger button** (`onTrigger`) — a module-local action control with no parameter binding.
- **Wavetable LOAD WAV button + dynamic bank ComboBox** (`wtLoadBtn`, `refreshBankSelector`, `wtFileChooser`) — action button + runtime-populated combo, also not parameter-backed.
- **LFO target selector** and **Mix-Mode "+" / hint labels** are ordinary combos/labels and fit FR3 fine — noted only for completeness.
- **Played-frequency FREQ display** is a *knob display behavior* (`setPlayedRatio` rewrites the FREQ knob's shown value), not a separate widget. FR13 names it correctly; just confirm the framework's knob can carry a display-transform (shown value ≠ raw parameter) so this isn't lost.

### Findings
- **high** (same root as FR3 finding) PLUCK + LOAD-WAV + dynamic bank combo are real controls the migration must preserve but the declarative model doesn't express. *Fix:* see FR3 fix above; ensure the architecture's control-type set covers action buttons and runtime-populated combos.
- **medium** FREQ knob display-transform must survive migration (§FR13) — `setPlayedRatio` means a knob's displayed value is derived (base × ratio), not the raw param. *Fix:* one line in FR4 (preserved affordances) noting the knob control supports a display-value transform, so the OSC/Wavetable FREQ readout isn't lost when knobs become generic.

## Mechanical notes

- **ID continuity:** FR1–FR14, NFR1–NFR5, OQ1–OQ2 all contiguous and unique. No gaps or dupes. No broken cross-references ("see §5," "see §6 inventory" resolve correctly).
- **Glossary:** absent; domain nouns used consistently, so not required at this stake level.
- **Assumptions index:** no inline `[ASSUMPTION]`/`[NOTE FOR PM]` tags. Given the developer is also the sole reader, acceptable — but the FR3 control-model gap would ideally have been a `[NOTE FOR PM]` at the framework boundary.
- **§6 heading vs §5 reference:** §3 and FR12 refer to "the §6 inventory"; the inventory lives in FR12 under §6 Functional Requirements — correct, just slightly indirect.
- **Window size:** §3 says "~1920×1200, as today" — confirm this matches the actual current editor size during architecture (not verified here against the code).
