#pragma once
#include <JuceHeader.h>
#include <variant>
#include <vector>
#include <array>
#include <functional>
#include "../SynthySlider.h"   // KnobSize
#include "../../DSP/ModTargets.h"   // LFOTarget = the shared modulation-target vocabulary (dependency-free)

// The unified-rack UI is data-driven (see ARCHITECTURE-SPINE AD-1/AD-4): every
// module is a ModuleDescriptor, not a bespoke component. This header defines ONLY
// the data model + the size-class table + slot accounting. Rendering lives in
// ModuleFrame (Story 1.2); placement in Rack (Story 1.3). No audio-thread code,
// no parameter definitions, no APVTS ownership here — pure UI-layer data.
namespace rack
{
    // Size classes are named by their grid FOOTPRINT: W{cols}H{rows} on the 24-column grid
    // (the smallest unit is 1 of 24 columns wide × 1 rack row tall). Column-based names make
    // the grid maths explicit at every call site and show exactly which widths exist. The 24-col
    // grid (was 12) gives the finer granularity needed to size small modules tightly without
    // dropping rotaries below their minimum. Current set mirrors the old XXS..XL 1:1 (doubled):
    //   W2H1=old XXS, W4H1=XS, W6H1=S, W8H1=M, W8H2=L, W12H2=XL. Add intermediate widths
    //   (e.g. W3H1) here as ONE new case for the size-tuning pass.
    enum class SizeClass  { W2H1, W3H1, W4H1, W4H2, W5H1, W6H1, W7H1, W8H1, W8H2, W9H2, W9H3, W12H2, W16H2, W24H1, W24H2, W10H1, W12H1, W13H1, W14H1 };
    enum class ModuleType { Generator, Modulator, Processor };       // identity / colour tag
    // For live LFO rings (AD-8): the ring target IS the modulation target. Single source of truth
    // is ModTargets.h; ModTarget::Off means "no ring on this knob" (== LFOTarget::Off).
    using ModTarget = LFOTarget;

    // Live modulation feed for the rings (AD-8, generalized in Story 8.1). `byTarget` is indexed by
    // ModTarget (None = 0, unused): the ring amount applied to each target by PERIODIC (LFO) sources
    // — the display LFO value times the summed LFO-sourced routing coefficient into that target. A
    // knob lights with feed.byTarget[(int) knob.modTarget]. `osc` adds the PER-OSCILLATOR amounts
    // (Epic 8.3): osc[oscIndex 0..2][slot 0=FREQ,1=AMP,2=DETUNE], so a routing to a single OSC lights
    // only that oscillator's knob (the global "Alle OSC" contribution still comes from byTarget).
    struct LiveModFeed
    {
        std::array<float, (size_t) ModTargets::kCount> byTarget {};
        float osc[3][6] {};   // [oscIndex 0..2][slot 0=FREQ,1=AMP,2=DETUNE,3=FB,4=VOICES,5=PAN] (ModDest::oscParamSlot)
    };

    // Rack zones (AD-10). Defined HERE (not inside Rack) so a ModuleDescriptor can carry
    // its own default zone — Rack.h includes this header, so the descriptor can't reference
    // Rack::Zone without an include-cycle. Rack aliases this as Rack::Zone for its callers.
    // (Persistence stores the zone by NAME, so appending an enumerator is safe.)
    // Input hosts non-DSP input surfaces (the on-screen keyboard) so they can be hidden
    // like any module — e.g. when playing via an external MIDI keyboard.
    enum class Zone { Generators, Modulation, Processing, Visualization, MasterBus, Input };

    // Desaturated identity tints (FR6), matching the rack mockup. The single source
    // of the type→colour map, shared by ModuleFrame (top edge / reset tint) and the
    // Rack (zone-header hue) so the palette is defined exactly once.
    inline juce::Colour typeColour (ModuleType t) noexcept
    {
        switch (t)
        {
            case ModuleType::Generator: return juce::Colour (0xff5e9b96);
            case ModuleType::Modulator: return juce::Colour (0xff9384b6);
            case ModuleType::Processor: return juce::Colour (0xff6f86ad);
        }
        return juce::Colour (0xff6f86ad);
    }

