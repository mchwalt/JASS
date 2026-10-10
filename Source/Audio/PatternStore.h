#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cstdint>
#include "../DSP/PercSequencer.h"   // kLanes / kMaxSteps — one definition for DSP, UI and store
#include "../DSP/StepSequencer.h"   // kMaxSteps, kGateTie / kGateSlide — the figure's cell vocabulary

// AD-14 (Story 18.5): sequencer patterns are CONTENT, not parameters.
//
// Until 18.5 every cell of the PERC grid was an APVTS parameter — 4 x 768 of them — because that
// bought preset persistence, DAW state, LiveState and the rack binding for free. In a DAW it also
// bought a parameter list of thousands of entries that no host hides reliably (Bitwig honours the
// not-automatable flag, Cubase only the VST3 hidden flag JUCE never sets), and the maintainer's
// verdict was the right one: a pattern is what you WRITE INTO the instrument, the way a 303 keeps
// its pattern in memory and its knobs on the panel. So the cells live here, and the host sees the
// knobs.
//
// Threading contract — the same one the parameters gave us, made explicit:
//   * the MESSAGE thread writes cells (grid clicks, preset load, DAW state, MIDI import, reset);
//   * the AUDIO thread reads the used range once per block (PluginProcessor::processBlock), one
//     relaxed atomic load per cell — exactly what getRawParameterValue cost before. No lock, no
//     allocation, no copy of the whole array.
//   * `revision` is bumped on every write that changes a cell. Pollers (the grid's repaint timer,
//     the preset-modified check) compare one integer instead of 3072 cells.
//
// Persistence is NOT in here: PresetIO writes the preset's `Perc.Lanes[].Steps` row strings from
// the store and reads them back; PluginProcessor serialises the DAW state via writeXml/readXml.
// Both use the same row-string form ('X' = hit, '.' = rest) so a LiveState file and a preset read
// the same.
struct PercPattern
{
    static constexpr int kLanes    = PercSequencer::kLanes;
    static constexpr int kMaxSteps = PercSequencer::kMaxSteps;

    // 0-based lane and step everywhere in this struct. Out-of-range reads are rests, writes no-ops.
    bool get (int lane, int step) const noexcept
    {
        return inRange (lane, step) && on[(size_t) lane][(size_t) step].load (std::memory_order_relaxed) != 0;
    }

    void set (int lane, int step, bool hit) noexcept
    {
        if (! inRange (lane, step)) return;
        auto& cell = on[(size_t) lane][(size_t) step];
        const uint8_t v = hit ? 1 : 0;
        if (cell.exchange (v, std::memory_order_relaxed) != v)
            revision.fetch_add (1, std::memory_order_relaxed);
    }

    void clear() noexcept
    {
        bool changed = false;
        for (auto& lane : on)
            for (auto& cell : lane)
                changed |= (cell.exchange (0, std::memory_order_relaxed) != 0);
        if (changed)
            revision.fetch_add (1, std::memory_order_relaxed);
    }

    // Highest step (0-based) carrying a hit in ANY lane, or -1 for an empty grid. The writers use
    // it to trim the rows they emit: a file is read by eye, and 16 pages of rests say nothing.
    int lastHit() const noexcept
    {
        for (int s = kMaxSteps - 1; s >= 0; --s)
            for (int l = 0; l < kLanes; ++l)
                if (get (l, s)) return s;
        return -1;
    }

    // The row as the grid shows it: 'X' = hit, '.' = rest, `steps` characters long.
    juce::String laneString (int lane, int steps) const
    {
        juce::String row;
        row.preallocateBytes ((size_t) juce::jmax (0, steps));
        for (int s = 0; s < juce::jlimit (0, kMaxSteps, steps); ++s)
            row << (get (lane, s) ? 'X' : '.');
        return row;
    }

    // Writes the row from its first character; cells past the string's end become rests, so a
    // short row (every file written before 16.3 has 48 or fewer) loads padded, as it always did.
    void setLaneString (int lane, const juce::String& row)
    {
        for (int s = 0; s < kMaxSteps; ++s)
            set (lane, s, s < row.length() && (row[s] == 'X' || row[s] == 'x'));
    }

