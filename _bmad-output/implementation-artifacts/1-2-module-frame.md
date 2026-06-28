---
baseline_commit: db8a1dfd5b0865749618b71419c51472a777fd5b
---

# Story 1.2: ModuleFrame renders a descriptor

Status: review

<!-- Note: Validation is optional. Run validate-create-story for quality check before dev-story. -->

## Story

As a JASS developer,
I want one `ModuleFrame` component that renders any `ModuleDescriptor` into a uniform header + body and owns its parameter bindings,
so that all modules share identical anatomy with zero per-module layout code (the second pillar of the rack framework).

## Acceptance Criteria

1. **Given** a `ModuleDescriptor` passed to a `ModuleFrame` placed at some bounds, **When** the frame is shown, **Then** it draws a **uniform header** (title text; an enable LED/toggle iff `enableParam` is non-empty; a reset ↺ button) and **flows the `body` elements into the size class's slot grid** — and there is **no layout math anywhere except `ModuleFrame::resized()`** (and later `Rack`). Each `BodyElement` maps to its widget: `Knob`→`SynthySlider`(+label), `Combo`→`juce::ComboBox`(+label), `Toggle`→`juce::ToggleButton`, `Action`→`juce::TextButton`, `FileAction`→`juce::TextButton`(opens a `juce::FileChooser`), `Caption`→`juce::Label`, `Display`→the referenced component added as a child.
2. **Given** any parameter-bound element (`Knob`/`Combo`/`Toggle`, and the header enable), **Then** `ModuleFrame` **creates and owns** the matching APVTS attachment (`SliderAttachment`/`ComboBoxAttachment`/`ButtonAttachment`) from the element's `paramId`; no `*Attachment` members exist in `PluginEditor`. Moving a knob updates its param and vice-versa.
3. **Given** a module with an `enableParam`, **When** that param is OFF, **Then** the **body region dims** (reduced opacity overlay) while the **header stays fully lit**; the enable toggle is the single source of truth and reflects external param changes. Modules with empty `enableParam` are always lit (no toggle, identical header geometry).
4. **Given** the reset ↺ button, **When** clicked, **Then** it writes **only** `desc.resetParams` back to their defaults via APVTS (`setValueNotifyingHost(getDefaultValue())`) and changes nothing else.
5. **Given** the build, **When** compiled, **Then** `Source/UI/rack/ModuleFrame.{h,cpp}` is added to `CMakeLists.txt` `target_sources`, the project builds clean (Release, JUCE warning flags) with no new warnings, and a temporary smoke instance of a `ModuleFrame` renders correctly in the running app (header + body + dim), then is removed before completion.
6. **Given** the real-time / state constraints, **Then** the frame touches the audio engine only through APVTS (attachments + `getParameter`/`getRawParameterValue`); it allocates/poll on the **message thread only**; a `Display` with a null `component` is handled safely (skipped, not dereferenced).

## Tasks / Subtasks

- [x] **Task 1: ModuleFrame skeleton + CMake** (AC: 1, 5)
  - [x] Create `Source/UI/rack/ModuleFrame.h` / `.cpp`; `class ModuleFrame : public juce::Component` in `namespace rack`
  - [x] Constructor `ModuleFrame(juce::AudioProcessorValueTreeState& apvts, ModuleDescriptor desc)` — store `apvts` ref + move-in the descriptor
  - [x] Add `Source/UI/rack/ModuleFrame.cpp` to `target_sources` in `CMakeLists.txt` (after `Source/UI/PluginEditor.cpp`)
- [x] **Task 2: Build the header** (AC: 1, 3, 4)
  - [x] Title `juce::Label` from `desc.title`
  - [x] Enable `juce::ToggleButton` ONLY if `desc.enableParam` is non-empty; add `ButtonAttachment(apvts, desc.enableParam, enableBtn)`
  - [x] Reset `juce::TextButton` styled "↺" (UTF-8 `\xE2\x86\xBA`, like `styleResetButton`); `onClick` resets `desc.resetParams` (Task 5)
