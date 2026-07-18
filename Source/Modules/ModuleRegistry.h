#pragma once
#include <JuceHeader.h>
#include <vector>
#include <memory>

// AUDIO-safe registry declaration. Parameters.h includes ONLY this (no UI headers) and calls
// appendAllParameters() to add every spec-driven module's APVTS parameters. The definition lives
// in ModuleRegistry.cpp, which pulls the UI-side per-module headers (AllModules.h).
namespace Modules
{
    void appendAllParameters (std::vector<std::unique_ptr<juce::RangedAudioParameter>>& out);

    // Nested .synthy (v3): each module writes/reads ONE object keyed by its persistObject, whose
    // fields are the params keyed by persistKey. Choice values are stored as their canonical
    // choice string; bools as bool; floats/ints as numbers. Spec-driven — no hand field lists.
    void writeState (juce::AudioProcessorValueTreeState& apvts, juce::DynamicObject& root);
    void readState  (juce::AudioProcessorValueTreeState& apvts, const juce::var& root);
}
