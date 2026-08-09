# JASS — Module System ("one module = one place")

This document is the developer reference for the declarative module-spec
architecture: the data model, the generation pipeline, the persistence
contract, and the **recipes** for adding parameters, modules, matrix
destinations and languages.

Companion docs: [`ARCHITECTURE.md`](ARCHITECTURE.md) (layers, signal flow,
threading) · [`DEVELOPER_GUIDE.md`](DEVELOPER_GUIDE.md) (build, dependencies,
gotchas) · [`JASS_Preset_Format.md`](JASS_Preset_Format.md) (preset format) ·
[`Glossary.md`](Glossary.md) (abbreviations & domain terms — special terms
below link there).

---

## 1. Concept

A synth module (FILTER, LFO 2, SAMPLER, …) is declared **once**, in a
`Source/Modules/<Name>Specs.h` header, as a
[`ModuleSpec`](Glossary.md#modulespec). From that single declaration the
system *generates*:

1. the **[APVTS](Glossary.md#apvts) parameter layout** — `Parameters::createLayout()` is one call:
   `Modules::appendAllParameters(params)`;
2. the **rack UI descriptor** — `makeModuleDescriptor(spec)` builds the
   module's body from its parameters;
3. the **nested [`.jass`](Glossary.md#jass-format) persistence** — `Modules::writeState/readState` walk
   the specs, one JSON object per module.

**Hand-written remains** (deliberately): the [DSP](Glossary.md#dsp) `process()` of each module
(`Source/DSP/`), the param → DSP wiring in `Parameters::applyToVoice`, and the
UI bodies of ~9 modules that need dynamic providers, file dialogs or injected
display components (see [§6](#6-editor-bound-modules)).

The core compatibility contract everywhere in this system is
**[append-only](Glossary.md#append-only)**:

- `Modules::all()` order **is** the APVTS parameter order — new modules are
  appended at the end, never inserted.
- Within a spec, new parameters are appended at the end of `params`.
- Choice item order must equal the parameter's enum order
  ([`ComboBoxAttachment`](Glossary.md#attachment) maps by index).
- Mod-target enums and the matrix catalog are append-only (indices are
  persisted).
- Preset fields are never renamed; missing ⇒ factory default.

---

## 2. `ParamSpec` (audio-safe half — `Source/Modules/ParamSpec.h`)

`ParamSpec` fields in **aggregate-initialization order** (all specs use brace
init, so the order matters):

| # | Field | Meaning |
|---|---|---|
| 1 | `juce::String id` | APVTS parameter ID. Never renamed. |
| 2 | `juce::String persistKey` | Key inside the module's JSON object in `.jass` (e.g. `"Cutoff"`). |
| 3 | `juce::String uiLabel` | Knob/combo caption (`"CUTOFF"`); empty ⇒ no caption. Also the APVTS display-name suffix. |
| 4 | `Kind kind` | `Float` (default), `Int`, `Bool`, `Choice`. |
| 5 | `juce::NormalisableRange<float> range` | Float/Int range (+ interval + skew). Int uses `start/end` as min/max. |
| 6 | `float defaultValue` | Float/Int raw value · Bool 0/1 · Choice **index**. |
| 7 | `juce::StringArray choices` | Choice items — **canonical** (APVTS *and* persistence strings). |
| 8 | `juce::StringArray displayChoices` | Optional UI-only labels (e.g. `"SoftClip"` → `"Soft Clip"`); empty ⇒ use `choices`. |
| 9 | `LFOTarget modTarget` | `Off` ⇒ no live [mod ring](Glossary.md#mod-ring) on this knob. |
| 10 | `bool freqDisplay` | Knob shows the *played* frequency (base × note ratio) with write-back through the inverse. |
| 11 | `bool showInBody` | `false` ⇒ APVTS param exists but no rack control (hidden LFO `Target`, SUB `Octave`, SAMPLER `Set`). |

Generators:

- `makeParameter(spec, namePrefix)` maps `Kind` →
  `AudioParameterBool/Choice/Int/Float`. Every ID gets
  `juce::ParameterID(id, 1)`. The display name is
  `"<module title> <uiLabel|persistKey>"` — cosmetic only; state matches by ID.
- `appendModuleParameters(params, namePrefix, out)` — the loop used by the
  registry.

Notes:

- There is **no `legacyKey`** field (the concept doc proposed one). Legacy
  flat presets are handled by a one-time conversion in
  `PresetIO::applyVarFlatLegacy`, not by a permanent fallback.
- `indexIsValue` is **not** a `ParamSpec` field — it lives on `rack::Combo`
  ([§5](#5-descriptor--rendering-pipeline)), so a purely spec-driven module
  cannot have an index-is-value combo; those are editor-built.

## 3. `ModuleSpec` (UI half — `Source/Modules/ModuleSpec.h`)

| Field | Meaning |
|---|---|
| `id` | Stable slug (`"filter"`, `"osc1"`, `"mixmode"`) — rack-layout key and help-ID fallback. |
| `helpId` | Help slug override; empty ⇒ `id`. Used for sharing: LFO 1–4 → `"lfo"`, OSC 1–3 → `"osc1"`. |
| `title` | Display title (`"FILTER"`); also the APVTS name prefix. |
| `persistObject` | `.jass` object key (`"Filter"`, `"Osc1"`, `"ModMatrix"`). |
| `enableParamId` | Bool param that becomes the header on/off toggle; `""` ⇒ always on. |
| `type` | `Generator` / `Modulator` / `Processor` — identity/colour tag only. |
| `zone` | Default rack zone. |
| `size` | Grid footprint (`SizeClass`). |
| `defaultVisible` | Factory visibility (e.g. LFO 4 and COMPRESSOR ship hidden). |
| `alignRight` | Pack right within the zone row (MASTER BUS modules). |
| `params` | The `ParamSpec` list. |
| `enabledWhen` / `onReset` / `extraBody` | Hooks for derived enable state, extra reset work, and appended body elements. **Currently unused by every spec** — modules needing them are hand-built in the editor instead, because a static spec cannot capture `apvts`/`processor`. |

`makeModuleDescriptor(spec)` copies the identity fields and builds the body:
the enable param becomes the header toggle (never a body cell), `showInBody ==
false` params are skipped, `Choice` → `Combo`, `Bool` → `Toggle`, everything
else → `Knob` (with mod ring and, when `freqDisplay`, the guarded
display-transform pair). `extraBody` is appended last.

## 4. Registry (`ModuleRegistry`, `AllModules.h`)

- `Modules::all()` (`AllModules.h`) returns the ordered list of **34 module
  specs** (~190 APVTS parameters), built fresh on each call:
  17 simple modules (filter … pitchEnv), `osc(1..3)`, `crossmod`,
  `lfo(1..4)`, `modMatrix`, `string`, `wavetable`, `adsr`, the three displays,
  `presetBank`, `sampler` — the last two *appended* after everything else
  because `all()` order is the APVTS order.
- `ModuleRegistry.h` declares the three audio-safe entry points
  (`appendAllParameters`, `writeState`, `readState`); `ModuleRegistry.cpp` is
  the **single TU** that includes `AllModules.h` (and thereby the UI headers).
  `Parameters.h`/`PresetIO.h` include only the registry header — this is what
  keeps the audio layer UI-free.
- Indexed modules are **spec factories**: `Modules::osc(int i)` (per-index
  default frequencies C4/C5/C3, OSC 1 defaults to Sawtooth) and
  `Modules::lfo(int i)` (LFO 1 keeps the stable ID `"lfo"`; the hidden
  `lfo{i}Target` Choice exists only to gate the LFO and to migrate old
  presets). MOD MATRIX generates its 8 slot rows in a loop
  (`modSlot{n}Source|Module|Param|Amount`).

---

## 5. Descriptor & rendering pipeline

```
ModuleSpec ──makeModuleDescriptor()──► rack::ModuleDescriptor ──Rack::addModule()──► rack::ModuleFrame
                (+ editor injection: enabledWhen / activeWhen / Display / Action /
                   comboDependencies / slotActivity for the hand-built modules)
```

### Body-element vocabulary (`UI/rack/ModuleDescriptor.h`)

`BodyElement = std::variant<Knob, Combo, Toggle, Action, FileAction, Caption, Display>`

| Element | Fields / behaviour |
|---|---|
| `Knob` | `paramId, label, toDisplay/fromDisplay` (all-or-nothing guarded pair; `ratio ≤ 0` ⇒ identity + no write-back), `modTarget`, `activeWhen` (per-knob relevance predicate → dimmed at α 0.35, e.g. PAN knobs in Mono/Pseudo-Stereo). |
| `Combo` | `paramId, label, items` = static `StringArray` **or** provider `function<StringArray()>` (dynamic), `indexIsValue` — writes the item index straight to the param, bypassing `ComboBoxParameterAttachment` (whose `index/(numItems−1)` mapping breaks for a varying item count); such combos are re-synced param→combo by the frame timer. Spans 2 layout slots. |
| `Toggle` | `paramId, label`. |
| `Action` | button; `label, onClick, refreshes` (list of combo param-IDs to re-poll afterwards). |
| `FileAction` | async `juce::FileChooser` button; `label, onChoose(File), refreshes, startFolder, wildcard`. |
| `Caption` | static text (named to avoid the `juce::Label` clash). |
| `Display` | `component*` (**non-owning** — the editor owns lifetime, AD-5) + `slots` span. |

Auxiliary descriptor data: `ComboDependency {watchParamId, refreshParamId,
onWatchChanged}` (MOD MATRIX: the PARAM combo re-lists when its slot's MODULE
changes) and `SlotActivity {groupSize, isActive(slot)}` (MOD MATRIX slot
dimming + green activity dots).

### ModuleFrame behaviour worth knowing

- Owns every widget **and** its APVTS attachment (AD-6).
- Enable = `enableParam` AND `enabledWhen` (absent signals count as on);
  header toggle only when `enableParam` exists; disabled modules dim the body
  only (header stays lit); every module has a status LED (filled green =
  enabled).
- Reset ↺ resets **every body paramId except the enable param** (the body is
  the source of truth — no hand-maintained reset list), then calls
  `onReset`, then re-polls all dynamic combos. (`ModuleDescriptor::resetParams`
  is effectively dead.)
- A 20 Hz timer runs only when the module has dynamic state (enable, derived
  enable, combo dependencies, conditional knobs, slot activity).
- `updateLiveFeed` drives the mod rings; for `osc1/2/3` the per-OSC feed is
  addressed by the module ID.

### Size classes

`SizeClass` is named by grid footprint on the 30-column grid:
`W{cols}H{rows}` (`W2H1 … W30H2`, 21 entries). One data table
(`sizeClassSpec` → `{cols, units, slotCapacity, knobDiameter}`) defines them
all; `slotCapacity` is only a debug guard (`assertFitsClass`). Adding a width
= one new enum entry + one table row. All knobs are `KnobSize::Small` (46 px).

The counts are **absolute columns**, which is why widening the grid from 24 to
30 (Story 7.3) left every module untouched: they keep their physical size and
simply pack more per row. Only "full width" is relative — those modules moved
`W24 → W30`.

---

## 6. Editor-bound modules

Nine modules need things a static spec cannot express (dynamic combo
providers, file dialogs, injected `Display` components, predicates reading
APVTS, action buttons). Their **parameters still come from the specs**, but
their descriptors are hand-built in `SynthyEditor::buildRack()`
(`Source/UI/PluginEditor.cpp`):

| Module | Why hand-built |
|---|---|
| PRESETS | injected `PresetBankPanel` display |
| STRING – KARPLUS | PLUCK `Action` (mirrored by the Space key) |
| WAVETABLE | dynamic BANK combo (`WavetableBankStore` provider), `FileAction` LOAD WAV, `onReset` → `resetToBuiltIns()` |
| SAMPLER | dynamic SET combo with `indexIsValue`, `FileAction` LOAD (file or `.sfz`) + FOLDER (`pickDirectory`, multisample import — both copy into `%AppData%\JASS\Samples`), ROOT knob `activeWhen` (inert for mapped sets), STRETCH toggle = pitch/time decoupling via a per-voice vendored Signalsmith Stretch (Story 12.3) |
| ENVELOPE – ADSR | injected `EnvelopeDisplay` |
| MOD MATRIX | per-slot dynamic PARAM combos (`indexIsValue`), `ComboDependency`, `SlotActivity` |
| OSCILLOSCOPE / SPECTRUM | injected display components |
| KEYBOARD | injected `MidiKeyboardComponent`; enable enforced in the editor timer because JUCE's keyboard ignores `isEnabled` |
| CROSS MOD | spec descriptor + injected `enabledWhen` (both selected operand OSCs enabled) |

**Gotchas here:**

- For hand-built modules the spec's `size`/`zone`/`helpId` are **ignored** —
  the editor's `add()` helper re-derives the ID from the title
  (lower-cased, alphanumerics only: `"STRING - KARPLUS"` → `"stringkarplus"`)
  and sets the size itself (e.g. WAVETABLE spec says `W14H1`, editor uses
  `W10H1`). Change size/body of these modules **in the editor**, not the spec.
- The editor also registers factory-enable overrides:
  `setFactoryEnableDefault(oscOn(i), 1.0)` for OSC 1–3 (their spec default is
  0 for preset compatibility, but the Init patch ships with them on).
- A central hook in `addRackModule` installs the PAN-knob relevance predicate
  on every knob whose `paramId` ends in `"Pan"` (inactive in Mono /
  Pseudo-Stereo), unless the knob already has an explicit `activeWhen`.

---

## 7. Mod-matrix integration

Three data tables cooperate (all dependency-free headers under `Source/DSP/`):

1. **`ModTargets.h`** — the flat target vocabulary. One X-macro row
   `X(Enum, "PersistName", "Label", "enableParamId")` generates the
   `LFOTarget` enum, `kCount`, persist names, labels and enable IDs.
   **Append-only** (indices persisted).
2. **`ModMatrixCatalog.h` (`ModDest`)** — the two-step MODULE → PARAM UI
   catalog: `modules[]` with per-module `{label, enableId, oscIndex,
   params[kMaxParams], numParams}`. `oscIndex 0..2` = per-oscillator target,
   `-1` = global ("Alle OSC"). **Append-only**: the module list is persisted
   as a *string*, the param as an *int index* — new modules/params go at the
   end, existing ones never move (which is why the list is only A→Z up to
   WAVETABLE; later modules were appended). Reordering params requires a real
   preset migration (v6 did exactly that — `v5ParamRemap`).
3. **`ModMatrix.h`** — sources, slots and the accumulate function
   (`kNumSlots = 8`, `kNumSources = 6`).

**How a parameter becomes a matrix destination:**

- *Per-voice target*: append an `LFOTarget` row → add a `Param` entry to the
  owning module's catalog row (bump `numParams`) → add the apply block in
  `SynthVoice::renderNextBlock` (capture base per block, apply with
  scale + clamp behind the `tActive[]` guard, restore in the epilogue).
- *Global (master-bus) target*: same two table entries, but the apply site is
  the block-rate `gMod[]` path in `SynthyProcessor::processBlock` (LFO sources
  only).
- If the target module should auto-enable, its enable ID must be in the
  processor's `managed[]` list.

**Sizing constants and their coupling** (grow together, and see the
clean-rebuild rule below):

| Constant | Couples to |
|---|---|
| `ModDest::kMaxParams` (6) | `Param params[kMaxParams]` **and** the MOD MATRIX `PARAM` param range |
| `kOscRingSlots` (6) | `rack::LiveModFeed::osc[3][6]` and `ModDest::oscParamSlot` numbering (FREQ=0, AMP=1, DETUNE=2, FB=3, VOICES=4, PAN=5) |
| `ModTargets::kCount` | `LiveModFeed::byTarget`, `ModMatrixConfig::kNumTargets`, `gMod[]`, per-voice offset arrays |
| `kNumPanGenerators` (9, `ChannelStrip.h`) | per-voice panner arrays |

> ⚠️ **[Clean-rebuild](Glossary.md#clean-rebuild) rule ([ODR](Glossary.md#odr) trap).**
> These constants size structs that voices embed **by value** in headers.
> Growing them changes struct sizes; an *incremental* MSBuild then mixes
> [TUs](Glossary.md#tu) with old and new layouts →
> heap corruption / `0xC0000005` at startup. After changing any header-struct
> size: build with **`/t:Rebuild`**. (Bitten repeatedly: `ModSlot` growth,
> `kNumPanGenerators` 7→9, stereo `WaveformCapture`.)

---

## 8. Persistence contract

Written by `PresetIO::toVar` / read by `PresetIO::applyVar`
(`Source/Audio/PresetIO.h`); the per-module work is spec-driven
(`Modules::writeState/readState`). Shape ([FormatVersion](Glossary.md#formatversion) **6**):

```jsonc
{
  "FormatVersion": 6, "Name": "…", "Modified": false,
  "Filter":  { "Enabled": true, "Type": "Lowpass", "Cutoff": 500.0, "Resonance": 2.5 },
  "Osc1":    { "Enabled": true, "Wave": "Sawtooth", "Freq": 261.63, … },   // numbered objects, not arrays
  "Lfo1":    { … }, … "Lfo4": { … },
  "ModMatrix": { "On": true, "Slot1Source": "LFO 1", "Slot1Module": "FILTER",
                 "Slot1Param": 0.0, "Slot1Amount": 0.5, … },               // flattened numbered keys
  "Sampler": { "Enabled": false, …, "File": "CH_01" },                     // File injected by PresetIO (by NAME)
  "RackLayout": { … }                                                      // only when non-default
}
```

Rules:

- **Choice → canonical string** (from `ParamSpec::choices`), matched back by
  name; unknown string ⇒ keep default. Bool → JSON bool; Int/Float → number.
- **Missing field / missing module object ⇒ factory default** — `applyVar`
  factory-resets *everything* first, then layers the file on top.
- The enable key name is per-module (`"Enabled"` mostly; `Master`, `CrossMod`
  and `ModMatrix` use `"On"`) — it is just a `persistKey`.
- Adding fields never bumps `kFormatVersion`; only *value/meaning* changes do
  (then write a migration — see the chain in `PresetIO.h` and
  [`JASS_Preset_Format.md`](JASS_Preset_Format.md)).
- Non-spec side channels handled directly by `PresetIO`:
  [`RackLayout`](Glossary.md#racklayout) (mirrored from the `apvts.state`
  string property), `Sampler.File` (re-resolved by name from
  `%AppData%\JASS\Samples` on load), [`PresetBanks.json`](Glossary.md#preset-bank)
  (global app file, not part of presets).

---

## 9. Help & localization

- One markdown file per help topic: `Resources/EN/<id>.md` +
  `Resources/DE/<id>.md` (module IDs + `zone-*` IDs). The **filename stem is
  the help ID**.
- Embedded via **one [`juce_add_binary_data`](Glossary.md#binary-data) target per language**
  (`JASS_HelpEN`/`JASS_HelpDE` with distinct `NAMESPACE` + `HEADER_NAME` —
  JUCE derives symbols from basenames only, so EN/DE files would collide in a
  single target).
- `HelpTextStore` registers languages explicitly; `has(id)` checks EN (the
  base language) and decides whether an info icon exists at all; `get`
  falls back DE → EN → empty.
- `helpId` aliasing lets instances share one text (LFO 1–4 → `lfo.md`) while
  the panel title stays instance-specific.
- The renderer (`UI/MarkdownRenderer.h`) supports paragraphs, `#`/`##`/`###`
  headings, `-`/`*` bullets, `**bold**`, `*italic*`, `` `code` `` — no tables,
  links or nested lists.
- **Adding a language** = new `Resources/<LANG>/` folder + one
  `juce_add_binary_data` target in `CMakeLists.txt` + one `registerLanguage`
  line in `HelpTextStore`.

---

## 10. Recipes

### 10.1 Add a parameter to an existing module

1. **Spec** — append one `ParamSpec` at the **end** of `m.params` in
   `Source/Modules/<Name>Specs.h` (append-only!).
2. **ID + wiring** — add the ID constant in `Parameters.h` (`namespace ID`)
   and the DSP wiring line in `applyToVoice`. Indexed IDs go through
   `JASS_INDEXED_ID` and must be added to `warmIndexedIds()`.
3. **If modulatable** — follow [§7](#7-mod-matrix-integration): `ModTargets`
   row (end), catalog `Param` entry (end), apply block (per-voice or global),
   `managed[]` if it should auto-enable.
4. **Persistence** — nothing: the spec drives it; old files simply lack the
   field ⇒ default. No `kFormatVersion` bump for additive fields.
5. **Help** — update `Resources/{EN,DE}/<helpId>.md` (embedded ⇒ rebuild).
6. **UI** — nothing for spec-driven modules; one body element in
   `buildRack()` for the hand-built ones (maybe a wider `SizeClass`).
7. `CHANGELOG.md` entry.

### 10.2 Add a new module

1. Write `Source/Modules/<Name>Specs.h` (`inline ModuleSpec <name>()`);
   header-only ⇒ no CMake change.
2. Register in `AllModules.h`: `#include` + append `<name>()` at the **very
   end** of `Modules::all()`.
3. `Parameters.h`: ID constants + `applyToVoice` wiring.
4. DSP class in `Source/DSP/<Name>.h` (convention: header-only,
   `setSampleRate(double)` + per-sample `process()`), embedded in
   `SynthVoice`, prepared in `prepareToPlay`. A new **`.cpp`** must be added
   to `target_sources` in `CMakeLists.txt`.
5. Editor: `addRackModule(makeModuleDescriptor(Modules::<name>()))` — or a
   hand-built descriptor if it needs displays/actions/dynamic combos.
6. Help texts `Resources/EN/<id>.md` + `Resources/DE/<id>.md` (CMake
   re-globs; build the help targets with `/t:Rebuild /nodeReuse:false`).
7. Matrix exposure, PAN slot, zone placement as needed (see §7 and
   ARCHITECTURE.md §8). Consider `defaultVisible = false` for large modules —
   a big new module can trigger the global auto-fit downscale.
8. `CHANGELOG.md`.

### 10.3 Checklists for the other seams

- **New LFO**: `kNumLFOs`++ (`DSP/LFO.h`) + append `ModSource` + one
  `kLfoSourceIdx` entry (the spec factory, editor loop and persistence follow
  the constant).
- **New matrix slot count**: `ModMatrixConfig::kNumSlots` (spec loop follows).
- **New size class**: one `SizeClass` enum entry + one `sizeClassSpec` row.
- **New wavetable/sample assets**: drop files into `Wavetables/`/`Samples/`
  (globbed by CMake, embedded, [seeded](Glossary.md#seeding) on first run —
  idempotent, existing user files never overwritten).
- **New demo preset**: file in `DemoPresets/` + CMake reconfigure +
  optionally a `defaultPresetBank()` slot + README. Seeding is idempotent —
  replace the AppData copy manually when re-tuning an existing preset.

---

## 11. Known limitations / current quirks

- `ModuleSpec::enabledWhen` / `onReset` / `extraBody` are declared but unused
  by every spec — all non-pure UI lives in `buildRack()` (a spec cannot
  capture `apvts`/`processor`).
- `ModuleDescriptor::resetParams` is dead; reset derives from the body.
- Spec `size`/`zone`/`helpId` are ignored for the hand-built modules ([§6](#6-editor-bound-modules)).
- `_bmad-output/project-context.md` predates the AppData rebrand and the
  format evolution (it still says `%AppData%\Synthy` and `kFormatVersion = 1`)
  — where it conflicts with this document, this document wins.
