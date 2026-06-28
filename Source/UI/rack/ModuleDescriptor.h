#pragma once
#include <JuceHeader.h>
#include <variant>
#include <vector>
#include <functional>
#include "../SynthySlider.h"   // KnobSize

// The unified-rack UI is data-driven (see ARCHITECTURE-SPINE AD-1/AD-4): every
// module is a ModuleDescriptor, not a bespoke component. This header defines ONLY
// the data model + the size-class table + slot accounting. Rendering lives in
// ModuleFrame (Story 1.2); placement in Rack (Story 1.3). No audio-thread code,
// no parameter definitions, no APVTS ownership here — pure UI-layer data.
namespace rack
{
    enum class SizeClass  { XS, S, M, L, XL };                      // 12-col grid spans: 2x1, 3x1, 4x1, 4x2, 6x2
    enum class ModuleType { Generator, Modulator, Processor };       // identity / colour tag
    enum class ModTarget  { None, Frequency, Amplitude, FilterCutoff }; // for live LFO rings (AD-8)

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
        ModTarget modTarget = ModTarget::None;
    };

    struct Combo
    {
        juce::String paramId;
        juce::String label;
        // Static items, OR a provider re-polled at refresh time (e.g. the
        // Wavetable bank list — refreshed declaratively via Action/FileAction
        // .refreshes in Story 1.5).
        std::variant<juce::StringArray, std::function<juce::StringArray()>> items;
    };

    struct Toggle
    {
        juce::String paramId;
        juce::String label;
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
        juce::String title;
        ModuleType   type {};
        juce::String enableParam;                 // empty => always-on (Master, ADSR, Mix-Mode)
        std::vector<juce::String> resetParams;    // the reset (↺) writes these defaults to APVTS
        std::vector<BodyElement>  body;
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
            // PROTOTYPE: column spans on the refined 12-column grid (cols × units). Decoupled
            // from knob size — the body fills the module width from its CONTENT (see
            // ModuleFrame::resized). slotCapacity is now only a generous debug guard, not a
            // layout driver. To be formalised in AD-2 via correct-course.
            case SizeClass::XS: return { 2, 1,  4, KnobSize::Small };  // 1–2 controls
            case SizeClass::S:  return { 3, 1,  6, KnobSize::Small };  // 3–4 controls
            case SizeClass::M:  return { 4, 1,  8, KnobSize::Small };  // 5–6 controls
            case SizeClass::L:  return { 4, 2, 16, KnobSize::Small };  // knobs + curve display (ADSR)
            case SizeClass::XL: return { 6, 2, 24, KnobSize::Small };  // wide visualisers (scope/spectrum)
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