    // DAW state (PluginProcessor::get/setStateInformation): ONE child element on the APVTS XML,
    //   <PercPattern lane1="X...X..." lane2="..." lane3="..." lane4="..."/>
    // rows trimmed to the last hit (an empty grid writes four empty attributes, which still says
    // "a pattern block was here" — the reader clears the grid for a state that has none).
    static constexpr const char* kXmlTag = "PercPattern";

    void writeXml (juce::XmlElement& parent) const
    {
        if (auto* old = parent.getChildByName (kXmlTag))
            parent.removeChildElement (old, true);
        auto* el = parent.createNewChildElement (kXmlTag);
        const int used = lastHit() + 1;
        for (int l = 0; l < kLanes; ++l)
            el->setAttribute ("lane" + juce::String (l + 1), laneString (l, used));
    }

    // True when a pattern block was present (and has been applied). Removes the element so the
    // tree handed to replaceState carries parameters only.
    bool readXml (juce::XmlElement& parent)
    {
        auto* el = parent.getChildByName (kXmlTag);
        if (el == nullptr) return false;
        for (int l = 0; l < kLanes; ++l)
            setLaneString (l, el->getStringAttribute ("lane" + juce::String (l + 1)));
        parent.removeChildElement (el, true);
        return true;
    }

    std::atomic<uint32_t> revision { 0 };

private:
    static bool inRange (int lane, int step) noexcept
    {
        return lane >= 0 && lane < kLanes && step >= 0 && step < kMaxSteps;
    }

    std::array<std::array<std::atomic<uint8_t>, kMaxSteps>, kLanes> on {};
};

// The STEP SEQ figure (18.5 stage 2): per step a semitone offset from the root (-24..+24), an ON
// flag (off = rest), an ACCENT flag (15.2) and the GATE (15.7: 5..100 % of the step, 101 = TIE,
// 102 = SLIDE). Same contract as PercPattern: message thread writes, audio thread reads the used
// range per block, `revision` bumps on every change. Factory default of a step: off, 0, plain, 100.
//
// The BASELINE is the figure as the preset loaded (or last saved) it — markClean() takes the
// snapshot. The rack's double-click-on-a-knob restores a cell from it, the way presetBaseline01
// does for a parameter (maintainer 2026-08-26). Message thread only.
struct StepPattern
{
    static constexpr int kMaxSteps   = StepSequencer::kMaxSteps;
    static constexpr int kPitchMin   = -24, kPitchMax = 24;
    static constexpr int kGateMin    = 5,   kGateMax  = 102;
    static constexpr int kGateDefault = 100, kGateTie = 101, kGateSlide = 102;

    int  pitch  (int s) const noexcept { return inRange (s) ? (int) pitchArr [(size_t) s].load (std::memory_order_relaxed) : 0; }
    bool on     (int s) const noexcept { return inRange (s) && onArr    [(size_t) s].load (std::memory_order_relaxed) != 0; }
    bool accent (int s) const noexcept { return inRange (s) && accArr   [(size_t) s].load (std::memory_order_relaxed) != 0; }
    int  gate   (int s) const noexcept { return inRange (s) ? (int) gateArr[(size_t) s].load (std::memory_order_relaxed) : kGateDefault; }

    void setPitch  (int s, int semis) noexcept { if (inRange (s)) store (pitchArr[(size_t) s], (int8_t)  juce::jlimit (kPitchMin, kPitchMax, semis)); }
    void setOn     (int s, bool v)    noexcept { if (inRange (s)) store (onArr   [(size_t) s], (uint8_t) (v ? 1 : 0)); }
    void setAccent (int s, bool v)    noexcept { if (inRange (s)) store (accArr  [(size_t) s], (uint8_t) (v ? 1 : 0)); }
    void setGate   (int s, int g)     noexcept { if (inRange (s)) store (gateArr [(size_t) s], (uint8_t) juce::jlimit (kGateMin, kGateMax, g)); }

    bool isDefault (int s) const noexcept
    {
        return ! on (s) && pitch (s) == 0 && ! accent (s) && gate (s) == kGateDefault;
    }

    // Highest step (0-based) that differs from the factory default, or -1 for an empty figure.
    int lastUsed() const noexcept
    {
        for (int s = kMaxSteps - 1; s >= 0; --s)
            if (! isDefault (s)) return s;
        return -1;
    }

