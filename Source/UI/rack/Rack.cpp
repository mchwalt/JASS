#include "Rack.h"
#include <array>
#include <algorithm>

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

    void Rack::addModule (ModuleDescriptor desc)
    {
        const auto spec = sizeClassSpec (desc.sizeClass);   // read footprint BEFORE moving
        const auto zone = desc.defaultZone;                 // AD-10: zone declared on descriptor
        const auto id   = desc.id;
        auto* f = frames.add (new ModuleFrame (apvts, std::move (desc)));
        addAndMakeVisible (*f);
        placed.push_back ({ id, f, spec.cols, spec.units });

        // Seed the RackLayout model (AD-10): call order becomes within-zone position, so the
        // default layout reproduces today's insertion-order packing exactly. visible defaults true.
        int pos = 0;
        for (const auto& e : layoutModel)
            if (e.zone == zone) ++pos;
        layoutModel.push_back ({ id, zone, pos, /*visible*/ true });
    }

    const Rack::Placed* Rack::placedById (const juce::String& id) const
    {
        for (const auto& p : placed)
            if (p.id == id) return &p;
        return nullptr;
    }

    juce::String Rack::zoneName (Zone zone) { return zoneText (zone); }

    void Rack::relayout()
    {
        layout (getWidth(), /*apply*/ true);
        repaint();
    }

    void Rack::setModuleVisible (const juce::String& id, bool visible)
    {
        RackLayoutEntry* target = nullptr;
        for (auto& e : layoutModel)
            if (e.id == id) { target = &e; break; }
        if (target == nullptr || target->visible == visible) return;

        target->visible = visible;
        if (visible)
        {
            // Re-showing appends the module at the END of its zone: module order follows the
            // order of (re-)selection (customization rule), not the original build slot.
            int maxPos = -1;
            for (const auto& e : layoutModel)
                if (e.zone == target->zone && &e != target)
                    maxPos = juce::jmax (maxPos, e.position);
            target->position = maxPos + 1;
        }
        relayout();
        if (onLayoutChanged) onLayoutChanged();
    }

    void Rack::setZoneVisible (Zone zone, bool visible)
    {
        auto it = std::find (hiddenZones.begin(), hiddenZones.end(), zone);
        const bool currentlyHidden = (it != hiddenZones.end());
        if      (visible && currentlyHidden)     hiddenZones.erase (it);
        else if (! visible && ! currentlyHidden) hiddenZones.push_back (zone);
        else return;   // no change
        relayout();
        if (onLayoutChanged) onLayoutChanged();
    }

    bool Rack::isModuleVisible (const juce::String& id) const
    {
        for (const auto& e : layoutModel)
            if (e.id == id) return e.visible;
        return false;
    }

    bool Rack::isZoneVisible (Zone zone) const
    {
        return std::find (hiddenZones.begin(), hiddenZones.end(), zone) == hiddenZones.end();
    }

    std::vector<Rack::ModuleInfo> Rack::modulesInZone (Zone zone) const
    {
        std::vector<const RackLayoutEntry*> es;
        for (const auto& e : layoutModel)
            if (e.zone == zone) es.push_back (&e);
        std::stable_sort (es.begin(), es.end(),
                          [] (const RackLayoutEntry* a, const RackLayoutEntry* b)
                          { return a->position < b->position; });

        std::vector<ModuleInfo> out;
        for (const auto* e : es)
        {
            juce::String title = e->id;
            if (const auto* p = placedById (e->id); p != nullptr && p->frame != nullptr)
                title = p->frame->moduleTitle();
            out.push_back ({ e->id, title, e->visible });
        }
        return out;
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

        std::vector<ModuleFrame*> shownFrames;   // frames actually placed this pass (apply only)

        const int gridLeft  = kPad;
        const int gridWidth = juce::jmax (cols, width - 2 * kPad);   // guard tiny widths
        const int wc        = (gridWidth - (cols - 1) * kGutter) / cols;   // Wc

        int y = kPad;

        for (auto zone : zoneOrder)
        {
            // Hidden zone (Story 4.2): no header band, no modules, no height contribution.
            if (std::find (hiddenZones.begin(), hiddenZones.end(), zone) != hiddenZones.end())
                continue;

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

            // AD-10: walk the RackLayout model for this zone — visible only, ordered by
            // position — instead of raw insertion order of `placed`. Position is seeded in
            // call order (addModule), so the default layout packs identically to before.
            std::vector<const RackLayoutEntry*> entries;
            for (const auto& e : layoutModel)
                if (e.zone == zone && e.visible)
                    entries.push_back (&e);
            std::stable_sort (entries.begin(), entries.end(),
                              [] (const RackLayoutEntry* a, const RackLayoutEntry* b)
                              { return a->position < b->position; });

            for (const auto* e : entries)
            {
                const auto* pl = placedById (e->id);
                if (pl == nullptr || pl->frame == nullptr) continue;
                const int fcols = pl->cols, funits = pl->units;

                // first free top-left cell that fits the cols×units footprint
                int fr = 0, fc = 0;
                for (bool found = false; ! found; ++fr)
                    for (int c = 0; c + fcols <= cols; ++c)
                        if (fits (fr, c, fcols, funits)) { fc = c; found = true; break; }
                --fr;   // the for-loop over-incremented once after finding

                ensureRows (fr + funits - 1);
                for (int rr = fr; rr < fr + funits; ++rr)
                    for (int cc = fc; cc < fc + fcols; ++cc)
                        occ[(size_t) rr][(size_t) cc] = 1;

                if (apply)
                    zonePlaced.push_back ({ pl->frame, fc, fr, fcols, funits });

                maxRowUsed = juce::jmax (maxRowUsed, fr + funits - 1);
                maxColUsed = juce::jmax (maxColUsed, fc + fcols - 1);
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
                    shownFrames.push_back (t.frame);
                }
            }

            const int rowsUsed = maxRowUsed + 1;
            if (rowsUsed > 0)
                y = zoneTopY + rowsUsed * kHu + (rowsUsed - 1) * kGutter;
            y += kGutter;   // gap before the next zone
        }

        // Frames not placed this pass (module hidden or its zone hidden) are taken out of
        // view — but kept alive with their APVTS attachments intact (hiding is UI-only).
        if (apply)
            for (auto* f : frames)
                f->setVisible (std::find (shownFrames.begin(), shownFrames.end(), f) != shownFrames.end());

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

    ModuleFrame* Rack::moduleById (const juce::String& id)
    {
        for (auto* f : frames)
            if (f != nullptr && f->moduleId() == id)
                return f;
        return nullptr;
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
