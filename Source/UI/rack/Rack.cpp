#include "Rack.h"
#include "../HelpTextStore.h"   // has()/get() for zone info icons (Story 6.1 pattern)
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
                case Rack::Zone::Input:         return ModuleType::Generator;
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
                case Rack::Zone::Input:         return "INPUT";
            }
            return {};
        }
    }

    Rack::Rack (juce::AudioProcessorValueTreeState& a, int columns, std::vector<Zone> zones)
        : apvts (a), cols (juce::jmax (1, columns)), zoneOrder (std::move (zones))
    {
        // One shared look for everything beneath the rack (AD-7); children inherit it.
        setLookAndFeel (&lnf);
        buildZoneHeaders();   // enable/reset/info controls for every zone header
    }

    juce::String Rack::zoneHelpId (Zone zone)
    {
        switch (zone)
        {
            case Zone::Generators:    return "zone-generators";
            case Zone::Modulation:    return "zone-modulation";
            case Zone::Processing:    return "zone-processing";
            case Zone::Visualization: return "zone-visualization";
            case Zone::MasterBus:     return "zone-masterbus";
            case Zone::Input:         return "zone-input";
        }
        return {};
    }

    void Rack::buildZoneHeaders()
    {
        // One control set per zone in render order. Built up-front (modules are added later),
        // so enable/reset exist for every zone; layout() shows/hides + positions them per pass.
        for (auto zone : zoneOrder)
        {
            ZoneHeaderControls zh;
            zh.zone = zone;
            const auto tint = typeColour (zoneTag (zone));

            // Group-bypass toggle (manual state — no APVTS attachment; syncZoneHeaderToggles
            // keeps it aligned with the members). We drive the toggle state ourselves (no
            // auto-flip) and derive the click target from the LIVE group state: any member on
            // ⇒ turn the whole group off, otherwise turn it on. (Using the button's own flipped
            // state fails for zones whose factory state isn't "all on" — e.g. CROSS MOD is off
            // by default — where the timer would immediately snap the toggle back.)
            zh.enableBtn = std::make_unique<juce::ToggleButton>();
            zh.enableBtn->setClickingTogglesState (false);
            zh.enableBtn->setColour (juce::ToggleButton::tickColourId,        tint);
            zh.enableBtn->setColour (juce::ToggleButton::tickDisabledColourId, tint.withAlpha (0.5f));
            zh.enableBtn->setTooltip ("Enable / bypass this whole group");
            zh.enableBtn->onClick = [this, zone]
            {
                int enableable = 0, on = 0;
                zoneEnableCounts (zone, enableable, on);
                setZoneEnabled (zone, on == 0);   // none on ⇒ enable all; any on ⇒ bypass all
            };
            addAndMakeVisible (*zh.enableBtn);

            zh.resetBtn = std::make_unique<IconButton> (IconButton::Kind::Reset);
            zh.resetBtn->setTint (tint);
            zh.resetBtn->setTooltip ("Restore this group's default modules");
            zh.resetBtn->onClick = [this, zone] { resetZone (zone); };
            addAndMakeVisible (*zh.resetBtn);

            // Info icon only when a zone help text exists (mirrors the module info icon).
            if (HelpTextStore::instance().has (zoneHelpId (zone)))
            {
                zh.infoBtn = std::make_unique<IconButton> (IconButton::Kind::Info);
                zh.infoBtn->setTint (tint);
                zh.infoBtn->setTooltip ("What is this group?");
                zh.infoBtn->onClick = [this, zone] { if (onZoneHelp) onZoneHelp (zone); };
                addAndMakeVisible (*zh.infoBtn);
            }

            zoneHeaders.push_back (std::move (zh));
        }
    }

    Rack::ZoneHeaderControls* Rack::zoneHeaderFor (Zone zone)
    {
        for (auto& zh : zoneHeaders)
            if (zh.zone == zone) return &zh;
        return nullptr;
    }

    juce::Component* Rack::zoneInfoButton (Zone zone)
    {
        if (auto* zh = zoneHeaderFor (zone)) return zh->infoBtn.get();
        return nullptr;
    }

    void Rack::setFactoryEnableDefault (const juce::String& enableParamId, float normValue)
    {
        if (enableParamId.isNotEmpty())
            factoryEnableByParam[enableParamId] = normValue;
    }

    void Rack::zoneEnableCounts (Zone zone, int& enableable, int& on) const
    {
        enableable = 0; on = 0;
        for (const auto& e : layoutModel)
        {
            if (e.zone != zone || ! e.visible) continue;
            const auto* p = placedById (e.id);
            if (p == nullptr || p->frame == nullptr) continue;
            const auto pid = p->frame->enableParamId();
            if (pid.isEmpty()) continue;
            ++enableable;
            if (auto* raw = apvts.getRawParameterValue (pid); raw != nullptr && raw->load() >= 0.5f)
                ++on;
        }
    }

    void Rack::setZoneEnabled (Zone zone, bool enabled)
    {
        // Group "mute with memory" over the VISIBLE enable-capable members (hidden modules stay
        // untouched — invariant). DISABLE: remember each member's current enable value, then
        // turn them all off. ENABLE: restore the remembered values so exactly the members that
        // were on come back; if nothing was remembered (the group was never bypassed — e.g. all
        // effects start off), fall back to turning the whole group ON so the toggle still acts.
        auto* zh = zoneHeaderFor (zone);

        if (! enabled)
        {
            if (zh != nullptr) zh->bypassSnapshot.clear();
            for (const auto& e : layoutModel)
            {
                if (e.zone != zone || ! e.visible) continue;
                const auto* p = placedById (e.id);
                if (p == nullptr || p->frame == nullptr) continue;
                const auto pid = p->frame->enableParamId();
                if (pid.isEmpty()) continue;
                if (auto* param = apvts.getParameter (pid))
                {
                    if (zh != nullptr) zh->bypassSnapshot[e.id] = param->getValue();   // remember
                    param->setValueNotifyingHost (0.0f);
                }
            }
        }
        else
        {
            const bool haveSnapshot = (zh != nullptr && ! zh->bypassSnapshot.empty());
            for (const auto& e : layoutModel)
            {
                if (e.zone != zone || ! e.visible) continue;
                const auto* p = placedById (e.id);
                if (p == nullptr || p->frame == nullptr) continue;
                const auto pid = p->frame->enableParamId();
                if (pid.isEmpty()) continue;
                if (auto* param = apvts.getParameter (pid))
                {
                    float val = 1.0f;   // fallback: turn the whole group on
                    if (haveSnapshot)
                    {
                        auto it = zh->bypassSnapshot.find (e.id);
                        val = (it != zh->bypassSnapshot.end()) ? it->second : 0.0f;
                    }
                    param->setValueNotifyingHost (val);
                }
            }
            if (zh != nullptr) zh->bypassSnapshot.clear();
        }

        syncZoneHeaderToggles();
    }

    void Rack::resetZone (Zone zone)
    {
        // Restore the group's default module selection: for every module whose FACTORY zone is
        // `zone`, put its default {zone, position, visible} back. (A module dragged OUT of this
        // zone is pulled back; a foreign module dragged IN is restored when ITS zone is reset.)
        for (auto& e : layoutModel)
            for (const auto& d : defaultLayout)
                if (d.id == e.id && d.zone == zone)
                {
                    e.zone     = d.zone;
                    e.position = d.position;
                    e.visible  = d.visible;
                    break;
                }

        // Restore DEFAULT enable state too (user-requested: reset = default visibility + enable).
        // The factory enable value is the param's APVTS default, EXCEPT where the shipped Init
        // patch overrides it (factoryEnableByParam — e.g. oscOn is declared 0 but Init turns it
        // on). Using getDefaultValue() blindly would silence OSC/most generators (their declared
        // default is 0), which is exactly the "reset turns everything off" bug. Knobs untouched.
        if (auto* zh = zoneHeaderFor (zone)) zh->bypassSnapshot.clear();   // drop any stashed bypass
        for (const auto& e : layoutModel)
        {
            if (e.zone != zone || ! e.visible) continue;
            const auto* p = placedById (e.id);
            if (p == nullptr || p->frame == nullptr) continue;
            const auto pid = p->frame->enableParamId();
            if (pid.isEmpty()) continue;
            if (auto* param = apvts.getParameter (pid))
            {
                float val = param->getDefaultValue();
                if (auto it = factoryEnableByParam.find (pid); it != factoryEnableByParam.end())
                    val = it->second;
                param->setValueNotifyingHost (val);
            }
        }

        relayout();
        writeLayoutToState();
        enforceHiddenDisabled();     // factory-hidden modules must stay silent (invariant)
        syncZoneHeaderToggles();
        if (onLayoutChanged) onLayoutChanged();
    }

    void Rack::syncZoneHeaderToggles()
    {
        // The enable toggle is lit when ANY visible enable-capable member is on (so a group
        // that is only partly on still reads as "on", and a click bypasses the whole group).
        for (auto& zh : zoneHeaders)
        {
            int enableable = 0, on = 0;
            zoneEnableCounts (zh.zone, enableable, on);
            if (zh.enableBtn != nullptr)
                zh.enableBtn->setToggleState (on > 0, juce::dontSendNotification);
        }
    }

    Rack::~Rack() { setLookAndFeel (nullptr); }

    void Rack::addModule (ModuleDescriptor desc)
    {
        const auto spec = sizeClassSpec (desc.sizeClass);   // read footprint BEFORE moving
        const auto zone = desc.defaultZone;                 // AD-10: zone declared on descriptor
        const auto id   = desc.id;
        const bool vis  = desc.defaultVisible;              // factory visibility (Story 4.3)
        const bool alignR = desc.alignRight;                // per-module H-alignment within the zone
        auto* f = frames.add (new ModuleFrame (apvts, std::move (desc)));
        addAndMakeVisible (*f);
        // Forward this frame's help-icon click up to the editor (Story 6.1).
        f->onHelp = [this] (const juce::String& mid) { if (onModuleHelp) onModuleHelp (mid); };
        placed.push_back ({ id, f, spec.cols, spec.units, alignR });

        // Seed the RackLayout model (AD-10): call order becomes within-zone position, so the
        // default layout reproduces today's insertion-order packing. Factory visibility from
        // the descriptor (default true; false => starts hidden so the rack doesn't overflow).
        int pos = 0;
        for (const auto& e : layoutModel)
            if (e.zone == zone) ++pos;
        layoutModel.push_back ({ id, zone, pos, vis, alignR });
        defaultLayout.push_back ({ id, zone, pos, vis, alignR });   // stock layout (for reset + isDefault)
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

    void Rack::setModuleAlignRight (const juce::String& id, bool alignRight)
    {
        bool changed = false;
        for (auto& e : layoutModel)
            if (e.id == id && e.alignRight != alignRight) { e.alignRight = alignRight; changed = true; }
        if (! changed) return;
        relayout();                 // UI-only: no enable coupling (unlike setModuleVisible)
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
        if (name == "INPUT")         return Zone::Input;
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
            o->setProperty ("alignR", e.alignRight);
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
                        e.zone       = zoneFromName (item.getProperty ("zone", {}).toString());
                        e.position   = (int)  item.getProperty ("pos", e.position);
                        e.visible    = (bool) item.getProperty ("vis", e.visible);
                        e.alignRight = (bool) item.getProperty ("alignR", e.alignRight);
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
                    if (d.zone != e.zone || d.position != e.position || e.visible != d.visible
                        || e.alignRight != d.alignRight) return false;
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
        // BEFORE enforcing the hidden⇒silent invariant: reveal any module the preset left enabled
        // but that the layout hides (e.g. a preset using the default-hidden COMPRESSOR). Otherwise
        // enforceHiddenDisabled would silence it and the user would never see it was in the patch.
        if (revealEnabledModules())
        {
            relayout();
            writeLayoutToState();
            if (onLayoutChanged) onLayoutChanged();
        }
        enforceHiddenDisabled();   // any STILL-hidden module ⇒ silent (invariant)
    }

    void Rack::enforceHiddenDisabled()
    {
        for (const auto& e : layoutModel)
            if (! e.visible)
                driveEnable (e.id, false);   // hidden ⇒ silent (invariant)
    }

    bool Rack::revealEnabledModules()
    {
        bool changed = false;
        for (auto& e : layoutModel)
            if (! e.visible)
                if (const auto* p = placedById (e.id); p != nullptr && p->frame != nullptr)
                {
                    const auto pid = p->frame->enableParamId();
                    if (pid.isNotEmpty())
                        if (auto* param = apvts.getParameter (pid); param != nullptr && param->getValue() > 0.5f)
                        { e.visible = true; changed = true; }   // enabled ⇒ show (leave the enable as-is)
                }
        return changed;
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
            out.push_back ({ e->id, title, e->visible, e->alignRight });
        }
        return out;
    }

    int Rack::preferredHeight (int width) const
    {
        // layout() does not mutate when apply == false, but it is non-const (it writes
        // member scratch only when applying) — measure via a const_cast-free copy path:
        return const_cast<Rack*> (this)->layout (width, /*apply*/ false);
    }

    int Rack::maxHeight (int width) const
    {
        // Measure the fully-populated rack: flip every entry visible, measure (apply == false,
        // so nothing on screen moves), then put the real visibility back. Same const_cast-free
        // reasoning as preferredHeight() — layout() only writes member scratch when applying.
        auto* self  = const_cast<Rack*> (this);
        auto  saved = self->layoutModel;
        for (auto& e : self->layoutModel)
            e.visible = true;
        const int h = self->layout (width, /*apply*/ false);
        self->layoutModel = std::move (saved);
        return h;
    }

    int Rack::layout (int width, bool apply)
    {
        if (apply)
        {
            zoneBands.clear();
            // Hide every zone-header control up-front; the loop below re-shows + positions the
            // controls of each zone that is actually rendered this pass.
            for (auto& zh : zoneHeaders)
            {
                if (zh.enableBtn) zh.enableBtn->setVisible (false);
                if (zh.resetBtn)  zh.resetBtn->setVisible (false);
                if (zh.infoBtn)   zh.infoBtn->setVisible (false);
            }
        }

        std::vector<ModuleFrame*> shownFrames;   // frames actually placed this pass (apply only)

        const int gridLeft  = kPad;
        const int gridWidth = juce::jmax (cols, width - 2 * kPad);   // guard tiny widths
        // Floor at 1: at a very small width the gutters can exceed gridWidth, making the raw cell
        // width negative (broken/negative bounds). 1px is a safe degenerate fallback.
        const int wc        = juce::jmax (1, (gridWidth - (cols - 1) * kGutter) / cols);   // Wc

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
            {
                ZoneBand band { zoneText (zone), zoneTag (zone),
                                { gridLeft, y, gridWidth, kZoneHeaderH } };
                band.lineStartX = gridLeft + kZoneLabelW;
                band.lineEndX   = gridLeft + gridWidth - 4;   // full width until the controls trim it

                // Standard controls sit at the RIGHT edge, same order/sizes as a module header
                // (far-right: enable 24 → reset 20 → info 20). The separator line runs between
                // the title and this cluster. Vertically centred in the header band.
                if (auto* zh = zoneHeaderFor (zone))
                {
                    const int cyc = y + kZoneHeaderH / 2;
                    const int bh  = 18, gap = 4;
                    int cx = gridLeft + gridWidth - 4;   // right edge (matches module header pad)

                    int enableable = 0, on = 0;
                    zoneEnableCounts (zone, enableable, on);

                    if (zh->enableBtn != nullptr && enableable > 0)
                    {
                        cx -= 24;
                        zh->enableBtn->setBounds (cx, cyc - bh / 2, 24, bh);
                        zh->enableBtn->setToggleState (on > 0, juce::dontSendNotification);
                        zh->enableBtn->setVisible (true);
                        cx -= gap;
                    }
                    if (zh->resetBtn != nullptr)
                    {
                        cx -= 20;
                        zh->resetBtn->setBounds (cx, cyc - bh / 2, 20, bh);
                        zh->resetBtn->setVisible (true);
                        cx -= gap;
                    }
                    if (zh->infoBtn != nullptr)
                    {
                        cx -= 20;
                        zh->infoBtn->setBounds (cx, cyc - bh / 2, 20, bh);
                        zh->infoBtn->setVisible (true);
                        cx -= gap;
                    }
                    band.lineEndX = cx - 2;   // separator stops just left of the cluster
                }

                zoneBands.push_back (band);
            }
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

            struct Placement { ModuleFrame* frame; int fc, fr, fcols, funits; bool alignRight; };
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
            // Within the zone, pack LEFT-aligned modules before RIGHT-aligned ones (stable, so
            // position order is preserved inside each group). This guarantees a left-aligned
            // module (PRESETS) claims the leftmost columns regardless of where the right-aligned
            // ones (STEREO/MASTER) sit in the position order. No-op for zones with no right group.
            std::stable_partition (entries.begin(), entries.end(),
                                   [] (const RackLayoutEntry* e) { return ! e->alignRight; });

            for (const auto* e : entries)
            {
                const auto* pl = placedById (e->id);
                if (pl == nullptr || pl->frame == nullptr) continue;
                // Clamp the footprint width to the rack width: a module wider than `cols` would make
                // the inner fit-loop's `c + fcols <= cols` never true, so `found` never flips and the
                // outer `for(;!found;++fr)` loops forever (growing rows unbounded). Placing it
                // full-width is the sane fallback for such a misconfiguration.
                const int fcols = juce::jmin (pl->cols, cols), funits = pl->units;

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
                    zonePlaced.push_back ({ pl->frame, fc, fr, fcols, funits, e->alignRight });

                maxRowUsed = juce::jmax (maxRowUsed, fr + funits - 1);
                maxColUsed = juce::jmax (maxColUsed, fc + fcols - 1);
            }

            if (apply)
            {
                // Per-module horizontal alignment (AD-10 follow-up): right-aligned modules are
                // shifted, as one block, to hug the right edge; left-aligned modules keep their
                // flush-left column. The MASTER BUS uses this to keep PRESETS on the left and
                // STEREO/MASTER/COMPRESSOR on the right (balancing the zone title). Zones with no
                // right-aligned module get shift 0 everywhere = the old flush-left packing.
                // Computed PER ROW (a zone can be multi-row): each row's right-aligned block is
                // shifted so its rightmost column lands on the last grid column. Exact for the
                // single-row master bus and correct for right-aligned modules in wider zones.
                std::vector<int> rightMaxCol ((size_t) juce::jmax (0, maxRowUsed + 1), -1);
                for (const auto& t : zonePlaced)
                    if (t.alignRight)
                        for (int rr = t.fr; rr < t.fr + t.funits && rr < (int) rightMaxCol.size(); ++rr)
                            rightMaxCol[(size_t) rr] = juce::jmax (rightMaxCol[(size_t) rr], t.fc + t.fcols - 1);

                for (const auto& t : zonePlaced)
                {
                    int colShift = 0;
                    if (t.alignRight)
                    {
                        int rmax = -1;   // tightest right col over the rows this frame occupies
                        for (int rr = t.fr; rr < t.fr + t.funits && rr < (int) rightMaxCol.size(); ++rr)
                            rmax = juce::jmax (rmax, rightMaxCol[(size_t) rr]);
                        if (rmax >= 0) colShift = cols - 1 - rmax;
                    }
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
        {
            for (auto* f : frames)
                f->setVisible (std::find (shownFrames.begin(), shownFrames.end(), f) != shownFrames.end());

            // The zone-header controls were created before any module frame (constructor vs.
            // addModule), so they sit BEHIND the frames in z-order. Pull the visible ones to the
            // front so a frame can never intercept a header button's click. (false = no focus.)
            for (auto& zh : zoneHeaders)
            {
                if (zh.enableBtn != nullptr && zh.enableBtn->isVisible()) zh.enableBtn->toFront (false);
                if (zh.resetBtn  != nullptr && zh.resetBtn->isVisible())  zh.resetBtn->toFront (false);
                if (zh.infoBtn   != nullptr && zh.infoBtn->isVisible())   zh.infoBtn->toFront (false);
            }
        }

        return y + kPad;
    }

    void Rack::resized()
    {
        layout (getWidth(), /*apply*/ true);
    }

    void Rack::updateLiveFeed (const LiveModFeed& ringByTarget, double playedRatio)
    {
        for (auto* f : frames)
            f->updateLiveFeed (ringByTarget, playedRatio);

        // Individual module enable toggles change the params behind our back; refresh the
        // zone-header group toggles from the live values on the same (editor) tick.
        syncZoneHeaderToggles();
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
            const auto b = z.bounds;
            const auto col = typeColour (z.tag);

            g.setColour (col);
            g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
            g.drawText (z.text, b.withWidth (kZoneLabelW), juce::Justification::centredLeft);

            // Separator line runs from just right of the title to just left of the right-edge
            // control cluster (enable/reset/info), so it never runs under any of them.
            const int lineX0 = juce::jmax (z.lineStartX, b.getX() + kZoneLabelW);
            const int lineX1 = z.lineEndX;
            if (lineX1 > lineX0 + 8)
            {
                const auto yMid = (float) b.getCentreY();
                g.setColour (col.withAlpha (0.35f));
                g.drawLine ((float) lineX0, yMid, (float) lineX1, yMid, 1.5f);
            }
        }
    }
}