    void clear() noexcept
    {
        for (int s = 0; s < kMaxSteps; ++s)
        {
            setPitch (s, 0); setOn (s, false); setAccent (s, false); setGate (s, kGateDefault);
        }
    }

    // --- baseline (double-click = as loaded) ---
    void markClean() noexcept
    {
        for (int s = 0; s < kMaxSteps; ++s)
        {
            basePitch[(size_t) s] = (int8_t)  pitch (s);
            baseGate [(size_t) s] = (uint8_t) gate  (s);
        }
    }
    int baselinePitch (int s) const noexcept { return inRange (s) ? (int) basePitch[(size_t) s] : 0; }
    int baselineGate  (int s) const noexcept { return inRange (s) ? (int) baseGate [(size_t) s] : kGateDefault; }

    // DAW state: ONE child element on the APVTS XML, rows trimmed to the last used step:
    //   <StepPattern on="X.X." pitch="0 7 0 12" accent="..X." gate="100 100 101 50"/>
    static constexpr const char* kXmlTag = "StepPattern";

    void writeXml (juce::XmlElement& parent) const
    {
        if (auto* old = parent.getChildByName (kXmlTag))
            parent.removeChildElement (old, true);
        auto* el = parent.createNewChildElement (kXmlTag);
        const int used = lastUsed() + 1;
        juce::String onRow, accRow;
        juce::StringArray pitches, gates;
        for (int s = 0; s < used; ++s)
        {
            onRow  << (on (s)     ? 'X' : '.');
            accRow << (accent (s) ? 'X' : '.');
            pitches.add (juce::String (pitch (s)));
            gates.add   (juce::String (gate (s)));
        }
        el->setAttribute ("on",     onRow);
        el->setAttribute ("accent", accRow);
        el->setAttribute ("pitch",  pitches.joinIntoString (" "));
        el->setAttribute ("gate",   gates.joinIntoString (" "));
    }

    bool readXml (juce::XmlElement& parent)
    {
        auto* el = parent.getChildByName (kXmlTag);
        if (el == nullptr) return false;
        const juce::String onRow = el->getStringAttribute ("on"), accRow = el->getStringAttribute ("accent");
        juce::StringArray pitches, gates;
        pitches.addTokens (el->getStringAttribute ("pitch"), " ", {});
        gates.addTokens   (el->getStringAttribute ("gate"),  " ", {});
        for (int s = 0; s < kMaxSteps; ++s)
        {
            setOn     (s, s < onRow.length()  && (onRow[s]  == 'X' || onRow[s]  == 'x'));
            setAccent (s, s < accRow.length() && (accRow[s] == 'X' || accRow[s] == 'x'));
            setPitch  (s, s < pitches.size() ? pitches[s].getIntValue() : 0);
            setGate   (s, s < gates.size()   ? gates[s].getIntValue()   : kGateDefault);
        }
        parent.removeChildElement (el, true);
        return true;
    }

    std::atomic<uint32_t> revision { 0 };

private:
    static bool inRange (int s) noexcept { return s >= 0 && s < kMaxSteps; }

    template <typename T>
    void store (std::atomic<T>& cell, T v) noexcept
    {
        if (cell.exchange (v, std::memory_order_relaxed) != v)
            revision.fetch_add (1, std::memory_order_relaxed);
    }

    std::array<std::atomic<int8_t>,  kMaxSteps> pitchArr {};
    std::array<std::atomic<uint8_t>, kMaxSteps> onArr {};
    std::array<std::atomic<uint8_t>, kMaxSteps> accArr {};
    std::array<std::atomic<uint8_t>, kMaxSteps> gateArr {};   // 0 until the first write — see ctor
    std::array<int8_t,  kMaxSteps> basePitch {};
    std::array<uint8_t, kMaxSteps> baseGate {};

public:
    StepPattern() noexcept
    {
        for (auto& g : gateArr) g.store ((uint8_t) kGateDefault, std::memory_order_relaxed);
        for (auto& g : baseGate) g = (uint8_t) kGateDefault;
    }
};

// The processor owns one of these; the editor, PresetIO and the DAW-state path reach it through
// the processor.
struct PatternStore
{
    PercPattern perc;
    StepPattern step;

    PatternStore() = default;
    PatternStore (const PatternStore&) = delete;              // arrays of atomics: not copyable,
    PatternStore& operator= (const PatternStore&) = delete;   // and nothing should want a copy
};