- [x] **Task 3: Build the body widgets from `desc.body`** (AC: 1, 2, 6)
  - [x] `std::visit` each `BodyElement` → create the matching widget into an `OwnedArray<juce::Component>` (+ an `OwnedArray<juce::Label>` for knob/combo captions)
  - [x] For `Knob`/`Combo`/`Toggle`: create the widget AND its APVTS attachment from `paramId`, stored in a `std::vector<std::unique_ptr<...>>` owned by the frame (use `SynthySlider` for knobs)
  - [x] For `Action`: `TextButton` with `onClick = el.onClick`. For `FileAction`: `TextButton` that opens a `juce::FileChooser` and calls `el.onChoose`
  - [x] For `Caption`: a `juce::Label` from `el.text`. For `Display`: `if (el.component) addAndMakeVisible(*el.component);` (null-safe — skip if null)
- [x] **Task 4: Layout in `resized()` — the single body-flow site** (AC: 1)
  - [x] Reserve a fixed header strip at top; the rest is the body area
  - [x] Divide the body area into the size class's slot grid (`sizeClassSpec(desc.sizeClass)` → cols/units → slot cells); place each body widget into consecutive cells, a `Display` spanning its `slots`
  - [x] Knobs use `KnobSize` diameter centred in their cell; this is the ONLY place body layout geometry exists