    // --- Body-element vocabulary (AD-4) -----------------------------------

    struct Knob
    {
        juce::String paramId;
        juce::String label;
        // Optional display transform as a GUARDED PAIR (AD-4): the knob shows a
        // derived value while editing the underlying param (e.g. FREQ shows the
        // played frequency = base * ratio, writing base back on edit). The pair is
        // ALL-OR-NOTHING: both empty (the default) => identity, and a partially-set
        // pair (one empty) MUST also be treated as identity by consumers — never
        // call a half-set transform. The ratio<=0 guard (no note sounding =>
        // identity, no write-back, never divide a stale/zero ratio) is enforced
        // where the knob is wired in Story 1.4 — here we only carry the functions.
        std::function<double(double base,  double ratio)> toDisplay;
        std::function<double(double shown, double ratio)> fromDisplay;
        ModTarget modTarget = ModTarget::Off;

        // Optional PER-KNOB relevance predicate. When set and false, this ONE knob is disabled
        // and dimmed while the rest of the module stays live — for a knob that only applies in
        // some modes (STEREO's WIDTH/TIME are Pseudo-Stereo-only). Like enabledWhen it must read
        // APVTS params only (AD-9) and is polled in the frame timer (message thread), so the
        // EDITOR injects it after makeModuleDescriptor (a static spec can't capture apvts).
        // Purely cosmetic/interaction: the DSP already ignores an irrelevant param.
        std::function<bool()> activeWhen;
    };

    struct Combo
    {
        juce::String paramId;
        juce::String label;
        // Static items, OR a provider re-polled at refresh time (e.g. the
        // Wavetable bank list — refreshed declaratively via Action/FileAction
        // .refreshes in Story 1.5).
        std::variant<juce::StringArray, std::function<juce::StringArray()>> items;
        // When true, the selected ITEM INDEX is written straight to the parameter (index==value),
        // bypassing ComboBoxParameterAttachment — whose value = index/(numItems-1) mismaps when the
        // item count varies against a fixed param range (MOD MATRIX PARAM: 1..N items, range 0..N-1).
        bool indexIsValue = false;
        // Layout width in body slots (a knob = 1). Default 2; raise it for combos whose items
        // are user-named and can be long (SAMPLER SET: "SalamanderPiano" — user 2026-08-04).
        int slots = 2;
        // Fired with the selected index on a USER selection only — programmatic updates
        // (preset load, refreshCombo, timer resync) use dontSendNotification and stay silent.
        // Lets a descriptor react to the gesture (SAMPLER SET → auto One-Shot for mapped sets)
        // without fighting preset restores. Supported for indexIsValue combos.
        std::function<void(int)> onUserSelect;
    };

    struct Toggle
    {
        juce::String paramId;
        juce::String label;
    };

    // Links one combo's items to another param's value: when `watchParamId` changes, the frame
    // calls onWatchChanged(newValue) (e.g. to clamp) and then re-polls `refreshParamId`'s provider.
    // Used by the MOD MATRIX so each slot's PARAM combo lists the params of the picked MODULE.
    struct ComboDependency
    {
        juce::String watchParamId;
        juce::String refreshParamId;
        std::function<void(int)> onWatchChanged;   // optional; receives the new watched value
    };

    struct Action   // a non-parameter trigger button (e.g. Karplus PLUCK)
    {
        juce::String label;
        std::function<void()> onClick;
        std::vector<juce::String> refreshes;   // combo paramIds to re-poll after firing
    };

    struct FileAction   // opens a file chooser and applies the result (e.g. LOAD WAV)
    {
        juce::String label;
        std::function<void(juce::File)> onChoose;
        std::vector<juce::String> refreshes;
        juce::File   startFolder;                 // where the chooser opens (empty => default)
        juce::String wildcard = "*";              // file filter (e.g. "*.wav")
        bool pickDirectory = false;               // true => choose a FOLDER instead of a file
                                                  // (SAMPLER multisample import, Story 12.2)
    };

    struct Caption   // static text (AD-4 "Label"); named Caption to avoid clashing with juce::Label
    {
        juce::String text;
    };

