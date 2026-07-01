---
title: "Sprint Change Proposal — Rack-UI Prototype Formalization"
project: JASS
created: 2026-07-01
author: Michael Walter (via correct-course)
status: approved-and-applied
approved: 2026-07-01
decision_master_stereo: "folded into Story 3.1 (no separate Story 3.0)"
scope_classification: Moderate
triggering_stories: [1.3]
affected_artifacts: [prd, architecture-spine, epics, deferred-work, project-context, C# Synthy app]
---

# Sprint Change Proposal — Rack-UI Prototype Formalization

## Section 1 — Issue Summary

**Problem statement.** During Story 1.3 (Rack grid layout), a throwaway sample rack was
built on top of the completed engine to see the layout working end-to-end. Trying it in the
running app produced three design decisions that **diverge from the approved PRD and
Architecture Spine**. The code (committed in `444871f`) already implements them, so the
specs and reality are now out of sync. This proposal formalizes the three decisions so the
prototype can graduate from "sample" to "adopted" before Stories 1.4/1.5 build on it.

**How discovered.** Hands-on prototyping in the standalone app after 1.3 integration
(the first time a real rack was visible). Recorded in
`implementation-artifacts/deferred-work.md` (2026-06-28) as "pending formalization —
run correct-course".

**Evidence (verified in code, commit `444871f`):**
- `Source/UI/rack/ModuleDescriptor.h:15` — `enum class SizeClass { XS, S, M, L, XL }`
  with 12-column spans (2×1 / 3×1 / 4×1 / 4×2 / 6×2); `slotCapacity` demoted to a
  "generous debug guard" (`ModuleDescriptor.h:119-131`).
- `Source/Audio/Parameters.h` — four **new** `AudioParameterBool` enable params
  (`filterOn:35`, `distortionOn:41`, `lfoOn:89`, `noiseOn:102`) built in `createLayout()`;
  engine gates on them in `PluginProcessor` (`filterOn:308`, `distortionOn:314`, `lfoOn:348`,
  `noiseOn:352`); `"Off"` removed from those four choice combos.
- Master + Stereo removed from the header and re-cast as rack modules in a new MASTER BUS
  zone (per deferred-work note + project memory; header flattened, "Current State" moved to
  the Save/Load cluster, title centred).

---

## Section 2 — Impact Analysis

### Epic Impact
- **Epic 1 (Foundation & Generators)** — Stories 1.1/1.2/1.3 are ✅ done. The size-class model
  they were written against (S/M/L, 1×1/2×1/2×2, slotCap 3/6/12) is superseded by the new
  XS–XL 12-column model. **Story 1.1 and 1.3 acceptance text is now historically inaccurate**
  but the code is ahead of it — reconcile the AC wording, don't reopen the stories.
- **Epic 3 (Chrome & Final Integration)** — materially changed:
  - **Story 3.1 (Global header chrome)** no longer builds Master/Stereo *into the header*.
    Master + Stereo become **rack module descriptors** in the MASTER BUS zone. Story 3.1
    shrinks to preset controls + "Current State" indicator only.
  - **Story 3.3 (Remove legacy layout code)** now also covers deleting the old header
    Master/Stereo controls (they moved into the rack, not just restyled).
- **Epic 2** — largely unaffected structurally, but every module descriptor it migrates now
  uses the XS–XL span model instead of S/M/L. The Enable-split gives Filter/Distortion/LFO
  a real header enable (they were previously "Off"-in-combo), which *simplifies* those
  descriptors (a normal `enableParam`, like every other module).

### Artifact Conflicts

| Artifact | Conflict | Needs |
|---|---|---|
| **PRD §5 + FR8** | Size-class table = 3 classes (S/M/L, footprint 1×1/2×1/2×2). Reality = 5 classes on a 12-column grid. | Rewrite §5 table + FR8 wording (still "single data-driven table", now 5 rows). |
| **PRD FR14** | Master + Stereo declared as fixed **header chrome**, "not rack modules". Reality = they ARE rack modules in a MASTER BUS zone. | Rewrite FR14; header keeps preset controls + Current State only. |
| **PRD §2 Non-Goals + §9 + NFR3** | "No new parameters / APVTS untouched." Reality = 4 new enable bools added to APVTS. | Amend: APVTS gained 4 append-only enable bools; **`.synthy` on-disk format still unchanged** (`"Off"` marker mapped via `PresetIO::choiceOrOff`). Preset interop preserved; parameter-ID append-only rule respected. |
| **PRD §5 / FR10** | Three zones (GENERATORS/MODULATION/PROCESSING). Reality adds a **MASTER BUS** zone. | Note the 4th zone (top row, right-aligned). |
| **Architecture AD-2** | "S=1×1, M=2×1, L=2×2; slotCapacity 3/6/12 enforced by assertion; body = cols×3." | Rewrite AD-2 to the 12-column span model; `ModuleFrame` body derives `nCols=ceil(slots/units)` from content; slotCapacity is now a generous debug guard, not the layout driver. |
| **Architecture Structural Seed / FR14 note** | "Master + Stereo live in the top header, not rack modules." | Update to MASTER BUS zone modules. |
| **deferred-work.md** | Lists these as "pending formalization". | Mark formalized; keep the standing **C# Synthy UI ToDo**. |
| **project-context.md** | AD-2 size model referenced indirectly; no param-model note. | Add a line: 4 enable bools added (append-only), `.synthy` unchanged. |

### Technical Impact
- **Audio engine:** already implemented and building — no further code change required by this
  proposal. The engine gates on the four new bools; `.synthy` round-trips via `choiceOrOff`.
- **Cross-app interop:** the **C# Synthy app must mirror the UI/model change** (add enable
  toggles for LFO/Noise/Filter/Distortion, drop `"Off"` from those combos, derive the toggle
  from the `"Off"` string). Pure C#-side work; no format change. This is the one item that
  leaves the C++ repo.
