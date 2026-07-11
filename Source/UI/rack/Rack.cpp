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
                case Rack::Zone::Generators:    return ModuleType::Generator;
                case Rack::Zone::Modulation:    return ModuleType::Modulator;
                case Rack::Zone::Processing:    return ModuleType::Processor;
                case Rack::Zone::Visualization: return ModuleType::Processor;
                case Rack::Zone::MasterBus:     return ModuleType::Processor;
            }
            return ModuleType::Generator;
        }

        juce::String zoneText (Rack::Zone z) noexcept
        {
            switch (z)
            {
                case Rack::Zone::Generators:    return "GENERATORS";
                case Rack::Zone::Modulation:    return "MODULATION";
                case Rack::Zone::Processing:    return "PROCESSING";
                case Rack::Zone::Visualization: return "VISUALIZATION";
                case Rack::Zone::MasterBus:     return "MASTER BUS";
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
        const bool vis  = desc.defaultVisible;              // factory visibility (Story 4.3)
        auto* f = frames.add (new ModuleFrame (apvts, std::move (desc)));
        addAndMakeVisible (*f);
        placed.push_back ({ id, f, spec.cols, spec.units });

        // Seed the RackLayout model (AD-10): call order becomes within-zone position, so the
        // default layout reproduces today's insertion-order packing. Factory visibility from
        // the descriptor (default true; false => starts hidden so the rack doesn't overflow).
        int pos = 0;
        for (const auto& e : layoutModel)
            if (e.zone == zone) ++pos;
        layoutModel.push_back ({ id, zone, pos, vis });
        defaultLayout.push_back ({ id, zone, pos, vis });   // stock layout (for reset + isDefault)
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

    void Rack::driveEnable (const juce::String& id, bool show)
    {
        // hide  => force the module disabled (it's "not present" for the synth);
        // show  => restore its FACTORY-DEFAULT enable state, NOT force-on — e.g. CROSS MOD
        //          defaults to disabled, so re-showing it must leave it disabled.
        if (const auto* p = placedById (id); p != nullptr && p->frame != nullptr)
        {
            const auto pid = p->frame->enableParamId();
            if (pid.isNotEmpty())
                if (auto* param = apvts.getParameter (pid))
                    param->setValueNotifyingHost (show ? param->getDefaultValue() : 0.0f);
        }
    }

    void Rack::setModuleVisible (const juce::String& id, bool visible)
    {
        // Interactive toggle: flip visibility AND couple the enable (hide ⇒ disable,
        // show ⇒ enable once). Order is owned by the list, so position is kept.
        bool changed = false;
        for (auto& e : layoutModel)
            if (e.id == id && e.visible != visible) { e.visible = visible; changed = true; }
        if (! changed) return;
        driveEnable (id, visible);
        relayout();
        writeLayoutToState();
        if (onLayoutChanged) onLayoutChanged();
    }

    void Rack::applyLayoutOrder (const std::vector<std::pair<juce::String, rack::Zone>>& ordered)
    {
        // The customization list is the authority on order + zone: assign each listed module
        // its zone and a within-zone position = its running index among same-zone entries in
        // `ordered`. Visibility is left untouched (owned by setModuleVisible).
        std::vector<std::pair<Zone, int>> next;   // per-zone running counter
        auto counterFor = [&] (Zone z) -> int&
        {
            for (auto& c : next) if (c.first == z) return c.second;
            next.push_back ({ z, 0 });
            return next.back().second;
        };

        for (const auto& [id, zone] : ordered)
            for (auto& e : layoutModel)
                if (e.id == id) { e.zone = zone; e.position = counterFor (zone)++; break; }

        relayout();
        writeLayoutToState();
        if (onLayoutChanged) onLayoutChanged();
    }

    // --- Layout persistence (Story 4.3, AD-11) --------------------------------------

    Rack::Zone Rack::zoneFromName (const juce::String& name)
    {
        if (name == "MODULATION")    return Zone::Modulation;
        if (name == "PROCESSING")    return Zone::Processing;
        if (name == "VISUALIZATION") return Zone::Visualization;
        if (name == "MASTER BUS")    return Zone::MasterBus;
        return Zone::Generators;
    }

    juce::var Rack::layoutToVar() const
    {
        juce::Array<juce::var> arr;
        for (const auto& e : layoutModel)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("id",   e.id);
            o->setProperty ("zone", zoneName (e.zone));
            o->setProperty ("pos",  e.position);
            o->setProperty ("vis",  e.visible);
            arr.add (juce::var (o));
        }
        return arr;
    }

    void Rack::applyLayoutVar (const juce::var& v)
    {
        // Restore from persisted layout: set zone/position/visible by id. Unknown ids are
        // ignored; modules absent from the data keep their default. NO enable coupling and NO
        // write-back here — this is the load path (enables come from their own params).
        if (auto* arr = v.getArray())
        {
            for (const auto& item : *arr)
            {
                const auto id = item.getProperty ("id", {}).toString();
                if (id.isEmpty()) continue;
                for (auto& e : layoutModel)
                    if (e.id == id)
                    {
                        e.zone     = zoneFromName (item.getProperty ("zone", {}).toString());
                        e.position = (int)  item.getProperty ("pos", e.position);
                        e.visible  = (bool) item.getProperty ("vis", e.visible);
                        break;
                    }
            }
        }
        relayout();
        if (onLayoutChanged) onLayoutChanged();
    }

    bool Rack::isDefaultLayout() const
    {
        for (const auto& e : layoutModel)
        {
            bool matched = false;
            for (const auto& d : defaultLayout)
                if (d.id == e.id)
                {
                    if (d.zone != e.zone || d.position != e.position || e.visible != d.visible) return false;
                    matched = true; break;
                }
            if (! matched) return false;
        }
        return true;
    }

    void Rack::writeLayoutToState()
    {
        const juce::Identifier prop (kLayoutStateProp);
        if (isDefaultLayout())
            apvts.state.removeProperty (prop, nullptr);              // default ⇒ no property (clean preset)
        else
            apvts.state.setProperty (prop, juce::JSON::toString (layoutToVar()), nullptr);
    }

    void Rack::resetLayout()
    {
        layoutModel = defaultLayout;   // restore factory zones + order + visibility
        relayout();
        writeLayoutToState();          // now default ⇒ clears the property
        enforceHiddenDisabled();       // factory-hidden modules must be silent (invariant)
        if (onLayoutChanged) onLayoutChanged();
    }

    void Rack::reloadLayoutFromState()
    {
        const auto s = apvts.state.getProperty (juce::Identifier (kLayoutStateProp)).toString();
        if (s.isNotEmpty())
            applyLayoutVar (juce::JSON::parse (s));
        else
        {
            layoutModel = defaultLayout;   // no stored layout ⇒ stock
            relayout();
            if (onLayoutChanged) onLayoutChanged();
        }
        enforceHiddenDisabled();   // never leave a hidden module audible
    }

    void Rack::enforceHiddenDisabled()
    {
        for (const auto& e : layoutModel)
            if (! e.visible)
                driveEnable (e.id, false);   // hidden ⇒ silent (invariant)
    }

    void Rack::setZoneVisible (Zone zone, bool visible)
    {
        // Bulk-set every module in the zone (no separate zone state). The zone's header is
        // derived in layout() — emptying a zone makes its header disappear automatically.
        // Each member's enable is coupled too (hide ⇒ disable, show ⇒ enable once).
        bool changed = false;
        for (auto& e : layoutModel)
            if (e.zone == zone && e.visible != visible)
            {
                e.visible = visible;
                driveEnable (e.id, visible);
                changed = true;
            }
        if (! changed) return;
        relayout();
        writeLayoutToState();
        if (onLayoutChanged) onLayoutChanged();
    }

    bool Rack::isModuleVisible (const juce::String& id) const
    {
        for (const auto& e : layoutModel)
            if (e.id == id) return e.visible;
        return false;
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
            // Derived zone visibility (Story 4.2): a zone with no visible module contributes
            // NO header band, no modules, no height — the header disappears automatically.
            bool anyVisible = false;
            for (const auto& e : layoutModel)
                if (e.zone == zone && e.visible) { anyVisible = true; break; }
            if (! anyVisible)
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