    struct Display   // wraps an existing display component (Scope/Spectrum/ADSR curve)
    {
        juce::Component* component = nullptr;   // NON-owning: the editor owns the lifetime (AD-5)
        int slots = 0;                          // grid slots this display spans
    };

    using BodyElement = std::variant<Knob, Combo, Toggle, Action, FileAction, Caption, Display>;

    // --- The module descriptor (AD-1) -------------------------------------

    struct ModuleDescriptor
    {
        SizeClass    sizeClass {};
        juce::String id;                          // stable slug (e.g. "osc1") — the layout key for
                                                  // the RackLayout model: show/hide + drag-drop (AD-10)
        juce::String helpId;                      // online-help resource slug; empty => use id. Lets
                                                  // instanced modules share ONE help text (LFO 1..4 =>
                                                  // "lfo", OSC 1..3 => "osc1") instead of duplicating it.
        juce::String title;
        ModuleType   type {};
        Zone         defaultZone {};              // default rack zone (AD-10); the RackLayout model
                                                  // seeds from this. typeTag/`type` is identity/colour
                                                  // only and is INDEPENDENT of the zone.
        bool         defaultVisible = true;       // factory visibility (Story 4.3): a factory reset
                                                  // restores this. false => module starts hidden (+silent)
                                                  // so the default rack doesn't overflow the screen.
        juce::String enableParam;                 // empty => always-on (Master, ADSR, Mix-Mode)
        std::vector<juce::String> resetParams;    // the reset (↺) writes these defaults to APVTS
        std::vector<BodyElement>  body;

        // Optional DERIVED active/dimmed state. When set, the module's enabled (lit) vs
        // dimmed state is computed from this predicate instead of a single enableParam —
        // e.g. Mix-Mode is active only when OSC1 AND OSC2 are enabled. The predicate must
        // read shared APVTS params only (AD-9), never reference another module object. It
        // does NOT add a header toggle (that is still enableParam's job); it only drives the
        // dim overlay + moduleEnabled(). If both enableParam and enabledWhen are set,
        // enabledWhen wins.
        std::function<bool()> enabledWhen;

        // Optional EXTRA reset action for state the ↺ can't reach via APVTS params — e.g. a
        // display's internal control (the Oscilloscope's time-base). When set, the module shows
        // a reset ↺ even if it has no resettable params, and doReset() calls this after writing
        // any param defaults. Used by the VISUALIZATION modules (Story 6.1 follow-up).
        std::function<void()> onReset;

        // Horizontal alignment WITHIN the zone row. false (default) => the module packs flush
        // left; true => it is shifted (as a block with the other right-aligned modules) to hug
        // the right edge. Used by the MASTER BUS: PRESETS stays left, STEREO/MASTER/COMPRESSOR
        // hug the right (balancing the zone title). Zones with no right-aligned module pack fully
        // left exactly as before.
        bool alignRight = false;

        // Dependent-combo links (see ComboDependency). Polled in the frame's timer (message thread),
        // so a MODULE change re-lists its slot's PARAM combo without touching the audio thread.
        std::vector<ComboDependency> comboDeps;

        // Per-slot activity highlight (MOD MATRIX). The body is a repeating run of `groupSize`
        // controls (a routing slot = SRC·MOD·PARAM·AMT = 4). isActive(slotIndex) reports whether
        // that slot is "wired" (its MOD != Off), so the frame can DIM inactive slots and mark active
        // ones with a lit dot. Polled in the frame timer (message thread). groupSize 0 => feature off.
        struct SlotActivity
        {
            int groupSize = 0;
            std::function<bool(int slotIndex)> isActive;
        };
        SlotActivity slotActivity;
    };

    // --- Size-class table (AD-2) ------------------------------------------
    // The single source of sizing. A 4th class (e.g. a wide-display W) is added
    // here as ONE new case — never by per-module custom dimensions.

    struct SizeClassSpec
    {
        int cols = 0;
        int units = 0;
        int slotCapacity = 0;
        int knobDiameter = 0;
    };

