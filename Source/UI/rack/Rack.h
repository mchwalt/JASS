#pragma once
#include <JuceHeader.h>
#include "ModuleDescriptor.h"
#include "ModuleFrame.h"
#include "SynthyLookAndFeel.h"

namespace rack
{
    // The Rack owns ALL module placement on a fixed 12-column × rack-unit grid (AD-2)
    // and is the SINGLE site of frame-outer-rectangle geometry (NFR1): no module
    // computes its own bounds. It builds and owns one ModuleFrame per descriptor,
    // groups them into zones (each preceded by a full-width zone header), and sets the
    // one shared SynthyLookAndFeel (AD-7) that every frame and control beneath it
    // inherits. It also fans the live feed (LFO value + played ratio) out to its frames
    // for modulation rings + display transforms (AD-8, Story 1.4).
    class Rack : public juce::Component
    {
    public:
        enum class Zone { Generators, Modulation, Processing, MasterBus };

        // The main rack is the default 8-column, three-zone grid. A narrower zone set /
        // column count builds a focused sub-rack (e.g. the top-right master-bus insert:
        // 1 column, just the MasterBus zone) — PROTOTYPE for the "everything is a module"
        // refinement (Stereo/Master as modules); to be formalised via correct-course.
        explicit Rack (juce::AudioProcessorValueTreeState& apvts,
                       int columns = kDefaultCols,
                       std::vector<Zone> zones = { Zone::Generators, Zone::Modulation, Zone::Processing });
        ~Rack() override;

        // Build + own a ModuleFrame for this descriptor, placed in the given zone.
        // Insertion order within a zone is the placement order.
        void addModule (Zone zone, ModuleDescriptor desc);

        void resized() override;            // THE placement + zone-header layout site
        void paint (juce::Graphics&) override;

        // Fan the live feed out to every frame (AD-8): the single editor timer reads the
        // processor's LFO atomic + active target + played ratio and calls this once per
        // tick. Each frame gates rings by its own enable and refreshes its transform knobs.
        void updateLiveFeed (bool lfoOn, ModTarget activeTarget, float lfoValue, double playedRatio);

        // Total stacked height the current population needs at `width` — lets the
        // editor size/verify the fixed window without scrolling (AC4).
        int preferredHeight (int width) const;

        // --- fixed grid constants (frozen from the mockup; AC4) ---
        static constexpr int kDefaultCols = 12;   // refined proportional grid (was 8); a pure
                                                  // layout raster, independent of knob size
        static constexpr int kGutter      = 10;   // uniform gutter between cells
        static constexpr int kHu          = 124;  // one rack-unit row height (L spans 2).
                                                  // Sized so a 1-unit body fits name caption +
                                                  // 46px knob + value box without shrinking.
        static constexpr int kZoneHeaderH = 28;   // full-width zone separator band
        static constexpr int kPad         = 8;    // inner padding around the grid

    private:
        struct Placed
        {
            ModuleFrame* frame = nullptr;
            Zone zone {};
            int cols = 1, units = 1;   // footprint from the size class
        };
        struct ZoneBand { juce::String text; ModuleType tag; juce::Rectangle<int> bounds; };

        // The ONE packing path (row-major first-fit, never overlaps). When apply is
        // true it sets frame bounds + zone-header bands; either way it returns the
        // total height consumed. Isolated here so it can later be swapped for a
        // different strategy (e.g. dense-fill) without touching the call sites.
        int layout (int width, bool apply);

        juce::AudioProcessorValueTreeState& apvts;
        const int cols;                            // columns this rack instance packs into
        std::vector<Zone> zoneOrder;               // zones rendered, top→bottom
        SynthyLookAndFeel lnf;                     // the single shared look (AD-7)
        juce::OwnedArray<ModuleFrame> frames;
        std::vector<Placed> placed;
        std::vector<ZoneBand> zoneBands;           // computed in layout(), drawn in paint()

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Rack)
    };
}
