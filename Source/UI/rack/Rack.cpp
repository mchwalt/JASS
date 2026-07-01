#include "Rack.h"
#include <array>

namespace rack
{
    namespace
    {
        ModuleType zoneTag (Rack::Zone z) noexcept
        {
            switch (z)
            {
                case Rack::Zone::Generators: return ModuleType::Generator;
                case Rack::Zone::Modulation: return ModuleType::Modulator;
                case Rack::Zone::Processing: return ModuleType::Processor;
                case Rack::Zone::MasterBus:  return ModuleType::Processor;
            }
            return ModuleType::Generator;
        }

        juce::String zoneText (Rack::Zone z) noexcept
        {
            switch (z)
            {
                case Rack::Zone::Generators: return "GENERATORS";
                case Rack::Zone::Modulation: return "MODULATION";
                case Rack::Zone::Processing: return "PROCESSING";
                case Rack::Zone::MasterBus:  return "MASTER BUS";
            }
            return {};
        }
    }

    Rack::Rack (juce::AudioProcessorValueTreeState& a, int columns, std::vector<Zone> zones)
        : apvts (a), cols (juce::jmax (1, columns)), zoneOrder (std::move (zones))
    {
        // One shared look for everything beneath the rack (AD-7); children inherit it.
        setLookAndFeel (&lnf);
    }

    Rack::~Rack() { setLookAndFeel (nullptr); }

    void Rack::addModule (Zone zone, ModuleDescriptor desc)
    {
        const auto spec = sizeClassSpec (desc.sizeClass);   // read footprint BEFORE moving
        auto* f = frames.add (new ModuleFrame (apvts, std::move (desc)));
        addAndMakeVisible (*f);
        placed.push_back ({ f, zone, spec.cols, spec.units });
    }

    int Rack::preferredHeight (int width) const
    {
        // layout() does not mutate when apply == false, but it is non-const (it writes
        // member scratch only when applying) — measure via a const_cast-free copy path:
        return const_cast<Rack*> (this)->layout (width, /*apply*/ false);
    }

    int Rack::layout (int width, bool apply)
    {
        if (apply)
            zoneBands.clear();

        const int gridLeft  = kPad;
        const int gridWidth = juce::jmax (cols, width - 2 * kPad);   // guard tiny widths
        const int wc        = (gridWidth - (cols - 1) * kGutter) / cols;   // Wc

        int y = kPad;

        for (auto zone : zoneOrder)
        {
            // --- full-width zone header band ---
            if (apply)
                zoneBands.push_back ({ zoneText (zone), zoneTag (zone),
                                       { gridLeft, y, gridWidth, kZoneHeaderH } });
            y += kZoneHeaderH + kGutter;

            // --- pack this zone's frames into the cols-wide grid (row-major first-fit) ---
            const int zoneTopY = y;
            std::vector<std::vector<char>> occ;   // occupancy, grown on demand

            auto ensureRows = [&] (int upto)
            {
                while ((int) occ.size() <= upto)
                    occ.emplace_back ((size_t) cols, (char) 0);   // all columns free
            };
            auto fits = [&] (int r, int c, int fcols, int units) -> bool
            {
                if (c + fcols > cols) return false;
                for (int rr = r; rr < r + units; ++rr)
                {
                    if (rr >= (int) occ.size()) continue;   // beyond the grid = free
                    for (int cc = c; cc < c + fcols; ++cc)
                        if (occ[(size_t) rr][(size_t) cc]) return false;
                }
                return true;
            };

            struct Placement { ModuleFrame* frame; int fc, fr, fcols, funits; };
            std::vector<Placement> zonePlaced;
            int maxRowUsed = -1, maxColUsed = -1;
            for (const auto& p : placed)
            {
                if (p.zone != zone) continue;

                // first free top-left cell that fits the cols×units footprint
                int fr = 0, fc = 0;
                for (bool found = false; ! found; ++fr)
                    for (int c = 0; c + p.cols <= cols; ++c)
                        if (fits (fr, c, p.cols, p.units)) { fc = c; found = true; break; }
                --fr;   // the for-loop over-incremented once after finding

                ensureRows (fr + p.units - 1);
                for (int rr = fr; rr < fr + p.units; ++rr)
                    for (int cc = fc; cc < fc + p.cols; ++cc)
                        occ[(size_t) rr][(size_t) cc] = 1;

                if (apply)
                    zonePlaced.push_back ({ p.frame, fc, fr, p.cols, p.units });

                maxRowUsed = juce::jmax (maxRowUsed, fr + p.units - 1);
                maxColUsed = juce::jmax (maxColUsed, fc + p.cols - 1);
            }

            if (apply)
            {
                // The MASTER BUS zone hugs the RIGHT edge (it visually balances the zone
                // title on the left); every other zone packs flush left. A uniform column
                // shift right-aligns the whole block (exact for the single-row master bus).
                const bool alignRight = (zone == Zone::MasterBus);
                const int colShift = (alignRight && maxColUsed >= 0) ? (cols - 1 - maxColUsed) : 0;
                for (const auto& t : zonePlaced)
                {
                    const int px = gridLeft + (t.fc + colShift) * (wc + kGutter);
                    const int py = zoneTopY + t.fr * (kHu + kGutter);
                    const int pw = t.fcols * wc + (t.fcols - 1) * kGutter;
                    const int ph = t.funits * kHu + (t.funits - 1) * kGutter;
                    t.frame->setBounds (px, py, pw, ph);
                }
            }

            const int rowsUsed = maxRowUsed + 1;
            if (rowsUsed > 0)
                y = zoneTopY + rowsUsed * kHu + (rowsUsed - 1) * kGutter;
            y += kGutter;   // gap before the next zone
        }

        return y + kPad;
    }

    void Rack::resized()
    {
        layout (getWidth(), /*apply*/ true);
    }

    void Rack::updateLiveFeed (bool lfoOn, ModTarget activeTarget, float lfoValue, double playedRatio)
    {
        for (auto* f : frames)
            f->updateLiveFeed (lfoOn, activeTarget, lfoValue, playedRatio);
    }

    void Rack::paint (juce::Graphics& g)
    {
        g.fillAll (juce::Colour (0xff15181d));   // rack ground (matches mockup --ground)

        for (const auto& z : zoneBands)
        {
            auto b = z.bounds;
            const auto col = typeColour (z.tag);

            g.setColour (col);
            g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
            auto label = b.removeFromLeft (220);
            g.drawText (z.text, label, juce::Justification::centredLeft);

            const auto yMid = (float) b.getCentreY();
            g.setColour (col.withAlpha (0.35f));
            g.drawLine ((float) b.getX(), yMid, (float) b.getRight() - 4.0f, yMid, 1.5f);
        }
    }
}