    inline SizeClassSpec sizeClassSpec (SizeClass c) noexcept
    {
        switch (c)
        {
            // Column spans on the 24-column grid (cols × units). Decoupled from knob size —
            // the body fills the module width from its CONTENT (see ModuleFrame::resized).
            // slotCapacity is only a generous debug guard, not a layout driver. Names encode the
            // footprint (W{cols}H{rows}); widths are 24ths, so 8/24 = one third of the rack.
            case SizeClass::W2H1:  return {  2, 1,  2, KnobSize::Small };  // single control (was XXS)
            case SizeClass::W3H1:  return {  3, 1,  3, KnobSize::Small };  // single control, wider header (fits title + 3 icons)
            case SizeClass::W4H1:  return {  4, 1,  4, KnobSize::Small };  // 1–2 controls   (was XS)
            case SizeClass::W4H2:  return {  4, 2, 12, KnobSize::Small };  // narrow 2-row (ADSR: 4 knobs + curve)
            case SizeClass::W5H1:  return {  5, 1,  6, KnobSize::Small };  // ~4 controls incl. 1–2 combos (LFO)
            case SizeClass::W6H1:  return {  6, 1,  6, KnobSize::Small };  // 3–4 controls   (was S)
            case SizeClass::W7H1:  return {  7, 1,  7, KnobSize::Small };  // ~4 controls, one unit narrower than W8 (LFO)
            case SizeClass::W8H1:  return {  8, 1,  8, KnobSize::Small };  // 5–6 controls   (was M; 3 per row)
            case SizeClass::W8H2:  return {  8, 2, 16, KnobSize::Small };  // knobs + curve display (was L)
            case SizeClass::W9H2:  return {  9, 2, 24, KnobSize::Small };  // MOD MATRIX (2 routing rows, ~LFO width)
            case SizeClass::W9H3:  return {  9, 3, 36, KnobSize::Small };  // MOD MATRIX (3 routing rows = 6 slots)
            case SizeClass::W12H2: return { 12, 2, 24, KnobSize::Small };  // wide visualisers (2 per row)
            case SizeClass::W16H2: return { 16, 2, 32, KnobSize::Small };  // MOD MATRIX wide: 2 rows × 3 slots
            case SizeClass::W24H1: return { 24, 1, 24, KnobSize::Small };  // full-width single row  (on-screen keyboard, flat)
            case SizeClass::W24H2: return { 24, 2, 48, KnobSize::Small };  // full-width two rows    (on-screen keyboard, tall keys)
            case SizeClass::W10H1: return { 10, 1, 10, KnobSize::Small };  // ~7-8 controls single row (WAVETABLE + PAN)
            case SizeClass::W12H1: return { 12, 1, 12, KnobSize::Small };  // widest single row (WAVETABLE: BANK+LOAD+6 knobs)
            case SizeClass::W13H1: return { 13, 1, 13, KnobSize::Small };  // WAVETABLE (BANK+LOAD+6 knobs, roomier)
            case SizeClass::W14H1: return { 14, 1, 14, KnobSize::Small };  // WAVETABLE with PAN (BANK+LOAD+6 knobs)
        }
        jassertfalse;
        return { 1, 1, 3, KnobSize::Small };
    }

    // --- Slot accounting + capacity assertion (AD-2) ----------------------

    inline int elementSlots (const BodyElement& e) noexcept
    {
        // Every control kind takes one slot; a Display spans its declared slots.
        if (auto* d = std::get_if<Display> (&e))
        {
            jassert (d->slots > 0);   // a Display must claim >=1 slot (default 0 is a setup bug)
            return d->slots;
        }
        return 1;
    }

    inline int bodySlots (const std::vector<BodyElement>& body) noexcept
    {
        int total = 0;
        for (const auto& e : body)
            total += elementSlots (e);
        return total;
    }

    // Debug guardrail: a descriptor must fit its size class. An overflowing body
    // (e.g. Wavetable declared M) trips here so it is caught during development.
    inline void assertFitsClass (const ModuleDescriptor& d) noexcept
    {
        jassert (bodySlots (d.body) <= sizeClassSpec (d.sizeClass).slotCapacity);
        juce::ignoreUnused (d);
    }
}
