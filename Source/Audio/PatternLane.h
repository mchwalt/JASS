#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <bitset>
#include <vector>
#include "Parameters.h"
#include "../DSP/StepSequencer.h"
#include "../DSP/Arpeggiator.h"

// One layer's pattern players (Story 21.8, AD-16): its STEP SEQ and its ARP, plus everything the
// pair needs between blocks — the latch, the host anchor, the held-keys edge, the record lock and
// the display atomics. STEP SEQ and ARP are layered modules since 21.8, so a lane is "that module
// pair, bound to one layer": lane 0 plays layer A with the unprefixed ids, lane 1 plays layer B
// with the "b" ids, each on its own MIDI channel, which the LayerRouter sends to the lane's layer.
// The processor holds an array of these and runs the same code for each; nothing in a lane knows
// another one — more lanes would be more entries in that array.
//
// The figure itself (the StepPattern, AD-14) lives in the PatternStore, one per lane, so presets,
// the DAW state and the rack's pattern cells reach it without going through the audio objects.
struct PatternLane
{
    // ---- set once (bind) ----------------------------------------------------------------------
    int          index = 0;          // = the layer it plays (0 = A, 1 = B)
    juce::String prefix;             // the layer's id prefix ("" / "b")
    int          channel = 14;       // its pattern MIDI channel

    // Cached parameter pointers (the layer's copies of the STEP SEQ / ARP params).
    std::atomic<float>* seqOn = nullptr;
    std::atomic<float>* seqLength = nullptr;
    std::atomic<float>* seqSync = nullptr;
    std::atomic<float>* seqRate = nullptr;
    std::atomic<float>* seqGate = nullptr;
    std::atomic<float>* seqAccent = nullptr;
    std::atomic<float>* arpOn = nullptr;
    std::atomic<float>* arpRate = nullptr;
    std::atomic<float>* arpMode = nullptr;
    std::atomic<float>* arpOctaves = nullptr;
    std::atomic<float>* arpGate = nullptr;

    void bind (juce::AudioProcessorValueTreeState& apvts, int laneIndex, const juce::String& idPrefix, int midiChannel)
    {
        using namespace Parameters;
        index = laneIndex;
        prefix = idPrefix;
        channel = midiChannel;
        auto p = [&] (const char* id) { auto* r = apvts.getRawParameterValue (prefix + id); jassert (r != nullptr); return r; };
        seqOn = p (ID::seqOn);       seqLength = p (ID::seqLength); seqSync = p (ID::seqSync);
        seqRate = p (ID::seqRate);   seqGate = p (ID::seqGate);     seqAccent = p (ID::seqAccent);
        arpOn = p (ID::arpOn);       arpRate = p (ID::arpRate);     arpMode = p (ID::arpMode);
        arpOctaves = p (ID::arpOctaves); arpGate = p (ID::arpGate);
        arpHeld.reserve (128);
    }

    juce::String id (const char* baseId) const { return prefix + baseId; }

    // ---- audio objects --------------------------------------------------------------------------
    StepSequencer seq;
    Arpeggiator   arp;
    std::vector<int> arpHeld;              // reused per block (reserved in bind)

    // ---- state between blocks -------------------------------------------------------------------
    bool seqKeyWasHeld = false;            // edge: a figure starting from silence (quantised entry)
    int  blockRoot = -1;                   // this block's held root (laneInput -> laneOutput)
    bool blockRequantize = false;          // this block restarts on the downbeat (laneInput -> laneOutput)
    std::bitset<128> hostKeysDown;         // 18.6: the "from silence" edge at sample resolution
    std::atomic<int>    latchedRoot { -1 };     // the latch: the root outlives the key (15.5)
    std::atomic<double> anchorPpq   { -1.0 };   // 18.6: host beats of step 0, -1 = none yet
    std::atomic<bool>   requantize  { false };  // preset-load latch: force a fresh quantised entry
    std::atomic<bool>   recordArmed { false };  // 15.4: the editor writes this lane's figure

    // ---- displays (message thread reads) ----------------------------------------------------------
    std::atomic<int>    noteDisplay { -1 };
    std::atomic<int>    stepDisplay { -1 };
    std::atomic<double> stepSecondsDisplay { 0.125 };

    // ---- this block (audio thread) ---------------------------------------------------------------
    bool seqActive() const noexcept { return seqOn->load() > 0.5f; }
    // STEP SEQ wins inside its layer (15.1: both replace the held chord, only one can run).
    bool arpActive() const noexcept { return ! seqActive() && arpOn->load() > 0.5f; }
    bool active() const noexcept    { return seqActive() || arpActive(); }

    void prepare (double sampleRate)
    {
        seq.prepare (sampleRate);
        arp.prepare (sampleRate);
    }

    // Shared key routing of this block: which layers take live keys and where the split is. A
    // lane's keys are the keys that reach its layer — the same rule the router applies to the
    // live hand, so a key either plays a layer live or feeds that layer's pattern, never both.
    struct KeyScope
    {
        bool playA = true, playB = false, split = false;
        int  splitNote = 48;
        bool reaches (int laneIndex, int note) const noexcept
        {
            const bool plays = laneIndex == 0 ? playA : playB;
            if (! plays) return false;
            if (! split) return true;
            return laneIndex == 0 ? note >= splitNote : note < splitNote;
        }
    };
};
