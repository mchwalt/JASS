#pragma once
#include <JuceHeader.h>

// Single source of truth for the app version is CMake's project(JASS VERSION …) in
// CMakeLists.txt, which JUCE surfaces as ProjectInfo::versionString (also the plugin
// version reported to a VST3 host). This helper just formats it for display.
namespace JASS
{
    // App version, CalVer YYYY.MM.MICRO. CMake stores the month unpadded (e.g. 2026.7.0);
    // for display we zero-pad it to two digits (2026.07.0) for consistent, sortable output.
    inline juce::String versionString()
    {
        auto parts = juce::StringArray::fromTokens(juce::String(ProjectInfo::versionString), ".", "");
        if (parts.size() >= 2 && parts[1].length() == 1)
            parts.set(1, "0" + parts[1]);   // month 7 -> 07
        return parts.joinIntoString(".");
    }
}