- **No preset/format migration** and no `kFormatVersion` bump needed.

---

## Section 3 — Recommended Approach

**Selected path: Option 1 — Direct Adjustment (documentation reconciliation).**

- **Option 1 (Direct Adjustment) — VIABLE, chosen.** Amend PRD / Architecture / Epics /
  deferred-work / project-context to match the already-built, already-verified prototype.
  Effort: **Low** (doc edits). Risk: **Low**. The prototype was validated in-app; we are
  ratifying reality, not designing anew.
- **Option 2 (Rollback) — NOT viable.** Reverting to S/M/L + header Master/Stereo would throw
  away verified, better layout work (12-col grid is more flexible; "everything is a module" is
  the PRD's own core principle — Master/Stereo as modules is *more* consistent, not less).
  No simplification gained; pure loss.
- **Option 3 (MVP Review) — NOT needed.** MVP scope is intact; if anything the changes tighten
  it (uniform enable across all modules, one coherent grid). No goals dropped.

**Rationale.** The three decisions each move the design *toward* the PRD's stated goal
("every module from one mold"): Master/Stereo as modules removes a chrome special-case; the
Enable-split makes Filter/Distortion/LFO/Noise behave like every other module; the 12-column
grid decouples layout from knob diameter (fixing AD-3's coupling risk). The only genuine
scope expansion is the 4 new APVTS params — acceptable because it is append-only and the
`.synthy` format is untouched, so the "no format impact" guarantee (the one that actually
protects users) holds.

---

## Section 4 — Detailed Change Proposals

### 4.1 — PRD (`prds/prd-JASS-2026-06-28/prd.md`)

**Change A — §5 size-class table (lines 48–58).**

OLD:
```
| Class | Footprint (W × H) | Intended for | Capacity |
| S | 1 column × 1 rack-unit | minimal modules | enable + reset + up to ~3 controls |
| M | 2 columns × 1 rack-unit | mid modules | enable + reset + up to ~5 controls, optional small inline indicator |
| L | 2 columns × 2 rack-units | rich modules / graphical displays | many controls and/or a full graphical display |
```
NEW:
```
The rack is a fixed 12-column proportional grid (raster decoupled from knob diameter).
Size classes are column spans:
| Class | Footprint (cols × units) | Intended for |
| XS | 2 × 1 | 1–2 controls (e.g. Master, Mix-Mode) |
| S  | 3 × 1 | small modules |
| M  | 4 × 1 | mid modules |
| L  | 4 × 2 | rich modules (ADSR: knobs + curve) |
| XL | 6 × 2 | wide visualisers (scope / spectrum) |
A module declares only its class; the rack places it. A module's body derives its internal
column count from its content (knobs centred), not from the knob diameter.
```
Rationale: matches implemented `SizeClass` enum + 12-col grid; replaces the coupled S/M/L model.

**Change B — FR8 (line 78).**

OLD: "**3 are defined and in use (S/M/L)** … (a 4th class is anticipated but not implemented now)"
NEW: "**5 are defined and in use (XS/S/M/L/XL)** on a 12-column grid; the set stays extensible by one table row."
Rationale: the anticipated extension already happened; table is still single-source.

**Change C — FR14 (lines 86).**

OLD: "The **global header** (preset SAVE/LOAD/RANDOM/RESET, preset-name/'Current State'
indicator, plus the master-bus **Master volume** and **Stereo** width/time) and the on-screen
keyboard remain as fixed chrome … they are not rack modules."
NEW: "The **global header** (preset SAVE/LOAD/RANDOM/RESET, centred title, preset-name/'Current
State' indicator in the Save/Load cluster) and the on-screen keyboard remain as fixed chrome.
**Master volume and Stereo (width/time + enable) are themselves rack modules** in a dedicated
**MASTER BUS** zone (top row of the rack, right-aligned) — consistent with 'every module from
one mold'. The legacy header Master/Stereo controls are removed."
Rationale: matches the built MASTER BUS zone; header flattened.

**Change D — §2 Non-Goals / §9 Out of Scope / NFR3 (lines 27, 91, 101).**

Add a clarifying note (do not silently contradict): "The UI redesign added **four append-only
enable bools** to the APVTS (`filterOn`, `distortionOn`, `lfoOn`, `noiseOn`) so those four
modules gain a real header enable like every other module. This is the sole parameter change;
it is append-only (no ID renamed/reordered) and the **`.synthy` on-disk format is unchanged**
— `\"Off\"` remains the disabled marker, mapped via `PresetIO::choiceOrOff`. Preset interop
with the C# app is preserved."
Rationale: keeps NFR3's real guarantee (format/interop) honest while recording the actual param addition.

**Change E — §5 zones + FR10 (lines 46, 80).**

Note the **MASTER BUS** zone as a 4th zone (top, right-aligned) alongside
GENERATORS/MODULATION/PROCESSING.

### 4.2 — Architecture Spine (`architecture-JASS-2026-06-28/ARCHITECTURE-SPINE.md`)

**Change F — AD-2 (lines 38–43).** Rewrite the rule:
- Grid = **12 columns** × rack-units, uniform gutters.
- Size classes = column spans: **XS 2×1, S 3×1, M 4×1, L 4×2, XL 6×2** (single data-driven table).
- `ModuleFrame` body derives its internal column count **from content** (`nCols = ceil(contentSlots / units)`, knobs centred) — **not** `cols×3`; layout is decoupled from knob diameter.
- `slotCapacity` remains in the table but is now a **generous debug guard** (`jassert`), not the layout driver.
- Remove the "S=3/M=6/L=12 enforced by assertion drives layout" claim.

**Change G — Structural Seed FR14 note (line 130).** Master + Stereo move **out of** the
"fixed chrome" list **into** rack module descriptors (MASTER BUS zone). Chrome now = preset
controls + Current State + keyboard only.

**Change H — AD-3 note (lines 45–48).** Optional: record that the 12-col decoupling reduces
AD-3's "coupled to knob diameter" risk (per-size knob sizes now cosmetic, not layout-driving).

### 4.3 — Epics (`epics.md`)

**Change I — Story 3.1 AC (lines 234–240).** Remove Master/Stereo from the header-chrome
deliverable; header = preset SAVE/LOAD/RANDOM/RESET + Current State. Add: "Master and Stereo
are migrated as **rack modules in the MASTER BUS zone** (not header chrome)."

**Change J — coverage [DECIDED: folded into Story 3.1].** Master + Stereo descriptor migration
is folded into **Story 3.1** (renamed "Global header chrome + MASTER BUS modules") — no separate
Story 3.0. They are rack modules now, not chrome.

**Change K — Story 3.3 (lines 262–267).** Add deletion of the old header Master/Stereo
controls to the legacy-removal scope.

**Change L — Stories 1.1/1.3 AC reconciliation (lines 116, 144).** Update the S/M/L
footprint references to the XS–XL 12-column model (historical accuracy; stories stay ✅ done).

### 4.4 — deferred-work.md

**Change M.** Move the "Prototype decisions pending formalization" block to a "Formalized
(2026-07-01, this proposal)" state. **Keep** the C# Synthy enable-mirror ToDo as an open
cross-app item.

### 4.5 — project-context.md

**Change N.** Add one line under Preset & State (or Parameters): the 4 append-only enable
bools + "`.synthy` format unchanged, `"Off"` mapped via `choiceOrOff`".

---

## Section 5 — Implementation Handoff

**Scope classification: Moderate** (backlog + spec reorganization; no fundamental replan).

| Recipient | Responsibility |
|---|---|
| **Developer (docs) — you / next session** | Apply edits A–N. Pure documentation; no C++ code change (code already built + verified in `444871f`). |
| **Story author (bmad-create-story)** | If a new Story 3.0 "MASTER BUS zone" is chosen over folding it into 3.1, author it. |
| **C# Synthy maintainer (out-of-repo)** | Mirror the enable-split in the C# app UI/model (open ToDo, tracked in deferred-work.md). Not blocking the C++ rack work. |

**Success criteria.**
- PRD, Architecture Spine, and Epics describe the XS–XL 12-column model, MASTER BUS zone, and
  the 4 enable bools — with no remaining statement contradicting the built code.
- `.synthy` interop guarantee remains explicitly stated and true.
- deferred-work.md shows the prototype block formalized and the C# ToDo still open.
- Stories 1.4/1.5 can proceed on a spec that matches reality.

**Sequencing.** Apply A–N (independent doc edits) → then resume Story 1.4 (mod rings +
display transform) and 1.5 (migrate generators) on the reconciled spec. The open
column-width / row-packing polish and the C# mirror are tracked but not blockers.
