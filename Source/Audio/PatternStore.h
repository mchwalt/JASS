#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cstdint>
#include "../DSP/PercSequencer.h"   // kLanes / kMaxSteps — one definition for DSP, UI and store

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

// The processor owns one of these; the editor, PresetIO and the DAW-state path reach it through
// the processor. Stage 2 of 18.5 adds the STEP SEQ figure next to the drum grid.
struct PatternStore
{
    PercPattern perc;

    PatternStore() = default;
    PatternStore (const PatternStore&) = delete;              // arrays of atomics: not copyable,
    PatternStore& operator= (const PatternStore&) = delete;   // and nothing should want a copy
};
