#pragma once
#include <JuceHeader.h>
#include <vector>
#include <memory>

// AUDIO-safe registry declaration. Parameters.h includes ONLY this (no UI headers) and calls
// createParameterLayout() to get every spec-driven module's APVTS parameters, one parameter group
// per module. The definition lives in ModuleRegistry.cpp, which pulls the UI-side per-module
// headers (AllModules.h).
namespace Modules
{
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Nested .synthy (v3): each module writes/reads ONE object keyed by its persistObject, whose
    // fields are the params keyed by persistKey. Choice values are stored as their canonical
    // choice string; bools as bool; floats/ints as numbers. Spec-driven — no hand field lists.
    // Writes/reads layer A and the instrument modules on ONE object — the preset root up to v10
    // (readers still take that shape); v11 writes Instrument / LayerA separately via
    // writeInstrumentState / writeLayerState.
    void writeState (juce::AudioProcessorValueTreeState& apvts, juce::DynamicObject& root);
    void readState  (juce::AudioProcessorValueTreeState& apvts, const juce::var& root);

    // ── AD-16 (Story 21.5): layers ──────────────────────────────────────────────────────────
    // The id prefix of layer B's copy of every patch-module parameter ("filterCutoff" → "bfilterCutoff").
    inline const char* const kLayerBPrefix = "b";
    // Is `id` (a layer-A id, unprefixed) a parameter of a patch module, i.e. does it exist per layer?
    // False for instrument modules (master bus, sequencers, boxes, displays, keyboard, LAYERS).
    bool isLayeredParamId (const juce::String& id);
    // Every patch-module parameter id, unprefixed (layer A's spelling), in spec order.
    const juce::StringArray& layeredParamIds();
    // Is `id` (unprefixed) the on/off switch of a patch module (oscOn1, filterOn, ...)? Linked editing
    // leaves these alone: in A+B a module's switch acts on the layer on screen (maintainer 2026-11-23).
    bool isLayeredEnableParamId (const juce::String& id);
    // The patch modules only, ids prefixed with `prefix`, persisted under the SAME object and field
    // names as layer A — so a layer is "a preset's patch part", wherever it is stored (`LayerB`).
    void writeLayerState (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix, juce::DynamicObject& root);
    // FormatVersion 11: the instrument modules only (master bus, sequencers, boxes, displays,
    // keyboard, LAYERS) — the preset's "Instrument" object.
    void writeInstrumentState (juce::AudioProcessorValueTreeState& apvts, juce::DynamicObject& root);
    void readInstrumentState  (juce::AudioProcessorValueTreeState& apvts, const juce::var& root);
    void readLayerState  (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix, const juce::var& root);
}
