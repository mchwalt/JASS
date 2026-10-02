#pragma once
#include <JuceHeader.h>
#include <vector>
#include <utility>
#include <map>
#include <functional>
#include <algorithm>
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
        // Zone now lives in ModuleDescriptor.h (AD-10) so a descriptor can declare its own
        // default zone without an include-cycle. Kept spelled `Rack::Zone` for all callers.
        using Zone = rack::Zone;

        // The main rack is the default 8-column, three-zone grid. A narrower zone set /
        // column count builds a focused sub-rack (e.g. the top-right master-bus insert:
        // 1 column, just the MasterBus zone) — PROTOTYPE for the "everything is a module"
        // refinement (Stereo/Master as modules); to be formalised via correct-course.
        explicit Rack (juce::AudioProcessorValueTreeState& apvts,
                       int columns = kDefaultCols,
                       std::vector<Zone> zones = { Zone::Generators, Zone::Modulation, Zone::Processing });
        ~Rack() override;

        // Build + own a ModuleFrame for this descriptor. The zone comes from the descriptor's
        // `defaultZone` (AD-10); within-zone order is seeded from the call order. Placement is
        // then driven exclusively by the RackLayout model, not raw insertion order.
        void addModule (ModuleDescriptor desc);

        void resized() override;            // THE placement + zone-header layout site
        void paint (juce::Graphics&) override;

        // Fan the live feed out to every frame (AD-8): the single editor timer reads the
        // processor's LFO atomic + active target + played ratio and calls this once per
        // tick. Each frame gates rings by its own enable and refreshes its transform knobs.
        void updateLiveFeed (const LiveModFeed& ringByTarget, double playedRatio);

        // Find a module frame by its descriptor id (nullptr if none). Lets the editor reach a
        // specific module — e.g. so the spacebar can trigger STRING-KARPLUS' PLUCK button.
        ModuleFrame* moduleById (const juce::String& id);

        // --- Show / hide (Story 4.2, AD-10) -------------------------------------------
        // Mutate the RackLayout model, then re-pack via the single layout() path and notify
        // the editor (onLayoutChanged) so it can auto-fit the window height (AD-12). Hiding
        // is UI-only: the frame keeps its APVTS attachments + audio (it is just not placed).
        void setModuleVisible (const juce::String& id, bool visible);

        // Set a module's horizontal alignment within its zone row (MODULES panel L/R toggle).
        // Persisted like visibility/order; RESET restores the descriptor default.
        void setModuleAlignRight (const juce::String& id, bool alignRight);
        // Bulk convenience: set the visibility of ALL modules in a zone (one re-pack). There is
        // NO separate zone-visibility state — a zone's header is derived (shown iff the zone has
        // ≥1 visible module), so an emptied zone auto-disappears (AD-10 single source of truth).
        void setZoneVisible (Zone zone, bool visible);
        bool isModuleVisible (const juce::String& id) const;

        // Set the full module order + zone assignment from the customization list (Story 4.2):
        // `ordered` is every module id paired with its target zone, in the desired list order;
        // within-zone position becomes the running index. Visibility is unchanged.
        void applyLayoutOrder (const std::vector<std::pair<juce::String, Zone>>& ordered);

        // --- Persistence + reset (Story 4.3, AD-11) -----------------------------------
        // The custom layout persists as ONE string property on apvts.state (`kLayoutStateProp`,
        // JSON of layoutToVar). PresetIO mirrors it into the `.synthy` "RackLayout" field;
        // getStateInformation carries it in the DAW state for free. Default layout ⇒ no property.
        // Restore the DEFAULT layout — the player's own, from %AppData%\JASS\RackLayout.json
        // (maintainer 2026-10-03: which modules the stock rack shows must be editable without a
        // compiler); the descriptor flags only seed that file on first run. Touches NO knob. A
        // module that is ON stays visible whatever the file says ("on ⇒ visible" is the first
        // rule) — so to keep a module out of the stock rack, Init must leave it off (MOD MATRIX).
        void resetLayout();

        // --- The default layout as data (2026-10-03) ----------------------------------
        // The descriptor defaults captured at build are the SEED. The editor hands in the file's
        // entries (merged by id over the seed: a module the file does not know keeps its
        // descriptor default, an id the rack does not know is ignored), and asks for the full
        // list back to write the seed file. "Save as default" adopts the current rack as the
        // default and reports it, so the editor can write the file.
        juce::var defaultLayoutToVar() const;
        void setDefaultLayout (const juce::var& fileEntries);
        void adoptCurrentAsDefault();
        std::function<void(const juce::var&)> onDefaultLayoutChanged;

        // The header's RESET (maintainer's model, 2026-10-03): "RESET = every module's own ↺ at
        // once", and nothing else — the rack's layout is the MODULES menu's business. Runs each
        // frame's doReset (hidden ones too, so Init is Init), then reveals what is now switched
        // on (an Init oscillator a preset had hidden must come back, or Init would be silent) and
        // re-enforces hidden ⇒ silent. The caller sets the params to default first
        // (processor.resetToDefault) — this adds the per-module extras on top.
        void resetAllModules();
        // re-apply the persisted layout from apvts.state. `presetLoad` = an explicit preset load
        // (not the start-up LiveState restore): the rack then shows exactly the patch — modules
        // the preset leaves switched off are hidden (hideUnusedModules).
        void reloadLayoutFromState (bool presetLoad = false);

        // Invariant guard: a HIDDEN module must never be audible. Forces every hidden module's
        // enableParam off. Call after any path that may re-enable params under a stale layout
        // (header RESET/RANDOM, preset load) so nothing plays invisibly.
        void enforceHiddenDisabled();

        static constexpr const char* kLayoutStateProp = "rackLayout";

        // Menu data: the modules of a zone in placement order, with title + visibility.
        struct ModuleInfo { juce::String id, title; bool visible; bool alignRight; };
        std::vector<ModuleInfo> modulesInZone (Zone zone) const;
        const std::vector<Zone>& zones() const noexcept { return zoneOrder; }
        static juce::String zoneName (Zone zone);
        // Help-resource slug for a zone's info icon (e.g. Generators -> "zone-generators").
        static juce::String zoneHelpId (Zone zone);

        // --- Zone-header standard controls (mirror the per-module enable/reset/info) -----
        // ENABLE = group bypass: toggle the enable param of every VISIBLE enable-capable
        // module in the zone (they stay visible, just dimmed). Does NOT touch visibility, so
        // it is distinct from the MODULES-panel show/hide. Hidden modules are left off
        // (invariant: a hidden module is never audible).
        void setZoneEnabled (Zone zone, bool enabled);
        // RESET = restore this group's DEFAULT module selection: every module whose factory
        // zone is `zone` gets its default {zone, position, visible} back (re-packs, persists,
        // enforces hidden⇒silent). Does not change any audio param value.
        void resetZone (Zone zone);

        // Called when a zone header's info icon is clicked (carries the zone) so the editor
        // can show the shared HelpPanel with the group's help text.
        std::function<void(Zone)> onZoneHelp;
        // Keep the zone-header enable toggles in sync with the live enable params (called by
        // the editor's timer, since individual module toggles change the params behind us).
        void syncZoneHeaderToggles();
        // The zone's info icon component (anchor for the help panel; nullptr if that zone has
        // no help). Lets the editor place the shared HelpPanel right next to the clicked icon.
        juce::Component* zoneInfoButton (Zone zone);

        // Override the FACTORY enable value for one enable-param, for those params whose raw
        // APVTS default does NOT equal the shipped Init state — currently only oscOn(1..3),
        // which resetToDefault() turns on explicitly (their declared default is 0 for preset
        // compatibility). resetZone() uses these when restoring a group's default enable state
        // so it reproduces the factory patch instead of silencing everything.
        void setFactoryEnableDefault (const juce::String& enableParamId, float normValue);

        // Called after any layout mutation (show/hide) so the editor can re-fit height.
        std::function<void()> onLayoutChanged;

        // Called when a module's header info icon is clicked (Story 6.1); carries the module
        // id so the editor can resolve title + help text and show the shared HelpPanel.
        std::function<void(const juce::String& id)> onModuleHelp;

        // Total stacked height the current population needs at `width` — lets the
        // editor size/verify the fixed window without scrolling (AC4).
        int preferredHeight (int width) const;

        // Worst-case height: what the rack would need at `width` with EVERY module visible.
        // The editor derives its ONE display-fit scale from this (AD-12), so revealing modules
        // — e.g. loading a preset that enables more of them — never resizes the window.
        // Pure measurement: the layout model is restored before returning.
        int maxHeight (int width) const;

        // Does this module only draw (see ModuleDescriptor::visualOnly)? Such a module may be
        // hidden for good: it is exempt from the worst-case measurement and from auto-reveal.
        bool isVisualOnly (const juce::String& id) const;

        // --- fixed grid constants (frozen from the mockup; AC4) ---
        static constexpr int kDefaultCols = 30;   // fine proportional grid (12 → 24 → 30); a pure
                                                  // layout raster, independent of knob size.
                                                  // Story 7.3 widened 24 → 30 together with the
                                                  // design width 1520 → 1920: that keeps the column
                                                  // at ~53 px, so every module keeps its physical
                                                  // size, but six more columns per row pack the
                                                  // rack two rows shorter. Height is the scarce
                                                  // dimension (the display-fit scale was at its
                                                  // readable floor); width was idle. MEASURED:
                                                  // 1980 px → 1608 px, fit scale 0.65 → 0.79.
        static constexpr int kGutter      = 10;   // uniform gutter between cells
        static constexpr int kHu          = 21;   // one QUARTER rack unit (Story 7.4). The raster was
                                                  // a whole 114 px row — sized so a 1-unit body fits
                                                  // name caption + 46 px knob + value box — which made
                                                  // every height a multiple of it: a module needing a
                                                  // little more than one row had to take two, and the
                                                  // second row arrived with the header height built in
                                                  // again. n quarters are n*21 + (n-1)*10 px, so 4 is
                                                  // still exactly 114 and 8 exactly 238 (nothing moved),
                                                  // while 5 = 145 and 6 = 176 exist now.
        static constexpr int kZoneHeaderH = 28;   // full-width zone separator band
        static constexpr int kZoneLabelW  = 172;  // reserved width for the zone title before its
                                                  // header controls (>= widest label at 15pt bold)
        static constexpr int kPad         = 8;    // inner padding around the grid

        // Fan ModuleFrame::refreshNamedReadouts out to every frame: value boxes that depend on
        // EDITOR state (STEP SEQ note names follow the keyboard octave) re-text in one call.
        void refreshNamedReadouts() { for (auto* f : frames) f->refreshNamedReadouts(); }

    private:
        struct Placed
        {
            juce::String id;           // stable module id (RackLayout key, AD-10)
            ModuleFrame* frame = nullptr;
            int cols = 1, units = 1;   // footprint from the size class
            bool alignRight = false;   // pack right within the zone row (from the descriptor)
        };
        // AD-10: the ordered, editable placement model — the single source of truth for
        // WHERE each module sits. `layout()` walks this (per zone, by position, visible only)
        // instead of raw insertion order. In Story 4.1 `visible` is always true (show/hide is
        // Story 4.3) and persistence is Story 4.2 — this is the plumbing those build on.
        struct RackLayoutEntry
        {
            juce::String id;
            Zone zone {};
            int  position = 0;      // within-zone order
            bool visible  = true;
            bool alignRight = false;// pack right within the zone row (user-editable; persisted)
        };
        struct ZoneBand { juce::String text; ModuleType tag; juce::Rectangle<int> bounds;
                          int lineStartX = 0; int lineEndX = 0; };

        // One set of standard controls per zone header (built once for every zone in
        // zoneOrder). enable = group bypass (manual toggle, no APVTS attachment — its state
        // is derived from the members and refreshed by syncZoneHeaderToggles); reset = restore
        // the group's default module selection; info = zone help (created only when a help
        // resource exists for the zone). Positioned/hidden by layout() per shown zone.
        struct ZoneHeaderControls
        {
            Zone zone {};
            std::unique_ptr<juce::ToggleButton> enableBtn;
            std::unique_ptr<IconButton>         resetBtn;
            std::unique_ptr<IconButton>         infoBtn;
            // "Mute with memory": when the group is bypassed we stash each member's enable value
            // here, so re-enabling restores exactly the members that were on (empty => not
            // bypassed / nothing remembered; enable then falls back to turning the group on).
            std::map<juce::String, float> bypassSnapshot;
        };
        std::vector<ZoneHeaderControls> zoneHeaders;   // one per zone in zoneOrder
        std::map<juce::String, float> factoryEnableByParam;   // enable-param id -> factory (Init) value
        void buildZoneHeaders();                       // create the controls (constructor)
        ZoneHeaderControls* zoneHeaderFor (Zone zone); // lookup (nullptr if none)
        // Count VISIBLE enable-capable modules in a zone and how many are currently ON.
        void zoneEnableCounts (Zone zone, int& enableable, int& on) const;

        // footprint/frame lookup by id (nullptr if unknown)
        const Placed* placedById (const juce::String& id) const;

        // The ONE packing path (row-major first-fit, never overlaps). When apply is
        // true it sets frame bounds + zone-header bands; either way it returns the
        // total height consumed. Isolated here so it can later be swapped for a
        // different strategy (e.g. dense-fill) without touching the call sites.
        int layout (int width, bool apply);

        juce::AudioProcessorValueTreeState& apvts;
        const int cols;                            // columns this rack instance packs into
        std::vector<Zone> zoneOrder;               // zones rendered, top→bottom
        SynthyLookAndFeel lnf;                     // the single shared look (AD-7)
        // Re-pack in place (reads the model) + repaint; used after a visibility change.
        void relayout();

        // Couple visibility → enable on an INTERACTIVE toggle (Story 4.2): hide ⇒ disable,
        // show ⇒ restore the module's FACTORY-DEFAULT enable state (not force-on — e.g. CROSS MOD
        // defaults to disabled). Writes the module's enableParam if it has one. NOT used on
        // load/reset (there visibility + enables are restored independently).
        void driveEnable (const juce::String& id, bool show);

        // Preset-load reconciliation (complement of enforceHiddenDisabled): a module the loaded
        // preset left ENABLED must be visible — you can't play through a module you can't see, and
        // a preset that uses a default-hidden module (e.g. COMPRESSOR) has no custom layout. Marks
        // such modules visible WITHOUT touching their enable. Returns true if anything changed.
        bool revealEnabledModules();

        // The other direction, on an explicit preset load only (maintainer 2026-10-02): a module
        // the preset leaves switched OFF is hidden, so the rack shows the patch and nothing else.
        // Exempt: the MASTER BUS and INPUT zones (infrastructure, not patch) and visual-only
        // displays (whether the scope is up is the player's call, no preset's). Returns true if
        // anything changed.
        bool hideUnusedModules();

        // --- Layout persistence helpers (Story 4.3) ---
        // One item of persisted layout data in either shape (grouped object or legacy array);
        // a void field means "not in the data" and leaves the entry's current value alone.
        struct LayoutItem { juce::String id, zone; juce::var pos, vis, alignR; };
        juce::var entriesToVar (const std::vector<RackLayoutEntry>& entries) const;   // → {"ZONE": [{id,vis,alignR}, …], …} in render order
        static void forEachLayoutItem (const juce::var& v, const std::function<void (const LayoutItem&)>& fn);
        static void mergeLayoutItem (std::vector<RackLayoutEntry>& into, const LayoutItem& it);   // by id; unknown ids ignored
        juce::var layoutToVar() const;                  // model → JSON var
        void applyLayoutVar (const juce::var& v);       // JSON var → model (by id), then re-pack (no enable coupling)
        bool isDefaultLayout() const;                   // model == captured defaults?
        void writeLayoutToState();                      // persist to apvts.state (or clear when default)
        static Zone zoneFromName (const juce::String& name);   // inverse of zoneName

        juce::OwnedArray<ModuleFrame> frames;
        std::vector<Placed> placed;                // id -> frame + footprint (build order)
        std::vector<RackLayoutEntry> layoutModel;  // AD-10 single source of truth for placement
        std::vector<RackLayoutEntry> defaultLayout;// descriptor defaults, captured at build (for reset + isDefault)
        std::vector<ZoneBand> zoneBands;           // computed in layout(), drawn in paint()

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Rack)
    };
}
