#pragma once
#include <JuceHeader.h>

// Tempo-sync note divisions for the LFO rate and the delay time (Feature: Tempo-Sync).
//
// ONE canonical source for both the UI combo AND the DSP maths, so the choice index can
// never drift out of sync with the meaning (the "combo-index-bug class" the project has
// been bitten by before). Index 0 = "Free" (use the free-running knob); indices >0 are
// musical divisions of a beat.
namespace SyncDivision
{
    // Display == combo order == index order used by beatsPerCycle() below. Index 0 is Free.
    inline const juce::StringArray kNames
    {
        "Free", "1/1", "1/2", "1/4", "1/8", "1/16", "1/4T", "1/8T", "1/4.", "1/8."
    };

    // Beats spanned by one full cycle (one LFO period / one delay time). Free (0) returns 0.
    // A quarter note = 1 beat; whole note = 4; triplets = ×2/3; dotted = ×3/2.
    inline double beatsPerCycle (int index) noexcept
    {
        switch (index)
        {
            case 1:  return 4.0;          // 1/1  whole
            case 2:  return 2.0;          // 1/2  half
            case 3:  return 1.0;          // 1/4  quarter
            case 4:  return 0.5;          // 1/8
            case 5:  return 0.25;         // 1/16
            case 6:  return 1.0  * 2.0 / 3.0;   // 1/4T quarter triplet
            case 7:  return 0.5  * 2.0 / 3.0;   // 1/8T eighth triplet
            case 8:  return 1.0  * 3.0 / 2.0;   // 1/4. dotted quarter
            case 9:  return 0.5  * 3.0 / 2.0;   // 1/8. dotted eighth
            default: return 0.0;          // 0 = Free (not synced)
        }
    }

    inline bool isSynced (int index) noexcept { return index > 0; }

    // Effective LFO rate in Hz for a synced division at the given tempo.
    inline double lfoRateHz (double bpm, int index) noexcept
    {
        const double beats = beatsPerCycle (index);
        if (beats <= 0.0 || bpm <= 0.0) return 0.0;
        return bpm / (60.0 * beats);          // cycles per second
    }

    // Effective delay time in seconds for a synced division at the given tempo.
    inline double delaySeconds (double bpm, int index) noexcept
    {
        const double beats = beatsPerCycle (index);
        if (beats <= 0.0 || bpm <= 0.0) return 0.0;
        return beats * 60.0 / bpm;
    }
}
