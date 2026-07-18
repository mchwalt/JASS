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
}
