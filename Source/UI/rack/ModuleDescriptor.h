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
    // A name ending in U{n} states the GRID HEIGHT in quarter units instead of content rows — the
    // two are separate since Story 7.4 (W28U6 = 28 columns, 2 content rows, 6 quarters = 176 px).
    enum class SizeClass  { W2H1, W3H1, W4H1, W4H2, W5H1, W6H1, W7H1, W8H1, W8H2, W9H2, W9H3, W12H2, W16H2, W24H1, W24H2, W10H1, W12H1, W13H1, W14H1, W30H1, W30H2, W28H2, W20H2, W28U6, W28U7, W20U7, W4U7, W30U7 };
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

        // Optional per-knob ON/OFF switch, drawn as a small checkbox in the TOP-RIGHT of the
        // knob's own cell (STEP SEQ: a step that is off is a rest). Off dims the knob through
        // the same activeWhen path the mode-dependent knobs use, so it reads exactly like every
        // other greyed control in the rack. Unlike activeWhen this needs no editor injection:
        // ModuleFrame owns the parameter and builds the predicate itself.
        juce::String toggleParamId;

        // Optional THIRD state for that corner switch (15.2): with this set, a click cycles
        // off → on → ACCENTED — the TR-909's "second press deepens it" gesture — and the accent
        // lands in this bool parameter. Rendered by the same switch (empty / tick / filled+tick),
        // so an accent row costs no rack space at all. Ignored when toggleParamId is empty.
        juce::String accentParamId;

        // Optional read-out override for the value box: the knob shows what this returns instead of
        // the number. PERC's NOTE knob uses it to say "Kick" rather than "36" (Story 16.1) — the
        // stored value stays the note number, only its presentation changes. Injected by the editor
        // (the name depends on the loaded kit, which a static spec cannot reach).
        std::function<juce::String (double value)> textFromValue;

        // Optional preview hook (Story 15.3): a knob whose value IS a pitch can sound it while it
        // is being edited — STEP SEQ, where a step is a number of semitones and writing a figure
        // otherwise means guessing. Called with (value, true) on every user-driven change and
        // (0, false) when the gesture ends. Injected by the EDITOR, which is the only place that
        // knows the keyboard state and the current octave.
        std::function<void (int value, bool sounding)> audition;

        // Optional HIGHLIGHT predicate: when it returns true this one knob is ringed in the module's
        // colour. Polled in the frame timer beside activeWhen, and injected by the EDITOR — it marks
        // UI state, not a parameter (STEP SEQ: the step the keyboard is about to write, Story 15.4).
        std::function<bool()> highlightWhen;

        // Optional PLAYHEAD predicate: true while this knob is the one a sequencer is sounding right
        // now. Drawn as a lit dot rather than a ring, so it cannot be confused with the write cursor
        // above — the two are visible at the same time and mean opposite things (what plays next vs.
        // what you are about to overwrite). Same polling path, also editor-injected.
        std::function<bool()> playingWhen;

        // Layout width in body slots (a plain control = 1), exactly like Combo::slots. A knob grows
        // to fill its cell (see ModuleFrame::resized), and a 1-slot cell in a densely packed module
        // is narrower than it is tall — the rotary is then capped by the WIDTH and the row's height
        // goes to waste. MOD MATRIX's AMT claims 2 so its cell is wide enough for the knob to reach
        // the height its row actually offers (measured: 46 px → 65 px, Story 7.5).
        int slots = 1;

        // Optional hover text override. Default (unset): a knob with textFromValue shows that same
        // text as its tooltip — the cure for a value box narrower than its name (PERC NOTE). Set it
        // when the hover should say MORE than the box: STEP SEQ boxes read "E1", the tooltip adds
        // the MIDI number ("E1 · 40") for transcribing from templates. Editor-injected, like
        // textFromValue. Appended last so existing aggregate initializers stay valid.
        std::function<juce::String (double value)> tooltipFromValue;

        // Optional ALTERNATE parameter sharing this knob's CELL (15.7): with the module's row
        // toggle (ModuleDescriptor::altRowTitle) flipped, the cell edits this param instead —
        // the BeatStep's "the knob row cycles its meaning". STEP SEQ: pitch ⇄ per-step gate.
        // ModuleFrame builds a second (hidden) slider on the same bounds; corner switch, write
        // ring and playhead stay anchored to the cell and work in both views. Editor-injected.
        juce::String altParamId;
        std::function<juce::String (double value)>       altTextFromValue;   // alt row's read-out
        std::function<double (const juce::String& text)> altValueFromText;   // ...and its inverse
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
        // Optional VALUES for the items, in the same order. Without it the item's POSITION is the
        // value, which is only safe while the list is the complete, unfiltered thing the parameter
        // indexes. PERC's KIT lists drum kits only (a single WAV would put the same recording on
        // all four lanes), so its positions are not store indices — and a filtered list with
        // position-as-value is precisely how this project has twice loaded the wrong sample.
        // Provide this and the parameter keeps meaning what it means, whatever the list shows.
        std::function<juce::Array<int>()> itemValues;
        // Layout width in body slots (a knob = 1). Default 2; raise it for combos whose items
        // are user-named and can be long (SAMPLER SET: "SalamanderPiano" — user 2026-08-04).
        int slots = 2;
        // Fired with the selected index on a USER selection only — programmatic updates
        // (preset load, refreshCombo, timer resync) use dontSendNotification and stay silent.
        // Lets a descriptor react to the gesture (SAMPLER SET → auto One-Shot for mapped sets)
        // without fighting preset restores. Supported for indexIsValue combos.
        std::function<void(int)> onUserSelect;
        // Optional PER-COMBO relevance predicate — the combo counterpart of Knob::activeWhen
        // (same contract, same condKnobs polling path): when set and false, this one combo is
        // disabled and dimmed while the module stays live. MOD MATRIX QUANT uses it — the mask
        // only acts on FREQ routings, so on any other target the combo reads as "not in play".
        std::function<bool()> activeWhen;
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

        // A module that only DRAWS — the OSCILLOSCOPE and the SPECTRUM. Their enable param freezes
        // and blanks a picture; nothing about them can be heard. That makes hiding them a decision
        // the rack may simply BELIEVE, which for every other module it must not:
        //   · Rack::maxHeight counts a hidden module anyway when it is factory-visible, because a
        //     preset enabling it would reveal it again and the window would resize (PR #27). A
        //     visual-only module is exempt: hiding it gives its height back for real.
        //   · revealEnabledModules therefore must NOT bring it back — otherwise the height it just
        //     freed returns on the next preset and the measurement oscillates.
        // The narrow scope is deliberate. Applying "hidden stays hidden" to a module that makes
        // sound would let a preset load without a part of its patch, which is a far worse trade
        // than a scope one has to switch on again by hand.
        bool visualOnly = false;

        // Dependent-combo links (see ComboDependency). Polled in the frame's timer (message thread),
        // so a MODULE change re-lists its slot's PARAM combo without touching the audio thread.
        std::vector<ComboDependency> comboDeps;

        // Row toggle title (15.7): non-empty => the header shows a latch button with this text,
        // and every Knob carrying an altParamId flips between its two params with it (STEP SEQ:
        // "GATE" flips the 32 step knobs between pitch and per-step gate). View state, not a
        // parameter — loading a preset must not flip what the user is looking at.
        juce::String altRowTitle;

        // Header ACTION buttons (15.8): plain one-shot text buttons in the title bar, left of
        // the row toggle. For module-scoped file actions — STEP SEQ's LOAD MIDI / SAVE MIDI:
        // the preset dialogs cannot separate ".jass or .mid?" up front (JUCE's native chooser
        // shows ONE combined filter entry), so the module carries its own entry points and the
        // intent is settled before any dialog opens (maintainer 2026-08-30, the 15.8 fallback).
        // With `onToggle` set instead of `onClick`, the button LATCHES and reports its state —
        // PERC's COPY mode (stamp a step column onto others) rides the same slot.
        struct HeaderAction
        {
            juce::String title, tooltip;
            std::function<void()>     onClick;    // one-shot …
            std::function<void(bool)> onToggle;   // … OR a latch; exactly one of the two is set
        };
        std::vector<HeaderAction> headerActions;

        // Collapsible display (story 16.2, first slice — maintainer 2026-08-31): with a title
        // set, the header carries a latch that folds the module's Display cells away and
        // shrinks it to `collapsedSize`; expanding restores `sizeClass` and the rack re-packs
        // LIVE, shifting the rows below. ADSR: the envelope curve behind a CURVE button. View
        // state — never persisted into presets (a preset load must not resize the rack).
        juce::String collapseTitle;                       // empty = feature off
        SizeClass    collapsedSize = SizeClass::W4H1;
        bool         startCollapsed = false;

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
        int units = 0;          // CONTENT rows: how many rows the body lays its cells over
        int heightUnits = 0;    // GRID height in quarter units (Rack::kHu). Separate from `units`
                                // because one number used to mean both — the trap every attempt at
                                // a shorter module fell into. 4 quarters == the old rack unit, so
                                // heightUnits = 4*units reproduces every previous height to the
                                // pixel (4·21+3·10 = 114, 8·21+7·10 = 238) and heights BETWEEN them
                                // become expressible (5 => 145, 6 => 176).
        int slotCapacity = 0;
        int knobDiameter = 0;
    };

    inline SizeClassSpec sizeClassSpec (SizeClass c) noexcept
    {
        switch (c)
        {
            // Column spans on the rack grid (cols × units) — 30 columns since Story 7.3, so a
            // width is 30ths and 8 columns are just over a quarter of the rack. Decoupled from
            // knob size: the body fills the module width from its CONTENT (see
            // ModuleFrame::resized). slotCapacity is only a generous debug guard, not a layout
            // driver. Names encode the footprint (W{cols}H{rows}) in ABSOLUTE columns, which is
            // why widening the grid from 24 to 30 left every module below untouched — they keep
            // their physical size and simply pack more per row. Full-width modules are the
            // exception: those had to move W24 → W30.
            // heightUnits is in QUARTER units (Rack::kHu = 21 px, gutter 10): 4 => 114 px, 8 => 238,
            // and the steps between are now sayable — 5 => 145, 6 => 176. Every class below keeps
            // 4*units, so this table describes exactly the rack that shipped.
            case SizeClass::W2H1:  return {  2, 1,  4,  2, KnobSize::Small };  // single control (was XXS)
            case SizeClass::W3H1:  return {  3, 1,  4,  3, KnobSize::Small };  // single control, wider header (fits title + 3 icons)
            case SizeClass::W4H1:  return {  4, 1,  4,  4, KnobSize::Small };  // 1–2 controls   (was XS)
            case SizeClass::W4H2:  return {  4, 2,  8, 12, KnobSize::Small };  // narrow 2-row (ADSR: 4 knobs + curve)
            case SizeClass::W5H1:  return {  5, 1,  4,  6, KnobSize::Small };  // ~4 controls incl. 1–2 combos (LFO)
            case SizeClass::W6H1:  return {  6, 1,  4,  6, KnobSize::Small };  // 3–4 controls   (was S)
            case SizeClass::W7H1:  return {  7, 1,  4,  7, KnobSize::Small };  // ~4 controls, one unit narrower than W8 (LFO)
            case SizeClass::W8H1:  return {  8, 1,  4,  8, KnobSize::Small };  // 5–6 controls   (was M; 3 per row)
            case SizeClass::W8H2:  return {  8, 2,  8, 16, KnobSize::Small };  // knobs + curve display (was L)
            case SizeClass::W9H2:  return {  9, 2,  8, 24, KnobSize::Small };  // MOD MATRIX (2 routing rows, ~LFO width)
            case SizeClass::W9H3:  return {  9, 3, 12, 36, KnobSize::Small };  // MOD MATRIX (3 routing rows = 6 slots)
            case SizeClass::W12H2: return { 12, 2,  8, 24, KnobSize::Small };  // wide visualisers (2 per row)
            case SizeClass::W16H2: return { 16, 2,  8, 32, KnobSize::Small };  // MOD MATRIX wide: 2 rows × 3 slots
            case SizeClass::W24H1: return { 24, 1,  4, 24, KnobSize::Small };  // full-width single row  (on-screen keyboard, flat)
            case SizeClass::W24H2: return { 24, 2,  8, 48, KnobSize::Small };  // full-width two rows    (on-screen keyboard, tall keys)
            case SizeClass::W10H1: return { 10, 1,  4, 10, KnobSize::Small };  // ~7-8 controls single row (WAVETABLE + PAN)
            case SizeClass::W12H1: return { 12, 1,  4, 12, KnobSize::Small };  // widest single row (WAVETABLE: BANK+LOAD+6 knobs)
            case SizeClass::W13H1: return { 13, 1,  4, 13, KnobSize::Small };  // WAVETABLE (BANK+LOAD+6 knobs, roomier)
            case SizeClass::W14H1: return { 14, 1,  4, 14, KnobSize::Small };  // WAVETABLE with PAN (BANK+LOAD+6 knobs)
            case SizeClass::W30H1: return { 30, 1,  4, 30, KnobSize::Small };  // full-width single row (on-screen keyboard)
            case SizeClass::W30H2: return { 30, 2,  8, 60, KnobSize::Small };  // full-width two rows   (STEP SEQ)
            // MOD MATRIX: 28 is not a round number, it is the measured one. Its 8 slots make 64
            // content slots over 2 rows = 32 cells (the AMT knob claims 2 of them since Story 7.5,
            // so it is wide enough to grow into its row's height). At 30 columns 28 keeps the cell
            // at 54 px; W24 gave 45 px cells and visibly small AMT knobs.
            case SizeClass::W28H2: return { 28, 2,  8, 64, KnobSize::Small };
            // TWO CONTENT ROWS, sized from the standard knob (Story 7.4): a knob row is
            // 13 caption + 40 knob + 8 + 14 value box + 4 cell padding = 79 px, so a two-row body
            // plus the 22 px header and 8 px padding wants 188 — which lands on SEVEN quarter units
            // (207 px), the first step that holds it. That is where MOD MATRIX, STEP SEQ, PERC and
            // ADSR now sit, instead of the 238 px they inherited from a raster that could only count
            // in whole 114 px rows. W28U6 (176 px) is the same module WITHOUT its repeated captions;
            // kept because the maintainer may still want that trade, not used today.
            case SizeClass::W28U6: return { 28, 2,  6, 64, KnobSize::Small };
            case SizeClass::W28U7: return { 28, 2,  7, 64, KnobSize::Small };   // MOD MATRIX before QUANT (kept: the maintainer may want the trade back)
            // MOD MATRIX since QUANT: a fifth control per slot makes 24 cells per row — full rack
            // width is what keeps the cell above ~55 px. The AMT knob is unaffected (2 cells wide,
            // capped by the row height anyway); only the combos give up a few px each.
            case SizeClass::W30U7: return { 30, 2,  7, 72, KnobSize::Small };
            case SizeClass::W20U7: return { 20, 2,  7, 40, KnobSize::Small };   // STEP SEQ, PERC
            case SizeClass::W4U7:  return {  4, 2,  7, 12, KnobSize::Small };   // ADSR (knobs + curve)
            // STEP SEQ: 32 steps as 2 x 16 plus five globals = 19 cells per row; 20 columns is
            // the narrowest width at which such a cell still reaches the 62 px a knob wants.
            case SizeClass::W20H2: return { 20, 2,  8, 40, KnobSize::Small };
        }
        jassertfalse;
        return { 1, 1, 4, 3, KnobSize::Small };
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