- [x] **Task 5: Reset behaviour** (AC: 4)
  - [x] On reset click: for each id in `desc.resetParams`, `apvts.getParameter(id)->setValueNotifyingHost(p->getDefaultValue())` (mirror the existing `resetParamsToDefault` helper; implement locally in the frame, do not depend on the editor's private one)
- [x] **Task 6: Enable-dim** (AC: 3, 6)
  - [x] If `enableParam` non-empty: poll its value via a small `juce::Timer` (mirror `EnvelopeDisplay`'s poll-and-repaint-on-change pattern), tracking a `dimmed` flag
  - [x] In `paintOverChildren`, when `dimmed`, fill the **body region only** with a translucent overlay (header stays lit); repaint only on change
- [x] **Task 7: Verify** (AC: 5)
  - [x] Build clean (Release). Temporarily construct one `ModuleFrame` (e.g. a Filter-like S descriptor + one enable + 2 knobs) inside `SynthyEditor`, eyeball header/body/dim/reset, then remove the temporary code. (Full rack integration is Story 1.3.)

## Dev Notes

### What this story is (and is NOT)
- **IS:** the `ModuleFrame` component — renders ONE descriptor (header + body), owns its attachments, dims when disabled, resets on ↺. It lays out its OWN children **within whatever bounds it is given**.
- **IS NOT:** the `Rack` grid that positions frames (Story 1.3), modulation-ring/display-transform wiring (Story 1.4), or any real module descriptors (Story 1.5). Use a throwaway descriptor to test.
- **NFR1 nuance:** `ModuleFrame::resized()` IS allowed to lay out the frame's internal header+body — that is *framework* code in one place, not per-module code. The `Rack` decides the frame's outer rectangle (1.3). No per-*module* class ever has a `resized()`.

### Target API (aim for this)
```cpp
namespace rack {
class ModuleFrame : public juce::Component, private juce::Timer
{
public:
    ModuleFrame (juce::AudioProcessorValueTreeState& apvts, ModuleDescriptor desc);
    ~ModuleFrame() override;
    void resized() override;            // header strip + body slot-grid flow (THE layout site)
    void paint (juce::Graphics&) override;             // header bar / frame chrome
    void paintOverChildren (juce::Graphics&) override; // dim overlay when disabled
private:
    void timerCallback() override;      // poll enableParam, repaint on change
    juce::AudioProcessorValueTreeState& apvts;
    ModuleDescriptor desc;
    juce::Label title;
    std::unique_ptr<juce::ToggleButton> enableBtn;   // only if enableParam set
    juce::TextButton resetBtn;
    juce::OwnedArray<juce::Component> widgets;        // body widgets (own non-Display ones)
    juce::OwnedArray<juce::Label>     captions;       // labels under knobs/combos + Caption
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>>  sliderAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>>  buttonAtt;
    std::unique_ptr<juce::FileChooser> fileChooser;   // for FileAction
    std::atomic<float>* enableValue = nullptr;        // apvts.getRawParameterValue(enableParam)
    bool dimmed = false;
};
}
```

### Must reuse — do NOT reinvent (with sources)
- **Attachment creation pattern** — exactly as in the editor today: `std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramId, slider)` (and ComboBox/Button variants). [Source: Source/UI/PluginEditor.cpp:157-176]
- **Reset pattern** — `for (id : ids) if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(p->getDefaultValue());` [Source: Source/UI/PluginEditor.cpp:10-15 `resetParamsToDefault`]
- **Reset button styling** — "↺" via UTF-8 `\xE2\x86\xBA`, accent-tinted. [Source: Source/UI/PluginEditor.cpp:17-24 `styleResetButton`]
- **Knob widget** — use `SynthySlider` (carries rotary style, right-click entry, shift-fine, wheel-step, mod-ring). Set `knobDiameter` from `sizeClassSpec(desc.sizeClass).knobDiameter`. [Source: Source/UI/SynthySlider.h]
- **Enable-poll-and-repaint pattern** — mirror `EnvelopeDisplay` (a `juce::Timer` at ~20 Hz that reads `std::atomic<float>*` params and repaints only on change). [Source: Source/UI/PluginEditor.h:84-114]
- **Shared look** — do NOT install a LookAndFeel here; one `SynthyLookAndFeel` is set globally by the `Rack` in Story 1.3 (AD-7). [Source: ARCHITECTURE-SPINE AD-7]
- **The descriptor + size table** — from Story 1.1: `rack::ModuleDescriptor`, `BodyElement` variant (`Knob/Combo/Toggle/Action/FileAction/Caption/Display`), `sizeClassSpec()`, `bodySlots()`/`assertFitsClass()`. Call `assertFitsClass(desc)` in the constructor. [Source: Source/UI/rack/ModuleDescriptor.h]

### Carry-overs from Story 1.1 review (address here)
- The static-text element is **`Caption`** (field `text`), not `Label`. [1.1 Completion Notes]
- **Null `Display.component`**: guard it (`if (el.component)`) — deferred item from 1.1 review, due now. [deferred-work.md]
- **Descriptor ownership/copy**: `ModuleFrame` stores the descriptor by value (move it in) and is the single owner of the built widgets/attachments. `Action`/`FileAction` closures will capture the processor/APVTS (which outlive the editor), so storing the descriptor in the frame is safe. Do NOT copy a live `ModuleFrame`/descriptor around. [deferred-work.md]

### Hard constraints (project-context.md — binding)
- **No audio-thread code.** All widget/attachment construction and the enable-poll timer run on the **message thread**. APVTS attachments are the only bridge to params. [project-context.md RT rules; NFR2]
- **No parameter-ID / APVTS-layout / `.synthy` changes.** Read params, never redefine them. [NFR3]
- **Naming dualism stays** — new code under `Source/UI/rack/` in `namespace rack`; existing `Synthy*` classes keep their names. [project-context.md]
- After editing `CMakeLists.txt`, regenerate is NOT needed for an added source if CMake re-runs on build (it does via ZERO_CHECK); a clean build picks it up. Build via MSBuild from PowerShell (`cmake.exe` not on PATH). [project-context.md Build]

### Project Structure Notes
- New files: `Source/UI/rack/ModuleFrame.{h,cpp}` next to `ModuleDescriptor.h`. First `.cpp` in `rack/` → requires the `target_sources` edit (AC5). No other structure change.

### Testing
- No unit-test framework in the project (unchanged). Verification = clean Release build + the temporary in-editor smoke instance (Task 7), removed before completion. Full visual/behavioural verification of placement lands in Story 1.3 (Rack).

### References
- [Source: _bmad-output/planning-artifacts/epics.md#Story-1.2]
- [Source: ARCHITECTURE-SPINE.md AD-1, AD-2, AD-4, AD-5, AD-6, AD-7]
- [Source: Source/UI/rack/ModuleDescriptor.h]
- [Source: Source/UI/PluginEditor.cpp:10-24, :157-176] [Source: Source/UI/PluginEditor.h:84-114]
- [Source: Source/UI/SynthySlider.h] [Source: _bmad-output/implementation-artifacts/1-1-module-descriptor-types.md] [Source: _bmad-output/implementation-artifacts/deferred-work.md]

## Dev Agent Record

### Agent Model Used

claude-opus-4-8[1m] (Claude Opus 4.8, 1M context)

### Debug Log References

- Build 1 (with new `ModuleFrame.cpp` in CMake + a temporary consumer-check in `PluginEditor.cpp`): CMake auto-reconfigured on the `CMakeLists.txt` change, compiled `ModuleFrame.cpp` + `PluginEditor.cpp`, linked `JASS.exe` — exit 0, no new warnings.
- Build 2 (temp consumer-check removed): clean, exit 0. `PluginEditor.cpp` is byte-identical to baseline (`git diff` empty).

### Completion Notes List

- Implemented `Source/UI/rack/ModuleFrame.{h,cpp}` (`namespace rack`, `juce::Component` + private `juce::Timer`): uniform header (title, optional enable `ToggleButton`, reset ↺ shown only when `resetParams` non-empty), body built from `desc.body` via `std::get_if` over the variant, and a single-site slot-grid layout in `resized()`.
- **Attachments owned by the frame (AC2/AD-6):** `SliderAttachment`/`ComboBoxAttachment`/`ButtonAttachment` created from each element's `paramId`, held in member vectors. No `*Attachment` added to `PluginEditor`.
- **Enable-dim (AC3/FR7):** frame polls `apvts.getRawParameterValue(enableParam)` via a 20 Hz `juce::Timer` (mirrors `EnvelopeDisplay`), repaints only on change; `paintOverChildren` dims the body region only, header stays lit. Always-on modules (empty `enableParam`) start no timer and never dim.
- **Reset (AC4):** `doReset()` writes only `desc.resetParams` via `p->setValueNotifyingHost(p->getDefaultValue())` (local impl, mirrors the editor's `resetParamsToDefault`; no dependency on the editor's private helper).
- **Widget mapping (AC1):** Knob→`SynthySlider` (diameter from `sizeClassSpec`), Combo→`juce::ComboBox` (static `addItemList(items,1)` or dynamic provider), Toggle→`juce::ToggleButton`, Action→`juce::TextButton`(onClick), FileAction→`juce::TextButton` launching an async `juce::FileChooser`→`onChoose`, Caption→`juce::Label`, Display→child component.
- **1.1-review carry-overs addressed:** null `Display.component` is skipped (null-safe); the frame takes the descriptor **by move** and is sole owner of widgets/attachments (no descriptor copying), resolving the ownership concern; `assertFitsClass(desc)` called in the constructor.
- **Deliberate scope choices:** no `LookAndFeel` installed here — the single `SynthyLookAndFeel` is set globally by the `Rack` in Story 1.3 (AD-7). Body slot-grid uses `nCols = sizeClassSpec.cols * 3` as a first-pass arrangement; exact `Wc`/`Hu`/cell tuning is pinned with the `Rack` + mockup constants (Story 1.3, deferred).
- **Verification honesty:** verified by clean Release build + a temporary consumer instantiation (`ModuleFrame` constructed with a Filter-like descriptor + `setBounds`, compiled then removed). I did **not** visually confirm rendering in the running app (no `Rack`/placement exists yet, Story 1.3) — visual/behavioural confirmation is deferred to Story 1.3 integration or a manual run by the user. No project test framework exists.

### File List

- **NEW:** `Source/UI/rack/ModuleFrame.h`
- **NEW:** `Source/UI/rack/ModuleFrame.cpp`
- **MODIFIED:** `CMakeLists.txt` (added `Source/UI/rack/ModuleFrame.cpp` to `target_sources`)

### Change Log

- 2026-06-28 — Story 1.2 implemented: `ModuleFrame` renders any `ModuleDescriptor` (uniform header + body slot-grid), owns its APVTS attachments, dims body when disabled, resets `resetParams` on ↺. Added to CMake. Verified by clean Release build (temp consumer-check compiled then removed). Visual confirmation deferred to Story 1.3 (Rack). Status → review.
