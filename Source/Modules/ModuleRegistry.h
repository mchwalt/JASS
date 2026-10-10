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
    void writeState (juce::AudioProcessorValueTreeState& apvts, juce::DynamicObject& root);
    void readState  (juce::AudioProcessorValueTreeState& apvts, const juce::var& root);
}
