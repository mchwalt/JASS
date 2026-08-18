#include "PluginEditor.h"
#include "../DSP/WavetableBank.h"
#include "HelpTextStore.h"           // embedded EN/DE help texts (Story 6.1)
#include "../Audio/PresetIO.h"
#include "../Audio/Parameters.h"   // Parameters::ID for the rack
#include "../Version.h"            // JASS::versionString() (CalVer)
#include "../Modules/AllModules.h"  // spec-driven module descriptors (makeModuleDescriptor + Modules::*)
#include <map>
#include <memory>
#include <vector>
#include <utility>
#include <array>
#include <cmath>
#include <algorithm>

// SynthyLookAndFeel now lives in Source/UI/rack/SynthyLookAndFeel.{h,cpp} (AD-7) —
// the rack framework owns the single shared look.

// --- Editor geometry (shared by refitHeight and the MODULES panel's budget line) -------------
// The design width went 1520 → 1920 with Story 7.3, hand in hand with the 24 → 30 column grid:
// together they keep the column at ~53 px, so no module changed physical size — the rack simply
// packs two rows shorter. Height is the scarce dimension (the fit scale was at its readable floor);
// width was idle, 1520 designed against 3413 available.
namespace EditorGeom
{
    constexpr int kDesignW    = 1920;
    constexpr int kBodyTop    = 72;   // header row + gap (matches resized())
    constexpr int kBodyBottom = 0;    // no reserved band — the keyboard is a rack module now
    constexpr int kMargin     = 12;   // getLocalBounds().reduced(12)
    constexpr double kChrome  = 90.0; // title bar + a little breathing room

    // Rack height that still fits on this display at `scale`, i.e. the budget the MODULES panel
    // reports against. Inverse of the sH computation in refitHeight.
    inline int affordableRackHeight (double scale, double screenH)
    {
        return (int) ((screenH - kChrome) / juce::jmax (0.01, scale)) - kBodyTop - kBodyBottom - 2 * kMargin;
    }

    // The display a given on-screen rectangle sits on — NOT the primary one. Both the fit scale and
    // the MODULES budget line asked for getPrimaryDisplay(), which is only the same thing on a
    // single-monitor desk. On the maintainer's (JASS on a 5120x2160 at 150 %, a smaller primary
    // elsewhere) the budget line reported a screen the window had never been on: 1732 px needed
    // against 1020 px "available", while the window sat comfortably on a screen with room to spare.
    // Falls back to the primary display before the editor has a peer (bounds still empty).
    inline const juce::Displays::Display* displayFor (juce::Rectangle<int> screenArea)
    {
        const auto& displays = juce::Desktop::getInstance().getDisplays();
        if (! screenArea.isEmpty())
            if (auto* d = displays.getDisplayForRect (screenArea))
                return d;
        return displays.getPrimaryDisplay();
    }
}

// --- Rack customization panel (Story 4.2) -----------------------------------
// A reorderable list of every module, grouped by zone. A left checkbox toggles
// visibility; dragging a module row reorders it (list order = on-screen order) and
// dragging it across a zone header moves it to that zone. All mutations go through the
// Rack API (setModuleVisible / setZoneVisible / applyLayoutOrder) — the single layout()
// path re-packs and the editor re-fits height (AD-10/AD-12). Shown in a CallOutBox.
namespace
{
    class RackCustomizePanel : public juce::Component,
                               private juce::Timer
    {
    public:
        explicit RackCustomizePanel (rack::Rack& r, const juce::String& lang = "EN")
            : rack (r), isDE (lang == "DE")
        {
            rebuildRows();
            addAndMakeVisible (resetBtn);
            resetBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff334155));
            resetBtn.onClick = [this] { rack.resetLayout(); rebuildRows(); repaint(); };
            setSize (kW, listHeight() + kBudgetH + kBtnH);

            // Discoverability toast (localized): drag-to-reorder isn't obvious, so after the
            // mouse rests a moment over the list we fade in a one-line hint, then fade it out.
            toastText = (lang == "DE")
                ? juce::String (juce::CharPointer_UTF8 ("Tipp: Reihenfolge per Drag & Drop ändern - einfach eine Modulzeile ziehen."))
                : juce::String ("Tip: reorder modules by drag & drop - just drag a row.");
            lastMoveMs = juce::Time::getMillisecondCounter();
            startTimerHz (30);
        }

        ~RackCustomizePanel() override { stopTimer(); }

        void resized() override
        {
            resetBtn.setBounds (juce::Rectangle<int> (0, listHeight() + kBudgetH, getWidth(), kBtnH)
                                    .reduced (6, 5));
        }

        int listHeight() const { return juce::jmax (kRowH, (int) rows.size() * kRowH); }

        // Story 7.3: rack height is a BUDGET, not something the fit scale can keep absorbing —
        // it already sits at its readable floor. Show what the current selection costs, so hiding
        // or revealing a module is a visible trade instead of type that silently gets smaller.
        // Returns { text, detail, overBudget } — the detail line names the DISPLAY the numbers were
        // measured on. Without it the line is a riddle when it disagrees with what one sees, and it
        // did disagree: it used to measure the primary display rather than this one.
        struct Budget { juce::String text, detail; bool over = false; };
        Budget budgetLine() const
        {
            using namespace EditorGeom;
            auto* disp = displayFor (getScreenBounds());
            if (disp == nullptr)
                return {};
            const auto ua = disp->userBounds;
            const double minScale = juce::jlimit (0.5, 1.0, 1.0 / juce::jmax (1.0, (double) disp->scale));
            const int need   = rack.maxHeight (kDesignW - 2 * kMargin);
            const int budget = affordableRackHeight (minScale, (double) ua.getHeight());
            const double worstH = (double) juce::jmax (1015, need + kBodyTop + kBodyBottom + 2 * kMargin);
            const double scale  = juce::jlimit (minScale, 1.0,
                                                juce::jmin ((ua.getHeight() - kChrome) / worstH,
                                                            ua.getWidth() / (double) kDesignW));
            const bool over = need > budget;
            // Both separators must be declared UTF-8. juce::String(const char*) takes plain ASCII,
            // so the raw '·' and '—' in the English literals arrived as mojibake ("Ä") while the
            // German ones, already wrapped, were fine (maintainer 2026-08-10: "Sonderzeichen!").
            juce::String t;
            t << "Rack " << need << " / " << budget << " px"
              << juce::String (juce::CharPointer_UTF8 (isDE ? " · Anzeige " : " · scale "))
              << juce::String (scale, 2);
            if (over)
                t << juce::String (juce::CharPointer_UTF8 (isDE ? " — zu hoch!" : " — over budget!"));

            // Second line: which screen this was measured on. The warning above had to shrink to
            // fit the 300 px panel, so the advice moves down here where there is room for it.
            juce::String d;
            d << juce::roundToInt (ua.getWidth()) << juce::String (juce::CharPointer_UTF8 (" × "))
              << juce::roundToInt (ua.getHeight())
              << " @ " << juce::String (disp->scale * 100.0, 0) << " %";
            if (over)
                d << juce::String (juce::CharPointer_UTF8 (isDE ? " · Modul ausblenden"
                                                                : " · hide a module"));
            return { t, d, over };
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour (0xff1b1f26));
            for (int i = 0; i < (int) rows.size(); ++i)
            {
                auto rb  = juce::Rectangle<int> (0, i * kRowH, getWidth(), kRowH);
                const auto& row = rows[(size_t) i];
                if (i == dragIndex)
                {
                    g.setColour (juce::Colour (0xff2b3340));
                    g.fillRect (rb);
                }
                auto box = rb.removeFromLeft (22);
                if (row.header)
                {
                    int vis = 0, total = 0; zoneCounts (row.zone, vis, total);
                    const int state = (vis == 0 ? 0 : (vis == total ? 1 : 2));   // empty / all / mixed
                    drawBox (g, box, state, juce::Colour (0xff8fb0c8));
                    g.setColour (juce::Colour (0xff8fb0c8));
                    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
                    g.drawText (rack::Rack::zoneName (row.zone), rb.reduced (4, 0),
                                juce::Justification::centredLeft);
                }
                else
                {
                    drawBox (g, box, row.visible ? 1 : 0, juce::Colours::white);
                    // Right cluster (carved right→left): drag hint, then the R and L align tags.
                    auto drag = rb.removeFromRight (18);
                    auto rBox = rb.removeFromRight (18);
                    auto lBox = rb.removeFromRight (18);
                    g.setColour (juce::Colours::white.withAlpha (row.visible ? 0.9f : 0.4f));
                    g.setFont (juce::FontOptions (13.0f));
                    g.drawText (row.title, rb.reduced (12, 0), juce::Justification::centredLeft);
                    drawAlignTag (g, lBox, "L", ! row.alignRight);   // left-aligned (default)
                    drawAlignTag (g, rBox, "R",   row.alignRight);   // right-aligned
                    g.setColour (juce::Colours::white.withAlpha (0.22f));
                    g.drawText ("::", drag, juce::Justification::centred);   // drag hint
                }
                g.setColour (juce::Colours::black.withAlpha (0.30f));
                g.drawHorizontalLine (i * kRowH, 0.0f, (float) getWidth());
            }

            // Height budget band, between the list and the RESET button.
            {
                auto band = juce::Rectangle<int> (0, listHeight(), getWidth(), kBudgetH);
                g.setColour (juce::Colour (0xff11141a));
                g.fillRect (band);
                const auto b = budgetLine();
                auto top = band.removeFromTop (kBudgetH / 2);
                g.setColour (b.over ? juce::Colour (0xffe0b050) : juce::Colours::white.withAlpha (0.55f));
                g.setFont (juce::FontOptions (12.0f));
                g.drawText (b.text, top.reduced (8, 0), juce::Justification::centredLeft);
                g.setColour (juce::Colours::white.withAlpha (b.over ? 0.55f : 0.35f));
                g.setFont (juce::FontOptions (11.0f));
                g.drawText (b.detail, band.reduced (8, 0), juce::Justification::centredLeft);
            }

            paintToast (g);   // drag-to-reorder hint (fades in after a rest, then out)
        }

        void mouseEnter (const juce::MouseEvent& e) override { mouseInside = true; noteActivity (e); }
        void mouseExit  (const juce::MouseEvent&)     override { mouseInside = false; }
        void mouseMove  (const juce::MouseEvent& e) override { noteActivity (e); }

        void mouseDown (const juce::MouseEvent& e) override
        {
            noteActivity (e);
            dragIndex = -1;
            const int i = e.y / kRowH;
            if (! juce::isPositiveAndBelow (i, (int) rows.size())) return;
            auto& row = rows[(size_t) i];
            if (e.x < 22)   // checkbox column → toggle visibility
            {
                if (row.header)
                {
                    // Bidirectional bulk: all-on → all-off; otherwise (none/mixed) → all-on.
                    int vis = 0, total = 0; zoneCounts (row.zone, vis, total);
                    const bool target = ! (total > 0 && vis == total);
                    rack.setZoneVisible (row.zone, target);
                    for (auto& r : rows) if (! r.header && r.zone == row.zone) r.visible = target;
                }
                else { row.visible = ! row.visible; rack.setModuleVisible (row.id, row.visible); }
                repaint();
                return;
            }
            if (! row.header)   // L / R alignment tags on the right of a module row
            {
                const int w = getWidth();
                if (e.x >= w - 36 && e.x < w - 18)        // R tag
                { row.alignRight = true;  rack.setModuleAlignRight (row.id, true);  repaint(); return; }
                if (e.x >= w - 54 && e.x < w - 36)        // L tag
                { row.alignRight = false; rack.setModuleAlignRight (row.id, false); repaint(); return; }
            }
            if (! row.header) dragIndex = i;   // only module rows are draggable
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            // The user is already doing exactly what the hint teaches — dismiss it.
            if (toastActive) { toastActive = false; toastAlpha = 0.0f; repaint(); }
            noteActivity (e);
            if (dragIndex < 0) return;
            const int target = juce::jlimit (1, (int) rows.size() - 1, e.y / kRowH);
            if (target != dragIndex)
            {
                auto row = rows[(size_t) dragIndex];
                rows.erase (rows.begin() + dragIndex);
                rows.insert (rows.begin() + target, row);
                dragIndex = target;
                repaint();
            }
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            if (dragIndex < 0) return;
            dragIndex = -1;
            commitOrder();
        }

    private:
        struct Row { bool header; rack::Rack::Zone zone; juce::String id, title; bool visible; bool alignRight; };

        // --- drag-to-reorder hint toast -------------------------------------------------
        // Any mouse activity resets the rest timer and re-arms the toast (so it can show again
        // after the next pause). lastMovePos is remembered so the banner appears near the cursor.
        void noteActivity (const juce::MouseEvent& e)
        {
            lastMoveMs   = juce::Time::getMillisecondCounter();
            lastMoveY    = e.y;
            hoverConsumed = false;
        }

        void timerCallback() override
        {
            const auto now = juce::Time::getMillisecondCounter();

            // Show once per rest: after kRestMs of stillness while the mouse is over the list.
            if (mouseInside && ! toastActive && ! hoverConsumed && (now - lastMoveMs) >= kRestMs)
            {
                toastActive   = true;
                toastStartMs  = now;
                hoverConsumed = true;   // don't re-show until the mouse moves again
            }

            if (! toastActive) return;

            // Fade in → hold → fade out, then retire.
            const auto el = now - toastStartMs;
            float a;
            if      (el < kFadeMs)                 a = (float) el / (float) kFadeMs;
            else if (el < kFadeMs + kHoldMs)       a = 1.0f;
            else if (el < 2 * kFadeMs + kHoldMs)   a = 1.0f - (float) (el - kFadeMs - kHoldMs) / (float) kFadeMs;
            else                                   { toastActive = false; a = 0.0f; }

            if (std::abs (a - toastAlpha) > 0.02f || ! toastActive)
            {
                toastAlpha = a;
                repaint();
            }
        }

        void paintToast (juce::Graphics& g)
        {
            if (toastAlpha <= 0.01f)
                return;

            const int margin = 8, bannerH = 46;
            const int maxY = juce::jmax (4, listHeight() - bannerH - 4);
            const int y    = juce::jlimit (4, maxY, lastMoveY + 14);
            auto area = juce::Rectangle<int> (margin, y, getWidth() - 2 * margin, bannerH).toFloat();

            g.setColour (juce::Colour (0xff0f1420).withAlpha (0.94f * toastAlpha));
            g.fillRoundedRectangle (area, 7.0f);
            g.setColour (juce::Colour (0xff40c0ff).withAlpha (0.85f * toastAlpha));
            g.drawRoundedRectangle (area, 7.0f, 1.4f);

            g.setColour (juce::Colours::white.withAlpha (0.92f * toastAlpha));
            g.setFont (juce::FontOptions (12.5f));
            g.drawFittedText (toastText, area.toNearestInt().reduced (10, 6),
                              juce::Justification::centredLeft, 2);
        }

        // A small L / R alignment tag: filled/blue when active, faint outline when not.
        static void drawAlignTag (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& t, bool active)
        {
            auto b = area.withSizeKeepingCentre (16, 16).toFloat();
            if (active) { g.setColour (juce::Colour (0xff4aa3ff)); g.fillRoundedRectangle (b, 3.0f); }
            else        { g.setColour (juce::Colours::white.withAlpha (0.20f)); g.drawRoundedRectangle (b, 3.0f, 1.0f); }
            g.setColour (active ? juce::Colours::white : juce::Colours::white.withAlpha (0.55f));
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            g.drawText (t, area, juce::Justification::centred);
        }

        // state: 0 = empty, 1 = full, 2 = partial (mixed — a thin dash)
        static void drawBox (juce::Graphics& g, juce::Rectangle<int> area, int state, juce::Colour c)
        {
            auto b = area.withSizeKeepingCentre (12, 12);
            g.setColour (c.withAlpha (0.6f));
            g.drawRect (b, 1);
            if (state == 1)      { g.setColour (c); g.fillRect (b.reduced (3)); }
            else if (state == 2) { g.setColour (c); g.fillRect (b.reduced (3, 5)); }
        }

        void zoneCounts (rack::Rack::Zone z, int& vis, int& total) const
        {
            vis = 0; total = 0;
            for (const auto& r : rows) if (! r.header && r.zone == z) { ++total; if (r.visible) ++vis; }
        }

        void rebuildRows()
        {
            rows.clear();
            for (auto z : rack.zones())
            {
                rows.push_back ({ true, z, {}, {}, true, false });
                for (const auto& m : rack.modulesInZone (z))
                    rows.push_back ({ false, z, m.id, m.title, m.visible, m.alignRight });
            }
        }

        void commitOrder()
        {
            // The zone of each module row = the most recent header above it; the row order
            // drives Rack::applyLayoutOrder. Also fix each row's cached zone for future drags.
            std::vector<std::pair<juce::String, rack::Rack::Zone>> ordered;
            rack::Rack::Zone cur {};
            for (auto& r : rows)
            {
                if (r.header) cur = r.zone;
                else { r.zone = cur; ordered.push_back ({ r.id, cur }); }
            }
            rack.applyLayoutOrder (ordered);
        }

        rack::Rack& rack;
        const bool  isDE = false;   // budget-line language (the panel is built per language)
        std::vector<Row> rows;
        juce::TextButton resetBtn { "Reset layout" };
        int dragIndex = -1;

        // toast state
        juce::String  toastText;
        juce::uint32  lastMoveMs   = 0;
        juce::uint32  toastStartMs = 0;
        int           lastMoveY    = 0;
        float         toastAlpha   = 0.0f;
        bool          toastActive  = false;
        bool          hoverConsumed = false;
        bool          mouseInside  = false;

        static constexpr int kW = 300, kRowH = 26, kBtnH = 30;   // +40 vs. before for the L/R align tags
        static constexpr int kBudgetH = 34;   // Story 7.3: the rack-height budget line — two rows
                                              // since the display it measured is named underneath
        static constexpr juce::uint32 kRestMs = 3000;   // rest time before the hint appears
        static constexpr juce::uint32 kFadeMs =  300;   // fade in / out duration
        static constexpr juce::uint32 kHoldMs = 4300;   // fully-visible hold (=> ~4.9 s total)
    };
}

// --- EnvelopeDisplay ---

void EnvelopeDisplay::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(2.0f);
    const float left = b.getX(), right = b.getRight();
    const float top = b.getY(), bottom = b.getBottom();
    const float w = b.getWidth(), h = b.getHeight();

    const float a = pA ? pA->load() : 0.0f;
    const float d = pD ? pD->load() : 0.0f;
    const float s = juce::jlimit(0.0f, 1.0f, pS ? pS->load() : 0.0f);
    const float r = pR ? pR->load() : 0.0f;

    // A fixed-width sustain hold; the rest of the width is split between A/D/R
    // proportional to their durations (so the SHAPE always fills the strip).
    const float sustainW = w * 0.22f;
    const float adrW = w - sustainW;
    const float sum = a + d + r;
    const float aw = (sum < 1.0e-4f) ? adrW / 3.0f : adrW * (a / sum);
    const float dw = (sum < 1.0e-4f) ? adrW / 3.0f : adrW * (d / sum);
    const float rw = (sum < 1.0e-4f) ? adrW / 3.0f : adrW * (r / sum);

    const float susY = bottom - s * h;
    const float xPeak = left + aw;
    const float xSusStart = xPeak + dw;
    const float xSusEnd = xSusStart + sustainW;
    const float xEnd = xSusEnd + rw;

    juce::Path curve;
    curve.startNewSubPath(left, bottom);   // note-on at zero
    curve.lineTo(xPeak, top);              // attack → peak
    curve.lineTo(xSusStart, susY);         // decay → sustain level
    curve.lineTo(xSusEnd, susY);           // sustain hold
    curve.lineTo(xEnd, bottom);            // release → zero

    // Soft fill under the curve, then the stroked line on top.
    juce::Path fill = curve;
    fill.lineTo(left, bottom);
    fill.closeSubPath();
    g.setColour(col.withAlpha(0.14f));
    g.fillPath(fill);

    g.setColour(col.withAlpha(0.9f));
    g.strokePath(curve, juce::PathStrokeType(1.6f));

    // Baseline.
    g.setColour(col.withAlpha(0.25f));
    g.drawLine(left, bottom, right, bottom, 1.0f);
}

// --- SynthyEditor ---

namespace
{
    // Left-aligns the JUCE-standalone title-bar text (right of the "Options" button, which the
    // wrapper puts at x=8, width 60) so the whole strip reads as JUCE's standalone chrome — not
    // part of the JASS editor below it. Only the title text is customised; the min/close buttons
    // keep the default look. Applied to the top-level DocumentWindow in the standalone only.
    struct StandaloneTitleLnF : juce::LookAndFeel_V4
    {
        void drawDocumentWindowTitleBar (juce::DocumentWindow& window, juce::Graphics& g,
                                         int w, int h, int titleSpaceX, int titleSpaceW,
                                         const juce::Image*, bool) override
        {
            if (w * h == 0) return;
            g.fillAll (getCurrentColourScheme().getUIColour (ColourScheme::widgetBackground));
            g.setColour (juce::Colours::white.withAlpha (window.isActiveWindow() ? 0.75f : 0.4f));
            g.setFont (juce::FontOptions ((float) h * 0.6f, juce::Font::bold));
            const int textX = juce::jmax (titleSpaceX, 76);                 // clear of the Options button
            const int textW = juce::jmax (0, (titleSpaceX + titleSpaceW) - textX);
            g.drawText (window.getName(), textX, 0, textW, h, juce::Justification::centredLeft, true);
        }
    };
}

SynthyEditor::SynthyEditor(SynthyProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lnf);

    // Preset Save / Load (JASS .jass JSON)
    addAndMakeVisible(saveBtn);
    addAndMakeVisible(loadBtn);
    saveBtn.onClick = [this]
    {
        presetChooser = std::make_unique<juce::FileChooser>(
            "Save preset", PresetIO::presetsFolder(), "*.jass");
        auto flags = juce::FileBrowserComponent::saveMode
                   | juce::FileBrowserComponent::canSelectFiles
                   | juce::FileBrowserComponent::warnAboutOverwriting;
        juce::Component::SafePointer<SynthyEditor> self (this);
        presetChooser->launchAsync(flags, [self](const juce::FileChooser& fc)
        {
            if (self == nullptr) return;   // editor closed while the dialog was open
            auto f = fc.getResult();
            if (f == juce::File{}) return;
            if (! f.hasFileExtension("jass")) f = f.withFileExtension("jass");
            PresetIO::saveToFile(self->processor.getAPVTS(), f, f.getFileNameWithoutExtension());
            self->processor.markPresetClean();   // current state now matches the saved file
            self->setPresetName(f.getFileNameWithoutExtension());
        });
    };
    loadBtn.onClick = [this]
    {
        presetChooser = std::make_unique<juce::FileChooser>(
            "Load preset", PresetIO::presetsFolder(), "*.jass");
        auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        juce::Component::SafePointer<SynthyEditor> self (this);
        presetChooser->launchAsync(flags, [self](const juce::FileChooser& fc)
        {
            if (self == nullptr) return;
            auto f = fc.getResult();
            if (f == juce::File{}) return;
            self->loadPresetFile(f);
        });
    };

    // DELETE removes a preset FILE — the counterpart the SAVE/LOAD pair was missing (maintainer
    // 2026-08-18: "keine Möglichkeit, Presets über die GUI zu löschen"). Same chooser as LOAD,
    // restricted to the Presets folder; the file goes to the RECYCLE BIN (moveToTrash), never
    // hard-deleted, after an explicit confirmation. Any F-key assignment of the deleted preset
    // is cleared along with it — a dead slot that errors on every press helps nobody.
    addAndMakeVisible(deleteBtn);
    deleteBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff334155));
    deleteBtn.onClick = [this]
    {
        presetChooser = std::make_unique<juce::FileChooser>(
            currentLang == "DE" ? "Preset l\xc3\xb6schen" : "Delete preset",
            PresetIO::presetsFolder(), "*.jass");
        auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        juce::Component::SafePointer<SynthyEditor> self (this);
        presetChooser->launchAsync(flags, [self](const juce::FileChooser& fc)
        {
            if (self == nullptr) return;
            auto f = fc.getResult();
            if (f == juce::File{}) return;
            if (f.getParentDirectory() != PresetIO::presetsFolder())
            {
                juce::NativeMessageBox::showMessageBoxAsync(
                    juce::MessageBoxIconType::InfoIcon,
                    self->currentLang == "DE" ? "Ordner" : "Folder",
                    (self->currentLang == "DE"
                        ? juce::String("Bitte ein Preset aus dem Presets-Ordner w\xc3\xa4hlen.")
                        : juce::String("Please choose a preset from the Presets folder.")));
                return;
            }
            const auto name = f.getFileNameWithoutExtension();
            juce::NativeMessageBox::showOkCancelBox(
                juce::MessageBoxIconType::WarningIcon,
                self->currentLang == "DE" ? "Preset l\xc3\xb6schen" : "Delete preset",
                (self->currentLang == "DE"
                    ? "\xe2\x80\x9e" + name + "\xe2\x80\x9c in den Papierkorb verschieben?"
                    : "Move \"" + name + "\" to the recycle bin?"),
                nullptr,
                juce::ModalCallbackFunction::create([self, f, name](int result)
                {
                    if (self == nullptr || result == 0) return;   // 0 = cancel
                    if (! f.moveToTrash())
                    {
                        juce::NativeMessageBox::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon,
                            self->currentLang == "DE" ? "L\xc3\xb6schen fehlgeschlagen" : "Delete failed",
                            (self->currentLang == "DE"
                                ? "\xe2\x80\x9e" + name + "\xe2\x80\x9c konnte nicht gel\xc3\xb6scht werden."
                                : "\"" + name + "\" could not be deleted."));
                        return;
                    }
                    self->clearBankSlotsFor(name);   // no F-key may keep pointing at a gone file
                }));
        });
    };

    addAndMakeVisible(randomBtn);
    randomBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff6d28d9));
    randomBtn.onClick = [this] { processor.randomize(); setPresetName("Random");
                                 if (rackBody) rackBody->enforceHiddenDisabled(); };

    addAndMakeVisible(resetBtn);
    resetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff475569));
    resetBtn.onClick = [this] { processor.resetToDefault(); setPresetName("Init");
                                resetPresetBank();                          // restore the demo presets on F1..F4
                                if (rackBody) rackBody->resetLayout(); };   // factory: sound Init + default layout/visibility

    // Show/hide MODULES menu (Story 4.2): opens a popup of zones + modules to toggle.
    addAndMakeVisible(modulesBtn);
    modulesBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff334155));
    modulesBtn.onClick = [this] { showModulesMenu(); };

    // Online-help language selector (Story 6.1): switches the language of the per-module help
    // panels. Persisted as a global app setting (not per-preset). EN is the base/fallback.
    langBox.addItem("EN", 1);
    langBox.addItem("DE", 2);
    currentLang = loadUiLanguage();
    langBox.setSelectedId(currentLang == "DE" ? 2 : 1, juce::dontSendNotification);
    langBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff334155));
    langBox.onChange = [this]
    {
        currentLang = (langBox.getSelectedId() == 2) ? "DE" : "EN";
        saveUiLanguage(currentLang);
        // Live-update an already-open panel (module OR zone help).
        if (helpPanel && helpPanel->isVisible() && currentHelpId.isNotEmpty() && rackBody)
        {
            // Re-fit, not just re-fill: the other language's text wraps differently, so the
            // panel's width/magnification have to be chosen again (see fitHelpContent).
            const auto pos = helpPanel->getBoundsInParent().getPosition();   // transformed position
            if (auto* f = rackBody->moduleById(currentHelpId))
                fitHelpContent(f->moduleTitle(),
                               HelpTextStore::instance().get(f->helpId(), currentLang));
            else if (currentHelpId.startsWith("zone-"))   // a group's help is showing
                for (auto z : rackBody->zones())
                    if (rack::Rack::zoneHelpId(z) == currentHelpId)
                        fitHelpContent(rack::Rack::zoneName(z),
                                       HelpTextStore::instance().get(currentHelpId, currentLang));
            placeHelpPanel(pos.x, pos.y);   // keep it where the user left it, but back in bounds
        }
    };
    addAndMakeVisible(langBox);

    // Current-preset display
    presetNameLabel.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    presetNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaab3c0));
    presetNameLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(presetNameLabel);
    setPresetName(processor.getCurrentPresetName());   // restored from LiveState

    // Animated 3D wordmark in the header (decorative; transparent + ignores mouse so the
    // header buttons underneath stay clickable). Animation is a persisted setting — right-click
    // the title to toggle; off => plain 2D legacy look (and no timer / CPU).
    addAndMakeVisible(spinningTitle);
    title3DAnimated = loadTitleAnimated();
    spinningTitle.setAnimate(title3DAnimated);

    // On-screen keyboard (shares the processor's MidiKeyboardState → plays the
    // active generators with full ADSR per note, transposed relative to C4).
    keyboard = std::make_unique<FillWidthKeyboard>(
        processor.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard);
    keyboard->setAvailableRange(21, 108);  // A0 .. C8 (full 88-key piano)
    keyboard->setKeyWidth(20.0f);          // FillWidthKeyboard::resized() spreads keys to fill its width
    keyboard->setOctaveForMiddleC(4);      // label MIDI 60 as "C4" (Roland convention) — matches the
                                           // docs/help everywhere ("FREQ knobs = sound at C4", SAMPLER
                                           // ROOT default 60 = C4); JUCE's default label was C3.
    keyboard->clearKeyMappings();   // computer keys are handled by US (see buildComputerKeyMap)
    buildComputerKeyMap();
    // Highlight played (ch.1) notes and the STEP SEQ preview (15.3) — turning a step lights the key
    // it will play — but never the ch.16 auto-play drone, which nobody played.
    keyboard->setMidiChannelsToDisplay(1 | (1 << (SynthyProcessor::kAuditionChannel - 1)));
    // Listen in on the same keyboard state (Story 15.4): while a STEP SEQ figure is being recorded,
    // every played note — computer keys, on-screen keyboard, or the MIDI keyboard, which is the
    // whole appeal — is written into the cursor's step instead of played.
    processor.getKeyboardState().addListener(this);
    // Allow playing via the computer keyboard (a, w, s, e, d, ... map to notes;
    // Up / Down shift the octave; the keyboard must have focus — grabbed on launch/click).
    // The keyboard is added to the rack as a Display module (Input zone) in buildRack();
    // the editor owns its lifetime, the frame parents + sizes it.
    keyboard->setWantsKeyboardFocus(true);
    juce::Component::SafePointer<juce::MidiKeyboardComponent> kbPtr(keyboard.get());
    juce::MessageManager::callAsync([kbPtr]() mutable { if (kbPtr) kbPtr->grabKeyboardFocus(); });

    // The editor itself must NOT grab keyboard focus either (a click on the empty
    // background would otherwise steal it from the keyboard). Up / Down still reach our
    // keyPressed via event bubbling up from the focused keyboard component.
    setWantsKeyboardFocus(false);

    // Stop every knob/toggle/combo from grabbing keyboard focus when clicked, so
    // the on-screen keyboard keeps focus and the computer keys keep playing notes
    // even WHILE the user is tweaking parameters. (The keyboard keeps its focus;
    // a slider's right-click value box still grabs focus on demand for typing.)
    // Story 1.3: stand up the rack BEFORE dropFocus so its controls are also
    // excluded from grabbing keyboard focus.
    buildRack();
    // Re-fit the window height whenever the rack layout changes (show/hide, AD-12).
    if (rackBody)
    {
        rackBody->onLayoutChanged = [this] { refitHeight(); };
        // Tell the rack which enable params ship "on" in the Init patch despite a declared
        // default of 0 (mirrors SynthyProcessor::resetToDefault): OSC 1..3. A zone RESET then
        // reproduces the factory enable state instead of silencing the oscillators.
        for (int i = 1; i <= 3; ++i)
            rackBody->setFactoryEnableDefault(Parameters::ID::oscOn(i), 1.0f);
        rackBody->reloadLayoutFromState();   // apply any layout already loaded from LiveState (Story 4.3)
    }

    std::function<void(juce::Component&)> dropFocus = [&](juce::Component& parent)
    {
        for (auto* child : parent.getChildren())
        {
            if (child == keyboard.get())
                continue;   // the keyboard MUST keep keyboard focus
            child->setWantsKeyboardFocus(false);
            dropFocus(*child);
        }
    };
    dropFocus(*this);

    // Online-help panel (Story 6.1): ONE shared, movable panel, added AFTER dropFocus so it
    // KEEPS keyboard focus (needed for ESC-to-close). Hidden until an info icon is clicked.
    helpPanel = std::make_unique<HelpPanel>();
    helpPanel->onClose = [this] { if (helpPanel) helpPanel->setVisible(false); };
    addChildComponent(*helpPanel);
    if (rackBody)
    {
        rackBody->onModuleHelp = [this](const juce::String& id) { showModuleHelp(id); };
        rackBody->onZoneHelp   = [this](rack::Rack::Zone z)    { showZoneHelp(z); };
    }

    // Size the editor: fixed design width, height derived from the rack's VISIBLE content.
    // Re-run on every show/hide via Rack::onLayoutChanged (AD-12). Must be after the rack +
    // all chrome exist so resized() sees every component.
    refitHeight();

    // Standalone only: rebrand the JUCE wrapper's title bar to "JUCE", left-aligned next to its
    // Options button, so it's clear that strip belongs to the JUCE standalone host (not JASS). The
    // top-level DocumentWindow only exists once the editor is on screen → do it async.
    if (processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
    {
        juce::Component::SafePointer<SynthyEditor> self (this);
        juce::MessageManager::callAsync ([self]
        {
            if (self == nullptr) return;
            if (auto* dw = dynamic_cast<juce::DocumentWindow*> (self->getTopLevelComponent()))
            {
                self->standaloneTitleLnF = std::make_unique<StandaloneTitleLnF>();
                dw->setName ("JUCE");
                dw->setLookAndFeel (self->standaloneTitleLnF.get());
                self->standaloneWin = dw;
                dw->repaint();
            }
        });
    }

    // Drive the OSC FREQ-knob display (played frequency).
    startTimerHz(30);
}

void SynthyEditor::refitHeight()
{
    using namespace EditorGeom;
    // Height follows the rack's visible content so the full rack always fits without scrolling.
    const int rackW = kDesignW - 2 * kMargin;
    const int rackH = rackBody ? rackBody->preferredHeight(rackW) : 800;
    const int designH = juce::jmax(1015, rackH + kBodyTop + kBodyBottom + 2 * kMargin);

    // The display-fit down-scale comes from the WORST CASE — but since Story 7.3 that means every
    // module which MAY appear (Rack::maxHeight), not every module that exists. Deriving it from
    // what is on screen right now made the editor grow and shrink on every preset change (a preset
    // reveals the modules it enables): visibly jumpy, which is what PR #27 fixed. Measuring the
    // may-appear set keeps that property — revealing only ever ADDS to the visible set, so preset
    // switching cannot oscillate — while giving the height back when the user hides a module, which
    // the old "count everything" rule never did.
    //
    // The floor is DERIVED, not a magic number: never render smaller than 1:1 in physical pixels.
    // On a desktop at 150 % that is 1/1.5 = 0.667 — and 0.65 is exactly where the maintainer says
    // the rack stops being readable (2026-08-09). At 100 % it is 1.0, at 125 % it is 0.8. Below
    // this JASS would be smaller than an unscaled UI, which is not a trade worth making.
    //
    // Measured on the display the WINDOW IS ON, not the primary one — those are the same thing only
    // on a single-monitor desk. On the maintainer's (JASS on a 5120x2160 at 150 %, primary elsewhere)
    // the scale was being derived from a screen the window had never touched.
    if (auto* disp = EditorGeom::displayFor(getScreenBounds()))
    {
        const auto ua = disp->userBounds;        // excludes the taskbar
        lastFitDisplay = ua;                     // so the timer can spot a move to another screen
        lastFitDisplayScale = disp->scale;
        const double minScale = juce::jlimit(0.5, 1.0, 1.0 / juce::jmax(1.0, (double) disp->scale));
        const int worstRackH = rackBody ? rackBody->maxHeight(rackW) : rackH;
        const int worstH = juce::jmax(1015, worstRackH + kBodyTop + kBodyBottom + 2 * kMargin);
        const double sH = (ua.getHeight() - kChrome) / (double) worstH;
        const double sW =  ua.getWidth()             / (double) kDesignW;
        fitScale = juce::jlimit(minScale, 1.0, juce::jmin(sH, sW));
    }
    else if (fitScale <= 0.0)
        fitScale = 1.0;

    setTransform(fitScale < 0.999 ? juce::AffineTransform::scale((float) fitScale)
                                  : juce::AffineTransform());
    setSize(kDesignW, designH);
}

void SynthyEditor::showModulesMenu()
{
    if (! rackBody) return;
    // The reorderable customization list (Story 4.2) in a call-out anchored to the button.
    // Parent = nullptr (desktop) so the editor's auto-fit transform doesn't skew mouse coords.
    auto panel = std::make_unique<RackCustomizePanel>(*rackBody, currentLang);

    // NOT CallOutBox::launchAsynchronously: its private callback runs a 200 ms timer that dismisses
    // the box as soon as JASS is no longer the foreground process (juce_CallOutBox.cpp ~line 78).
    // For a menu that is fine; for this panel it is wrong — it carries the rack height BUDGET, a
    // number one reads while doing something else, and it vanished the moment the user clicked into
    // another window (maintainer 2026-08-10). Everything else is JUCE's own pattern: the modal
    // callback owns both the panel and the box, and ModalComponentManager deletes it once the modal
    // state ends — so clicking outside still closes it, and nothing leaks.
    struct KeepOpenCallout final : juce::ModalComponentManager::Callback
    {
        KeepOpenCallout (std::unique_ptr<juce::Component> c, juce::Rectangle<int> area)
            : content (std::move (c)), box (*content, area, nullptr)
        {
            box.setVisible (true);
            box.enterModalState (true, this);
        }
        void modalStateFinished (int) override {}
        std::unique_ptr<juce::Component> content;
        juce::CallOutBox box;
    };

    // Keep a SafePointer to the call-out so the destructor can dismiss it before rackBody dies
    // (the panel references *rackBody).
    modulesCallout = &(new KeepOpenCallout (std::move (panel), modulesBtn.getScreenBounds()))->box;
}

void SynthyEditor::timerCallback()
{
    // RT-safety (11.1): run any auto-enable coupling deferred from the audio thread (host automation)
    // here on the message thread — where allocating + setValueNotifyingHost is safe.
    processor.reconcileParamCouplingsIfDirty();

    // Dragged onto another monitor? Re-fit. Moving a window changes no bounds we already watch, so
    // without this the rack keeps the previous screen's scale — on a desk that mixes a 5120x2160 at
    // 150 % with a smaller display, that is the difference between fitting and being cut off. The
    // check itself is two comparisons; refitHeight (which re-measures the rack) runs only on a
    // real change.
    if (lastFitDisplayScale > 0.0)
        if (auto* disp = EditorGeom::displayFor (getScreenBounds()))
            if (disp->userBounds != lastFitDisplay || disp->scale != lastFitDisplayScale)
                refitHeight();

    double ratio = processor.getCurrentNoteRatio();

    // Live modulation rings (Story 8.1): build the amount currently applied to each
    // ModTarget by PERIODIC (LFO) sources = the display LFO value × the summed LFO-sourced
    // routing coefficient into that target. That is the implicit legacy LFO→its-own-target
    // routing PLUS any matrix slot whose source is LFO 1. Velocity/Envelope need a sounding
    // note, so they contribute no idle-time ring (acceptable for v1, AC7).
    namespace P = Parameters::ID;
    auto& apvts = processor.getAPVTS();

    // Build the ring feed (indexed by ModTarget == LFOTarget index). Only FREE-RUNNING sources
    // (LFOs, Chaos) animate at idle — Envelope/Velocity stay 0 (they need a sounding note; AC7).
    // lfoSrcVal[src] holds each such source's display value at its ModSource slot; note-bound
    // sources stay 0, so the matrix loop can add amt*lfoSrcVal[src] unconditionally.
    static constexpr int kLfoSourceIdx[kNumLFOs] = { (int) ModSource::LFO1, (int) ModSource::LFO2,
                                                     (int) ModSource::LFO3, (int) ModSource::LFO4 };
    std::array<float, ModMatrixConfig::kNumSources> lfoSrcVal {};
    rack::LiveModFeed feed {};
    // LFOs are pure matrix sources now (no built-in target) — just record each LFO's display value
    // at its ModSource slot; the matrix loop below lights the rings for LFO-sourced routings.
    for (int i = 0; i < kNumLFOs; ++i)
        lfoSrcVal[(size_t) kLfoSourceIdx[i]] = processor.getLfoDisplayValue(i);
    lfoSrcVal[(size_t) ModSource::ChaosX] = processor.getChaosDisplayValue(0);   // same snapshot the
    lfoSrcVal[(size_t) ModSource::ChaosY] = processor.getChaosDisplayValue(1);   // voices modulate with
    {
        const bool matrixOn = *apvts.getRawParameterValue(P::modMatrixOn) > 0.5f;
        if (matrixOn)
            for (int n = 1; n <= ModMatrixConfig::kNumSlots; ++n)
            {
                const int   src = (int) *apvts.getRawParameterValue(P::modSlotSource(n));
                const int   mod = (int) *apvts.getRawParameterValue(P::modSlotModule(n));
                const int   par = (int) *apvts.getRawParameterValue(P::modSlotParam(n));
                const float amt = *apvts.getRawParameterValue(P::modSlotAmount(n));
                if (mod <= 0 || src < 0 || src >= (int) lfoSrcVal.size()) continue;   // Off / bad source
                const int   tgt = (int) ModDest::targetOf(mod, par);
                const float val = amt * lfoSrcVal[(size_t) src];   // Env/Vel contribute 0 (no idle ring)
                if (const int oscIdx = ModDest::oscIndexOf(mod); oscIdx >= 0 && oscIdx < 3)
                {
                    if (const int slot = ModDest::oscParamSlot((LFOTarget) tgt); slot >= 0)
                        feed.osc[oscIdx][slot] += val;   // per-OSC: light only that oscillator's knob
                }
                else if (tgt > 0 && tgt < (int) feed.byTarget.size())
                    feed.byTarget[(size_t) tgt] += val;   // global (incl. "Alle OSC")
            }
    }

    // Enforce the KEYBOARD module's enable on its INPUT: JUCE's MidiKeyboardComponent ignores
    // the enabled flag (it never reads isEnabled), so a disabled module would otherwise still
    // be playable. When keyboardOn is off, block mouse clicks and drop keyboard focus (computer
    // keys only sound while focused); when on, restore both. Poll-and-diff (matches the frame's
    // dim sync) so this touches the component only when the toggle actually flips.
    if (keyboard)
    {
        const bool kbOn = *apvts.getRawParameterValue("keyboardOn") > 0.5f;
        if (kbOn != keyboardPlayable)
        {
            keyboardPlayable = kbOn;
            keyboard->setInterceptsMouseClicks (kbOn, kbOn);
            keyboard->setWantsKeyboardFocus (kbOn);
            if (kbOn)
                keyboard->grabKeyboardFocus();
            else
            {
                releaseComputerKeys();   // module switched off mid-note: don't leave notes hanging
                if (keyboard->hasKeyboardFocus (true))
                    keyboard->giveAwayKeyboardFocus();
            }
        }

        // Safety sweep for the computer-key notes: a key released while JASS is NOT the foreground
        // app sends us no event, so its note would hang. Releases only — starting a note still
        // requires a real key event, otherwise typing elsewhere would play JASS. Also drop
        // everything the moment we lose the foreground, mirroring what JUCE's own focusLost did.
        if (juce::Process::isForegroundProcess())
            updateComputerKeys (false);
        else
            releaseComputerKeys();

        // Same idea for the STEP SEQ preview (15.3): a knob moved by the wheel or the value box
        // never reports a drag end, so the note is closed on a timeout instead.
        if (auditionTicks > 0 && --auditionTicks == 0)
            auditionStep (0, false);

        // Show the running pattern on the keyboard (maintainer 2026-08-11): the sequencer's notes
        // bypass the keyboard state on purpose, so the display is fed from the processor's atomic.
        if (keyboard != nullptr)
            keyboard->setPatternNote (processor.getSeqNote());

        // Recording a figure (15.4): parameters are written HERE, on the message thread, from the
        // note handleNoteOn parked — that callback runs on the audio thread for hardware MIDI.
        // Switching STEP SEQ off ends the recording, so an armed cursor can never outlive its module.
        {
            const bool seqOnNow = *apvts.getRawParameterValue (Parameters::ID::seqOn) >= 0.5f;
            if (seqCursor >= 0)
            {
                // AC6 ends the recording when the module is SWITCHED off — the falling edge, not the
                // state. Writing a figure into a silent sequencer is a perfectly sane way to work
                // (and the only quiet one), so simply being off must not refuse the cursor.
                if (seqOnLast && ! seqOnNow)
                    seqSetCursor (-1);
                else if (const int pending = seqPendingNote.exchange (-1); pending >= 0)
                    seqWriteNote (pending);
            }
            seqOnLast = seqOnNow;
        }

        // A modal popup (MODULES call-out, a combo dropdown, …) grabs the keyboard focus, so
        // computer-key playing pauses while it's open. When it closes, hand focus back to the
        // on-screen keyboard so playing resumes without an extra click — but NOT while the user
        // is typing in a value box (a TextEditor has focus then). Edge-triggered on modal-close.
        const bool modalOpen = juce::Component::getCurrentlyModalComponent() != nullptr;
        if (keyboardPlayable && modalWasOpen && ! modalOpen
            && dynamic_cast<juce::TextEditor*> (juce::Component::getCurrentlyFocusedComponent()) == nullptr)
            keyboard->grabKeyboardFocus();
        modalWasOpen = modalOpen;
    }

    // Story 12.6: the sample sets are preloaded on a background thread, so the SET combo has to
    // pick up entries as they land. Edge-triggered on the set COUNT — a plain int compare per
    // tick, and the re-list only happens while sets are still arriving (a few times at startup).
    if (const int sets = SampleBankStore::instance().getNumSets(); sets != shownSampleSets)
    {
        shownSampleSets = sets;
        if (rackBody)
        {
            if (auto* f = rackBody->moduleById("sampler"))
                f->refreshCombo(Parameters::ID::samplerSet);
            // ...and PERC's KIT, for exactly the same reason. Without this its list stayed as it was
            // built — at startup that is EMPTY apart from "(no kit)" — so the combo could not show
            // the kit the patch was actually playing: the parameter pointed at a real set, the
            // resync skipped an item index the list did not have yet, and the box kept reading
            // "(no kit)" while the drums ran (maintainer 2026-08-11).
            if (auto* f = rackBody->moduleById("perc"))
                f->refreshCombo(Parameters::ID::percKit);
        }
    }

    // The live feed drives the rack (AD-8): ONE timer, rack fans out to its frames.
    if (rackBody)
        rackBody->updateLiveFeed(feed, ratio);

    // Keep the header label in sync: it reacts both to the (async-restored)
    // preset name and to live edits flipping the "modified" flag.
    updatePresetLabel();

    // F1..F12 bank fallback: poll the physical key state here too, but ONLY while JASS is the
    // foreground app (so F-keys never fire for another program). This covers the cases keyStateChanged
    // can't — a MODULES call-out or a combo dropdown owns focus (its own top-level window), so key
    // events don't reach the editor. Shared fKeyDown[] state means no double-trigger with keyStateChanged;
    // it also re-arms a stuck "down" if a release event was ever missed.
    if (juce::Process::isForegroundProcess())
        pollPresetHotkeys();
}

// Preset bank F1..F12 edge detection — shared by keyStateChanged (low-latency, fires the instant a
// key transitions while WE hold focus) and the GUI timer (fallback: catches presses when key events
// don't reach us — e.g. a MODULES call-out or a combo dropdown has focus). Both share fKeyDown[], so
// whichever observes the edge first handles it once; the other sees no new edge (no double-trigger).
// Edge detection on the physical key state is immune to auto-repeat. Single press = load; double = assign.
void SynthyEditor::pollPresetHotkeys()
{
    if (presetBank == nullptr) return;
    const juce::uint32 now = juce::Time::getMillisecondCounter();
    constexpr juce::uint32 kDoublePressMs = 500;   // two presses within this window = "double press"
    for (int i = 0; i < 12; ++i)
    {
        const bool phys = juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::F1Key + i);
        if (phys && ! fKeyDown[i])                 // press edge
        {
            fKeyDown[i] = true;
            if (now - fKeyLastPressMs[i] <= kDoublePressMs)
            {
                fKeyLastPressMs[i] = 0;            // consume, so a third press starts fresh
                assignPresetSlot (i);             // second quick press => assign dialog
            }
            else
            {
                fKeyLastPressMs[i] = now;
                if (presetBank->isAssigned (i))   // first press => load (empty slot: nothing)
                    triggerPresetSlot (i);
            }
        }
        else if (! phys && fKeyDown[i])            // release edge
        {
            fKeyDown[i] = false;
        }
    }
}

bool SynthyEditor::keyStateChanged (bool /*isKeyDown*/)
{
    pollPresetHotkeys();       // we have focus here → lowest-latency path
    updateComputerKeys(true);  // start/stop notes for the note keys (focus implied by the event)
    return false;              // observe only; let the event propagate normally
}

// A loaded-and-untouched preset shows its name; once any parameter changes
// (and until the user saves) it's an unsaved working state → "Current State".
void SynthyEditor::updatePresetLabel()
{
    auto text = processor.isPresetModified()
                  ? juce::String("Current State")
                  : ("Preset: " + processor.getCurrentPresetName());
    if (text != shownLabel)
    {
        shownLabel = text;
        presetNameLabel.setText(text, juce::dontSendNotification);
    }
}

void SynthyEditor::setPresetName(const juce::String& name)
{
    processor.setCurrentPresetName(name);   // keep the processor (LiveState) in sync
    updatePresetLabel();
}

// Shared LOAD path: used by the LOAD button and by the preset-bank F-keys. Loads a .jass file,
// fails loudly on a bad file, keeps the header label in sync, reflects the loaded layout, and
// surfaces a format migration. (Was inline in loadBtn.onClick; extracted for reuse.)
void SynthyEditor::loadPresetFile(const juce::File& f)
{
    const auto res = PresetIO::loadFromFile(processor.getAPVTS(), f);
    if (! res.ok)
    {
        // Fail LOUDLY — a corrupt / non-JASS / missing file must not silently reset to defaults.
        juce::NativeMessageBox::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            currentLang == "DE" ? "Laden fehlgeschlagen" : "Load failed",
            (currentLang == "DE"
                ? "\xe2\x80\x9e" + f.getFileNameWithoutExtension()
                      + "\xe2\x80\x9c konnte nicht geladen werden (defekt oder kein JASS-Preset)."
                : "\"" + f.getFileNameWithoutExtension()
                      + "\" could not be loaded (corrupt or not a JASS preset)."));
        return;
    }

    loadedFormatVersion = res.migrated ? PresetIO::kFormatVersion : res.fileVersion;
    setPresetName(f.getFileNameWithoutExtension());
    if (rackBody) rackBody->reloadLayoutFromState();   // reflect the loaded layout (Story 4.3);
                                                       // this can still adjust enable params
                                                       // (enforceHiddenDisabled forces hidden
                                                       // modules off — e.g. the now-hidden-by-
                                                       // default COMPRESSOR).
    processor.markPresetClean();   // snapshot the SETTLED state AFTER layout enforcement, so a
                                   // freshly loaded preset reads as clean (not "Current State").

    // The STEP SEQ latch is restored by PresetIO from the patch's own "StepSeq.LatchRoot" (15.5),
    // so a sequencer patch starts on the note it was saved on — nothing to do here.

    if (res.migrated)
    {
        // Surface the conversion (AC6): the user should know a format upgrade happened
        // and that the original was backed up.
        juce::NativeMessageBox::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            currentLang == "DE" ? "Preset migriert" : "Preset migrated",
            (currentLang == "DE"
                ? "Von Format v" + juce::String(res.fileVersion) + " auf v"
                      + juce::String(PresetIO::kFormatVersion)
                      + " aktualisiert.\nEine Sicherung liegt im Ordner PresetsBackup."
                : "Upgraded from format v" + juce::String(res.fileVersion) + " to v"
                      + juce::String(PresetIO::kFormatVersion)
                      + ".\nA backup was saved to the PresetsBackup folder."));
    }
}

// Remove every F-key assignment of this preset name — the bank stores names and re-resolves them
// at trigger time, so a renamed or deleted file leaves a slot that can only ever error. Called by
// the DELETE flow and by triggerPresetSlot when the resolve fails (maintainer 2026-08-18).
void SynthyEditor::clearBankSlotsFor(const juce::String& name)
{
    bool changed = false;
    for (size_t i = 0; i < presetSlots.size(); ++i)
        if (presetSlots[i] == name)
        {
            presetSlots[i] = {};
            if (presetBank) presetBank->setAssignment((int) i, {});
            changed = true;
        }
    if (changed)
        PresetIO::savePresetBanks(presetSlots);
}

// Preset quick-access bank: load the preset assigned to a slot (F-key tap / single click).
void SynthyEditor::triggerPresetSlot(int slot)
{
    if (slot < 0 || slot >= (int) presetSlots.size()) return;
    const auto name = presetSlots[(size_t) slot];
    if (name.isEmpty()) return;   // empty slot => nothing to load
    const auto f = PresetIO::presetsFolder().getChildFile(name + ".jass");
    if (! f.existsAsFile())
    {
        // The file behind this slot is gone (renamed or deleted on disk). Say so, then CLEAR the
        // assignment — pressing the key again must not repeat the error (maintainer 2026-08-18).
        juce::NativeMessageBox::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            currentLang == "DE" ? "Preset fehlt" : "Preset missing",
            (currentLang == "DE"
                ? "\xe2\x80\x9e" + name + "\xe2\x80\x9c gibt es nicht mehr \xe2\x80\x94 die Belegung von F"
                      + juce::String(slot + 1) + " wird entfernt."
                : "\"" + name + "\" no longer exists \xe2\x80\x94 clearing the F"
                      + juce::String(slot + 1) + " assignment."));
        clearBankSlotsFor(name);
        return;
    }
    loadPresetFile(f);
}

// Preset quick-access bank: restore the FACTORY default (the four demo presets on F1..F4, rest
// empty), invoked by RESET. Updates the display, the in-memory slots, and the persisted global file.
void SynthyEditor::resetPresetBank()
{
    presetSlots = PresetIO::defaultPresetBank();
    PresetIO::savePresetBanks(presetSlots);
    if (presetBank) presetBank->setAllAssignments(presetSlots);
}

// Preset quick-access bank: open the assign dialog for a slot (double-click / long-hold). The
// chosen preset name is stored in the slot, persisted globally, and shown on the button.
void SynthyEditor::assignPresetSlot(int slot)
{
    if (slot < 0 || slot >= (int) presetSlots.size()) return;
    presetChooser = std::make_unique<juce::FileChooser>(
        (currentLang == "DE" ? "Preset auf F" : "Assign preset to F") + juce::String(slot + 1),
        PresetIO::presetsFolder(), "*.jass");
    auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    juce::Component::SafePointer<SynthyEditor> self (this);
    presetChooser->launchAsync(chooserFlags, [this, self, slot](const juce::FileChooser& fc)
    {
        if (self == nullptr) return;   // editor closed while the dialog was open (then `this` is valid below)
        auto f = fc.getResult();
        if (f == juce::File{}) return;
        const auto name = f.getFileNameWithoutExtension();

        // The bank stores only the NAME and re-resolves it inside the Presets folder at trigger time.
        // So a file picked from ELSEWHERE would silently resolve to a different (or missing) preset —
        // reject anything outside the Presets folder and say why.
        if (f.getParentDirectory() != PresetIO::presetsFolder())
        {
            juce::NativeMessageBox::showMessageBoxAsync(
                juce::MessageBoxIconType::InfoIcon,
                currentLang == "DE" ? "Ordner" : "Folder",
                (currentLang == "DE"
                    ? juce::String("Bitte ein Preset aus dem Presets-Ordner w\xc3\xa4hlen.")
                    : juce::String("Please choose a preset from the Presets folder.")));
            return;
        }

        // No duplicate assignments: the same preset must not sit on two keys. If it is already
        // on another slot, reject (keep everything as-is) and say where it lives.
        for (int other = 0; other < (int) presetSlots.size(); ++other)
            if (other != slot && presetSlots[(size_t) other] == name)
            {
                juce::NativeMessageBox::showMessageBoxAsync(
                    juce::MessageBoxIconType::InfoIcon,
                    currentLang == "DE" ? "Bereits belegt" : "Already assigned",
                    (currentLang == "DE"
                        ? "\xe2\x80\x9e" + name + "\xe2\x80\x9c liegt bereits auf F" + juce::String(other + 1)
                              + ".\nBitte diese Taste zuerst neu belegen."
                        : "\"" + name + "\" is already on F" + juce::String(other + 1)
                              + ".\nReassign that key first."));
                return;
            }

        presetSlots[(size_t) slot] = name;
        PresetIO::savePresetBanks(presetSlots);
        if (presetBank) presetBank->setAssignment(slot, name);
    });
}

// Computer-key → note map for a German (QWERTZ) keyboard: HOME row = white keys, TOP row = black
// keys, spanning the full width from 'a' up to the 'ä'/'#' keys (~2.5 octaves). Playing is detected
// via KeyPress::isKeyCurrentlyDown → VkKeyScan (Windows), which resolves each character to the
// physical key for the ACTIVE layout, so the umlaut keys (ö/ä) register. Octave = Up/Down.
void SynthyEditor::buildComputerKeyMap()
{
    // Letters use CHARACTERS — JUCE resolves them to the active layout via VkKeyScan. The keys
    // PAST the letter block are addressed by their raw virtual-key CODE (physical key) instead of
    // the layout character (ö/ä/#/+), so they don't depend on the German character assignment.
    // 0x10000 == JUCE's Windows extendedKeyModifier: it tells isCurrentlyDown "the low bits are a
    // raw VK, don't run VkKeyScan". (Non-Windows falls back to the character.)
    struct KM { int code; int offsetFromC; };
   #if JUCE_WINDOWS
    constexpr int E = 0x10000;
    const int keyOe = E | 0xC0, keyAe = E | 0xDE, keyHash = E | 0xBF, keyPlus = E | 0xBB;   // VK_OEM_3/7/2/PLUS
   #else
    const int keyOe = L'ö', keyAe = L'ä', keyHash = '#', keyPlus = '+';
   #endif
    const KM whites[] = { {'a',0},{'s',2},{'d',4},{'f',5},{'g',7},{'h',9},{'j',11},
                          {'k',12},{'l',14},{keyOe,16},{keyAe,17},{keyHash,19} };   // C D E F G A B C D E F G
    const KM blacks[] = { {'w',1},{'e',3},{'t',6},{'z',8},{'u',10},{'o',13},{'p',15},{keyPlus,18} };   // C# D# F# G# A# C# D# F#
    computerKeys.clear();
    computerKeys.reserve(std::size(whites) + std::size(blacks));
    for (const auto& k : whites) computerKeys.push_back({ juce::KeyPress(k.code, juce::ModifierKeys(), 0), k.offsetFromC, -1 });
    for (const auto& k : blacks) computerKeys.push_back({ juce::KeyPress(k.code, juce::ModifierKeys(), 0), k.offsetFromC, -1 });
}

// Reconcile the physical key state with what is sounding. The note is resolved ONCE, at press time,
// and remembered per key — that is the whole point: the octave may move underneath a held key and
// its note-off still lands on the note that was actually started. (JUCE's own implementation keys
// its state by note number and cannot do this, which is why we do not use its key mappings.)
//
// allowNoteOn=false is the timer's safety sweep: releases only. Starting notes requires a real key
// event (i.e. focus), otherwise typing in another application would play JASS.
void SynthyEditor::updateComputerKeys(bool allowNoteOn)
{
    if (keyboard == nullptr)
        return;
    const int ch = keyboard->getMidiChannel();
    // A note key must NOT care about modifiers. KeyPress::isCurrentlyDown() also compares them
    // against the KeyPress's own (none, here), so holding SHIFT made every note key read as
    // released â and the sweep below then cut the sound. Which is exactly what SHIFT is for while
    // playing: fine-dragging a knob (user report 2026-08-10, "damit hÃ¶rt abrupt das Spiel auf").
    // The static form asks only about the physical key, with the same layout resolution.
    //
    // Typing into a value box must not play either: this handler hangs off the EDITOR, so key
    // events bubble up to it even while a TextEditor has focus. Note-ONs are suppressed there â
    // releases are not, or a note started before the click would hang.
    const bool typing = dynamic_cast<juce::TextEditor*> (juce::Component::getCurrentlyFocusedComponent()) != nullptr;
    for (auto& k : computerKeys)
    {
        const bool down = juce::KeyPress::isKeyCurrentlyDown (k.key.getKeyCode());
        if (down && k.sounding < 0)
        {
            if (! allowNoteOn || ! keyboardPlayable || typing)
                continue;
            k.sounding = juce::jlimit(0, 127, 12 * kbBaseOctave + k.offsetFromC);
            processor.getKeyboardState().noteOn(ch, k.sounding, 1.0f);
        }
        else if (! down && k.sounding >= 0)
        {
            processor.getKeyboardState().noteOff(ch, k.sounding, 0.0f);
            k.sounding = -1;
        }
    }
}

// Move every SOUNDING computer-key note to the current octave (called when Up/Down shifts it while
// keys are held). It is a re-trigger, not a glide: the new note starts a fresh envelope, which is
// what makes the switch audible. Keys that are not sounding are untouched — they pick up the new
// octave at their next press, like always.
void SynthyEditor::retuneSoundingComputerKeys()
{
    if (keyboard == nullptr)
        return;
    const int ch = keyboard->getMidiChannel();
    for (auto& k : computerKeys)
    {
        if (k.sounding < 0)
            continue;
        const int note = juce::jlimit(0, 127, 12 * kbBaseOctave + k.offsetFromC);
        if (note == k.sounding)
            continue;                       // clamped to the same note — leave it playing
        processor.getKeyboardState().noteOff(ch, k.sounding, 0.0f);
        processor.getKeyboardState().noteOn(ch, note, 1.0f);
        k.sounding = note;
    }
}

// Sound the step that is being edited (Story 15.3). A step's value is a number of SEMITONES, so it
// needs a reference pitch: the computer keyboard's current C — MIDI 48 (C3) by default, moving with
// the Up/Down octave keys, so the preview matches the octave you are playing in.
//
// It rides its own MIDI channel. Channel 1 would be swallowed: while STEP SEQ (or the ARP) runs the
// processor drops every channel-1 note so that only the pattern sounds — precisely the moment a
// preview is wanted. Velocity is the 100 the sequencer itself emits, or the preview would be louder
// than the step it previews.
void SynthyEditor::auditionStep(int semitones, bool sounding)
{
    auto& state = processor.getKeyboardState();
    const int ch = SynthyProcessor::kAuditionChannel;
    if (! sounding)
    {
        if (auditionNote >= 0)
            state.noteOff(ch, auditionNote, 0.0f);
        auditionNote  = -1;
        auditionTicks = 0;
        return;
    }
    const int note = juce::jlimit(0, 127, 12 * kbBaseOctave + semitones);
    if (note != auditionNote)
    {
        if (auditionNote >= 0)
            state.noteOff(ch, auditionNote, 0.0f);   // re-trigger: a drag scrubs the scale
        state.noteOn(ch, note, 100.0f / 127.0f);
        auditionNote = note;
    }
    auditionTicks = 24;   // ~0.8 s at the editor's 30 Hz — the wheel and the value box have no
                          // drag end, and a hanging note is worse than a short one
}

// --- Writing a figure by playing it (Story 15.4) -------------------------------------------------
// The cursor is the step the next played note lands in. It is UI state only: no parameter, nothing
// persisted (AC9). Setting it also arms the processor, which then stops looking for a root note —
// otherwise every key entered would restart and transpose the figure while it is being written.
void SynthyEditor::seqSetCursor(int step)
{
    if (step == seqCursor)
        return;
    seqCursor = step;
    processor.setSeqRecordArmed(seqCursor >= 0);
    if (seqCursor < 0)
        seqPendingNote.store(-1);   // drop anything parked between the last note and the stop
}

// Write the played note into the cursor's step and move on. The value is the offset from the
// computer keyboard's current C — the SAME reference auditionStep previews with (15.3), so what a
// knob sounds and what a key writes agree, and both follow the Up/Down octave keys (AC2).
void SynthyEditor::seqWriteNote(int midiNote)
{
    if (seqCursor < 0 || seqCursor >= StepSequencer::kMaxSteps)
        return;
    namespace P = Parameters::ID;
    auto& apvts     = processor.getAPVTS();
    const int step  = seqCursor + 1;                                       // params are 1-based
    const int semis = juce::jlimit(-24, 24, midiNote - 12 * kbBaseOctave);

    if (auto* p = apvts.getParameter(P::seqPitch(step)))
        p->setValueNotifyingHost(p->convertTo0to1((float) semis));
    // A written step is not a rest (AC3) — a figure entered by playing must sound without a
    // second pass over 32 switches.
    if (auto* on = apvts.getParameter(P::seqStep(step)))
        on->setValueNotifyingHost(1.0f);

    auditionStep(semis, true);   // hear what was just written, through 15.3's one preview path
    seqAdvanceCursor();
}

// SPACE: this step stays silent. Skipping has to SWITCH THE STEP OFF, not merely step over it —
// after ↺ every step is on at offset 0, so a step passed by without a word would play the root
// rather than rest (maintainer 2026-08-11). The pitch is left alone, so switching the step back on
// restores whatever was written there before.
void SynthyEditor::seqSkipStep()
{
    if (seqCursor < 0)
        return;
    if (auto* on = processor.getAPVTS().getParameter(Parameters::ID::seqStep(seqCursor + 1)))
        on->setValueNotifyingHost(0.0f);
    seqAdvanceCursor();
}

// Next step — or stop, once the step after LEN would be reached (AC6).
void SynthyEditor::seqAdvanceCursor()
{
    if (seqCursor < 0)
        return;
    const int len  = (int) *processor.getAPVTS().getRawParameterValue(Parameters::ID::seqLength);
    const int next = seqCursor + 1;
    seqSetCursor((next >= len || next >= StepSequencer::kMaxSteps) ? -1 : next);
}

// MidiKeyboardState calls this on the AUDIO thread for hardware MIDI (the state is pumped from
// processBlock), so nothing is written here — the note is parked and the 30 Hz timer picks it up on
// the message thread. Our own preview channel and the auto-play drone are not played notes.
void SynthyEditor::handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNote, float)
{
    if (seqCursor < 0)
        return;
    if (midiChannel == SynthyProcessor::kAuditionChannel || midiChannel == SynthyProcessor::kDroneChannel)
        return;
    seqPendingNote.store(midiNote);
}

void SynthyEditor::handleNoteOff(juce::MidiKeyboardState*, int, int, float) {}

// Note-off everything we started (KEYBOARD module switched off, focus lost, …). Only our own
// notes — external MIDI hardware and the ch.16 auto-play drone are untouched.
void SynthyEditor::releaseComputerKeys()
{
    if (keyboard == nullptr)
        return;
    const int ch = keyboard->getMidiChannel();
    for (auto& k : computerKeys)
        if (k.sounding >= 0)
        {
            processor.getKeyboardState().noteOff(ch, k.sounding, 0.0f);
            k.sounding = -1;
        }
}

bool SynthyEditor::keyPressed(const juce::KeyPress& key)
{
    // ESC closes an open help panel (Story 6.1) — fallback in case the panel lost focus.
    if (key == juce::KeyPress::escapeKey && helpPanel && helpPanel->isVisible())
    {
        helpPanel->setVisible(false);
        return true;
    }
    // Up / Down arrows shift the computer-keyboard octave. (Moved off z/x — those are now note keys,
    // so the whole letter area from 'a' to the 'ä'/'#' keys is free for playing.)
    if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
    {
        int dir = (key == juce::KeyPress::upKey) ? 1 : -1;
        const int oct = juce::jlimit(1, 7, kbBaseOctave + dir);
        if (oct == kbBaseOctave)
            return true;                    // already at the end of the range — nothing to do
        kbBaseOctave = oct;
        // Shifting mid-play is a performance gesture, so held keys FOLLOW the octave: their note
        // moves with the switch instead of either dying (the pre-2026-08-09 behaviour) or sitting
        // stubbornly at the old pitch. Because we own the key→note bookkeeping, moving a note is
        // just note-off/note-on on OUR remembered note — no bitmask can fall out of sync, and
        // releasing the key later still lands on whatever note it is currently sounding.
        retuneSoundingComputerKeys();
        // A LATCHED step-sequencer figure has no held key left to retune, so the octave shift is
        // handed to its root directly — the pattern moves with the arrows like everything else.
        processor.transposeSeqLatch (dir * 12);
        // STEP SEQ's note read-outs are relative to this octave — re-text them (no value changed,
        // so nothing else would).
        if (rackBody != nullptr)
            rackBody->refreshNamedReadouts();
        return true;
    }
    // While a figure is being recorded (15.4) SPACE means "leave this step empty and move on".
    // It takes precedence over the Karplus pluck below: during recording the whole keyboard is a
    // writing surface, and a rest is the one thing no note key can express.
    if (key == juce::KeyPress::spaceKey && seqCursor >= 0)
    {
        seqSkipStep();
        return true;
    }
    // SPACE stops a LATCHED figure. Since the pattern outlives the key that started it, something
    // has to end it — and the key that means "nothing" everywhere else is the obvious one. Ranked
    // between writing (above: SPACE is a rest) and the Karplus pluck (below), so it only claims the
    // key while there is actually a figure running.
    if (key == juce::KeyPress::spaceKey && processor.isSeqLatched())
    {
        processor.stopSeqLatch();
        return true;
    }
    // Spacebar re-plucks the Karplus string. Trigger the actual PLUCK button so it
    // visibly presses too (its onClick calls pluckString); fall back to the direct call.
    if (key == juce::KeyPress::spaceKey)
    {
        if (auto* f = rackBody ? rackBody->moduleById("stringkarplus") : nullptr)
            f->clickFirstAction();
        else
            processor.pluckString();
        return true;
    }
    // ESC closes the help panel. Handled HERE (not in the panel) so the panel never needs to
    // grab keyboard focus — opening help therefore never interrupts computer-keyboard playing.
    if (key == juce::KeyPress::escapeKey && helpPanel && helpPanel->isVisible())
    {
        helpPanel->setVisible(false);
        return true;
    }
    return false;
}

void SynthyEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));
    // The title ("J A S S" + subtitle) is drawn by the animated SpinningTitle3D child now.
}

// --- SpinningTitle3D: extruded, Y-rotating 3D wordmark (see PluginEditor.h) -----------------

void SpinningTitle3D::resized()
{
    rebuildGlyphPath();
}

void SpinningTitle3D::rebuildGlyphPath()
{
    glyphPath.clear();
    constexpr float band = 20.0f;   // subtitle strip along the bottom
    const float h = juce::jmax(12.0f, ((float) getHeight() - band) * 0.66f);
    juce::GlyphArrangement ga;
    ga.addLineOfText(juce::Font(juce::FontOptions(h, juce::Font::bold)), "J A S S", 0.0f, 0.0f);
    ga.createPath(glyphPath);
    const auto tb = glyphPath.getBounds();
    glyphPath.applyTransform(juce::AffineTransform::translation(-tb.getCentreX(), -tb.getCentreY()));
}

void SpinningTitle3D::paint(juce::Graphics& g)
{
    auto area    = getLocalBounds().toFloat();
    auto subArea = area.removeFromBottom(20.0f);

    if (! animate)
    {
        // Plain legacy 2D wordmark (animation disabled in settings).
        g.setColour(juce::Colour(0xff40c0ff));
        g.setFont(juce::FontOptions(26.0f, juce::Font::bold));
        g.drawText("J A S S", area.toNearestInt(), juce::Justification::centred);
    }
    else if (! glyphPath.isEmpty())
    {
        const float cx = area.getCentreX();
        const float cy = area.getCentreY();
        const float ct = std::cos(angle);
        const float st = std::sin(angle);

        // Extrusion slices, sorted far → near for the painter's algorithm. A slice at depth z0
        // has centre rotated-z = z0·cos(angle); larger = farther from the viewer → drawn first.
        std::array<float, (size_t) kLayers> z0s;
        for (int i = 0; i < kLayers; ++i)
            z0s[(size_t) i] = -kDepth * 0.5f + kDepth * (float) i / (float) (kLayers - 1);
        std::sort(z0s.begin(), z0s.end(), [ct](float a, float b) { return a * ct > b * ct; });

        const juce::Colour back  (0xff0b3a5e);   // deep blue (far side / wall base)
        const juce::Colour wall  (0xff2f6f96);   // muted blue (wall top) — kept CLEARLY darker than
        const juce::Colour front (0xff6fd3ff);   // the bright near face, so the front reads as ONE
        for (int k = 0; k < kLayers; ++k)         // crisp lit surface instead of a smeared stack.
        {
            const float z0 = z0s[(size_t) k];
            const float t  = (float) k / (float) (kLayers - 1);   // 0 = back, 1 = front
            juce::Path p = glyphPath;
            // Orthographic Y-rotation of a flat (z = z0) glyph reduces to an affine:
            //   x' = x·cos + z0·sin ,  y' = y   → x-scale by cos, x-shift by z0·sin.
            p.applyTransform(juce::AffineTransform::scale(ct, 1.0f)
                                 .translated(cx + z0 * st, cy));
            // The wall stays in the darker back→wall range; only the very front slice is the
            // bright face → the stirnfläche keeps a defined edge (no bright doubled outline).
            const bool isFront = (k == kLayers - 1);
            g.setColour(isFront ? front : back.interpolatedWith(wall, t));
            g.fillPath(p);
        }
    }

    g.setColour(juce::Colour(0xff8899aa));
    g.setFont(juce::FontOptions(13.0f));
    g.drawText("Just Another Simple Synthesizer  (v" + JASS::versionString() + ")",
               subArea.toNearestInt(), juce::Justification::centred);
}

// The editor carries a display-fit down-scale (AD-12) that shrinks EVERY child — on a 1440 px
// display that is 0.65, which renders the panel's 14 pt body type at about 9 pt: help you need
// reading glasses for. The panel is a plain overlay, so it can simply cancel the transform and
// draw at true pixel size. Both helpers below exist so the two callers agree on the size the
// panel actually occupies on screen (kWidth is its UNSCALED width).
// Fill the panel and pick the geometry it is shown at. Text size stays at full magnification —
// readability is the whole point of opting out of the auto-fit scale. What varies is the shape:
// a long text (MOD MATRIX, KEYBOARD) is first given a WIDER panel, since fewer wrapped lines
// make it shorter and this design has width to spare. Whatever is still too tall for the window
// scrolls inside the panel, so nothing is ever cut off at the bottom.
void SynthyEditor::fitHelpContent(const juce::String& title, const juce::String& body)
{
    helpScale = (fitScale > 0.0) ? 1.0 / fitScale : 1.0;

    // Cap in the panel's OWN coordinates: it is drawn magnified by helpScale, so the window
    // height it may occupy has to be divided by that factor.
    const int availH    = juce::jmax(200, getHeight() - 8 - 80);   // 80 = keyboard clearance
    const int maxPanelH = juce::jmax(120, (int) (availH / helpScale));

    for (const int w : { HelpPanel::kWidth, 480, 620, 760 })
        if (helpPanel->setContent(title, body, w, maxPanelH))
            return;   // fits without scrolling at this width
    // Still longer than the window even at 760 px: the panel stays at that width and scrolls.
}

int SynthyEditor::helpPanelWidthOnScreen() const
{
    return juce::roundToInt((helpPanel != nullptr ? helpPanel->getWidth() : HelpPanel::kWidth)
                            * helpScale);
}

void SynthyEditor::placeHelpPanel(int wantX, int wantY)
{
    if (helpPanel == nullptr) return;

    helpPanel->setTransform(helpScale > 1.001 ? juce::AffineTransform::scale((float) helpScale)
                                              : juce::AffineTransform());
    // Clamp against the size the panel occupies AFTER its own transform, or a scaled-up panel
    // would hang off the right/bottom edge.
    const int effW = juce::roundToInt(helpPanel->getWidth()  * helpScale);
    const int effH = juce::roundToInt(helpPanel->getHeight() * helpScale);
    const int maxX = juce::jmax(8, getWidth()  - effW - 8);
    const int maxY = juce::jmax(8, getHeight() - effH - 80);   // keep clear of the keyboard
    const int x    = juce::jlimit(8, maxX, wantX);
    const int y    = juce::jlimit(8, maxY, wantY);
    // JUCE transforms a component's WHOLE bounds — position included (getBoundsInParent()), so
    // a scale of 1.54 would also multiply x/y and push the panel off the bottom right. Feed it
    // the pre-image of the position we want, exactly as JUCE's own setCentrePosition() does.
    helpPanel->setTopLeftPosition(juce::roundToInt(x / helpScale),
                                  juce::roundToInt(y / helpScale));
}

void SynthyEditor::showModuleHelp(const juce::String& id)
{
    if (helpPanel == nullptr || rackBody == nullptr)
        return;

    // Toggle: clicking the SAME module's info icon while its panel is open closes it.
    if (helpPanel->isVisible() && currentHelpId == id)
    {
        helpPanel->setVisible(false);
        return;
    }

    auto* f = rackBody->moduleById(id);
    const juce::String title  = (f != nullptr) ? f->moduleTitle() : id;
    const juce::String helpId = (f != nullptr) ? f->helpId()      : id;   // may alias (LFO 1..4 => "lfo")
    fitHelpContent(title, HelpTextStore::instance().get(helpId, currentLang));   // sets size first
    currentHelpId = id;

    // Anchor beside the clicked module: to its right if there's room, else to its left,
    // aligned to its top — then clamp fully inside the editor (above the keyboard band).
    const int panelW = helpPanelWidthOnScreen();
    int x = getWidth() / 2 - panelW / 2, y = 90;
    if (f != nullptr)
    {
        auto tl = getLocalPoint(f, juce::Point<int>(0, 0));   // module top-left in editor coords
        x = tl.x + f->getWidth() + 8;
        if (x + panelW > getWidth() - 8)
            x = tl.x - panelW - 8;                            // no room right → place left
        y = tl.y;
    }
    placeHelpPanel(x, y);

    helpPanel->setVisible(true);
    helpPanel->toFront(false);   // false = do NOT steal keyboard focus (keeps the on-screen
                                 // keyboard playable via the computer keys). ESC is handled by
                                 // the editor's keyPressed, so the panel needs no focus.
}

void SynthyEditor::showZoneHelp(rack::Rack::Zone zone)
{
    if (helpPanel == nullptr)
        return;

    const juce::String helpId = rack::Rack::zoneHelpId(zone);   // e.g. "zone-generators"

    // Toggle: clicking the SAME group's info icon while its panel is open closes it.
    if (helpPanel->isVisible() && currentHelpId == helpId)
    {
        helpPanel->setVisible(false);
        return;
    }

    // Panel title = the group name; content = the zone's help text in the active language.
    fitHelpContent(rack::Rack::zoneName(zone),
                   HelpTextStore::instance().get(helpId, currentLang));
    currentHelpId = helpId;

    // Anchor beside the clicked info icon (it sits at the header's right edge): place the panel
    // to its LEFT, just below the header row; fall back to the right if there's no room left.
    const int panelW = helpPanelWidthOnScreen();
    int x = getWidth() / 2 - panelW / 2, y = 96;
    if (rackBody != nullptr)
        if (auto* anchor = rackBody->zoneInfoButton(zone))
        {
            auto tl = getLocalPoint(anchor, juce::Point<int>(0, 0));
            x = tl.x - panelW - 8;
            if (x < 8)
                x = tl.x + anchor->getWidth() + 8;
            y = tl.y + anchor->getHeight() + 6;
        }
    placeHelpPanel(x, y);

    helpPanel->setVisible(true);
    helpPanel->toFront(false);
}

juce::File SynthyEditor::uiLanguageFile()
{
    return PresetIO::jassFolder().getChildFile("ui-language.txt");
}

juce::String SynthyEditor::loadUiLanguage()
{
    auto f = uiLanguageFile();
    if (f.existsAsFile())
    {
        auto s = f.loadFileAsString().trim().toUpperCase();
        if (s == "EN" || s == "DE")
            return s;
    }
    return "EN";
}

void SynthyEditor::saveUiLanguage(const juce::String& lang)
{
    uiLanguageFile().replaceWithText(lang);
}

juce::File SynthyEditor::titleAnimFile()
{
    return PresetIO::jassFolder().getChildFile("title-anim.txt");
}

bool SynthyEditor::loadTitleAnimated()
{
    auto f = titleAnimFile();
    if (f.existsAsFile())
        return f.loadFileAsString().trim() != "0";   // "0" => off; anything else / missing => on
    return true;
}

void SynthyEditor::saveTitleAnimated(bool on)
{
    titleAnimFile().replaceWithText(on ? "1" : "0");
}

void SynthyEditor::mouseDown(const juce::MouseEvent& e)
{
    // Right-click on the title band toggles the 3D animation (persisted). The wordmark component
    // is click-transparent, so the click lands here on the editor; the header buttons keep their
    // own clicks. Left-clicks are ignored (nothing to do in the header background).
    if (! e.mods.isPopupMenu() || ! g_titleBounds.contains(e.getPosition()))
        return;

    juce::PopupMenu m;
    // "About"-style info (disabled items): app version (CalVer) + the format version of the
    // currently loaded preset (Story 9.2, AC2/AC6).
    m.addItem(100, "JASS " + JASS::versionString(), false, false);
    m.addItem(101, (currentLang == "DE" ? "Preset-Format v" : "Preset format v")
                       + juce::String(loadedFormatVersion), false, false);
    m.addSeparator();
    m.addItem(1, (currentLang == "DE" ? "3D-Titel animieren" : "Animate 3D title"),
              true, title3DAnimated);
    juce::Component::SafePointer<SynthyEditor> self (this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(
                        juce::Rectangle<int>(e.getScreenX(), e.getScreenY(), 1, 1)),
                    [self](int r) mutable
                    {
                        if (self == nullptr || r != 1) return;   // editor closed while menu was open
                        self->title3DAnimated = ! self->title3DAnimated;
                        self->spinningTitle.setAnimate(self->title3DAnimated);
                        self->saveTitleAnimated(self->title3DAnimated);
                    });
}

void SynthyEditor::buildRack()
{
    // TEMP (Story 1.3): a throwaway population to verify the grid engine, zone headers
    // and shared look at the fixed 1920×1200 target. It mirrors the mockup census
    // (≈10×S, 6×M, 4×L) and binds REAL Parameters::ID values so the frames' APVTS
    // attachments resolve. Story 1.5 replaces this with the real module descriptors.
    using namespace rack;
    auto& apvts = processor.getAPVTS();
    // MASTER BUS is the top row (first zone), then the three main zones below it.
    rackBody = std::make_unique<Rack>(apvts, Rack::kDefaultCols,
        std::vector<Rack::Zone>{ Rack::Zone::MasterBus, Rack::Zone::Generators,
                                 Rack::Zone::Modulation, Rack::Zone::Processing,
                                 Rack::Zone::Visualization, Rack::Zone::Input });

    namespace P = Parameters::ID;

    // Per-generator PAN gating (Epic 10 follow-up). In Mono and Pseudo-Stereo the voice renders
    // SINGLE-channel — positionToGains collapses to gain 1.0 and the pan VALUE is never read — so
    // every PAN knob is inert in those two modes. Left looking live it actively misleads, because PAN
    // is the one control that makes Stereo-Pan / Binaural / Kunstkopf differ from each other at all:
    // with every generator centred those three modes render identical mono, since there is no
    // direction to place. So grey the knobs out where they do nothing. Applied centrally on the way
    // into the rack, which also covers the hand-built bodies (KARPLUS, WAVETABLE) and any PAN
    // parameter added later; an explicit per-knob predicate (STEREO's WIDTH/TIME) is never overwritten.
    auto* outModeRaw = apvts.getRawParameterValue (P::outputMode);
    auto panKnobsActive = [outModeRaw]
    {
        const int m = (int) outModeRaw->load();
        return m != (int) OutputMode::Mono && m != (int) OutputMode::PseudoStereo;
    };
    auto addRackModule = [this, panKnobsActive] (ModuleDescriptor d)
    {
        for (auto& el : d.body)
            if (auto* k = std::get_if<Knob> (&el))
                if (k->activeWhen == nullptr && k->paramId.endsWith ("Pan"))
                    k->activeWhen = panKnobsActive;
        rackBody->addModule (std::move (d));
    };

    // small builders to keep the descriptor list readable
    auto K = [](juce::String id, juce::String lbl) { return Knob{ std::move(id), std::move(lbl) }; };
    auto C = [](juce::String id, juce::String lbl, juce::StringArray items)
             { return Combo{ std::move(id), std::move(lbl), std::move(items) }; };
    // Story 1.4 (verification wiring; folded into the real descriptors in 1.5/2.2):
    // a knob tagged as an LFO ring target …
    auto Kmod = [](juce::String id, juce::String lbl, ModTarget mt)
             { Knob k{ std::move(id), std::move(lbl) }; k.modTarget = mt; return k; };
    // (Kfreq — the FREQ display-transform helper — is now in OscSpecs.h via freqDisplay.)

    // A free-running knob is meaningless while its module is tempo-synced: SYNC other than "Free"
    // means the division decides and the DSP ignores the knob entirely. Grey it out instead of
    // leaving it sitting there looking live (user 2026-08-10) - the same treatment STEREO's
    // WIDTH/TIME get outside Pseudo-Stereo. Index 0 is "Free" (SyncDivision::kNames), so the knob
    // is active exactly when the index is 0.
    auto greyWhenSynced = [&apvts] (ModuleDescriptor& d, const juce::String& knobId,
                                    const juce::String& syncParamId)
    {
        auto* sync = apvts.getRawParameterValue (syncParamId);
        for (auto& el : d.body)
            if (auto* k = std::get_if<Knob> (&el))
                if (k->paramId == knobId)
                    k->activeWhen = [sync] { return sync != nullptr && (int) sync->load() == 0; };
    };

    auto add = [&](Rack::Zone zone, SizeClass sc, ModuleType type, juce::String title,
                   juce::String enableParam, std::vector<BodyElement> body,
                   std::function<void()> onReset = {}, bool visualOnly = false)
    {
        ModuleDescriptor d;
        d.sizeClass = sc; d.type = type; d.visualOnly = visualOnly;
        // Stable slug from the title (e.g. "OSC 1" -> "osc1") — the RackLayout key for
        // show/hide + drag-drop (AD-10). Derived once here so every module gets one.
        d.id = title.toLowerCase().retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789");
        d.title = std::move(title);
        d.defaultZone = zone;   // AD-10: zone declared on the descriptor
        d.enableParam = std::move(enableParam); d.body = std::move(body);
        d.onReset = std::move(onReset);   // extra non-param reset (e.g. scope time-base)
        addRackModule(std::move(d));
    };


    // ---- MASTER BUS (top row; PROTOTYPE: decisions A+B) ----
    // Stereo becomes a normal module whose Enable IS stereoOn (no special-case header
    // chrome); Master is the new XS class (a single knob). Demonstrates "everything is a
    // module" before we formalise FR14 / the XS size class via correct-course.
    // COMPRESSOR: master-bus glue (runs on the summed mix in processBlock). id "compressor".
    // MASTER BUS — spec-driven (Modules::*). See Source/Modules/*Specs.h.
    // PRESETS quick-access bank (F1..F12): a custom panel the editor owns (like the scope/keyboard),
    // injected as a Display. The 12 slot assignments are a GLOBAL app setting (PresetIO), not
    // per-preset; they are loaded here and re-persisted whenever a slot is (re)assigned. Added
    // FIRST so it is the left-most MASTER BUS module (it is left-aligned; the others hug the right).
    {
        auto* panel = new PresetBankPanel();
        presetBank = panel;
        rackOwned.add(panel);
        presetSlots = PresetIO::loadPresetBanks();
        // Defensive: enforce uniqueness in case the file ever holds duplicates (hand-edited / older).
        // Keep the first occurrence, clear later ones; self-heal the file if anything changed.
        bool deduped = false;
        for (int a = 0; a < (int) presetSlots.size(); ++a)
            if (presetSlots[(size_t) a].isNotEmpty())
                for (int b = a + 1; b < (int) presetSlots.size(); ++b)
                    if (presetSlots[(size_t) b] == presetSlots[(size_t) a])
                    { presetSlots[(size_t) b].clear(); deduped = true; }
        if (deduped) PresetIO::savePresetBanks(presetSlots);
        panel->setAllAssignments(presetSlots);
        panel->onLoadSlot   = [this](int i) { triggerPresetSlot(i); };
        panel->onAssignSlot = [this](int i) { assignPresetSlot(i); };
        add(Rack::Zone::MasterBus, SizeClass::W8H1, ModuleType::Processor, "PRESETS",
            P::presetBankOn, { Display{ panel, 8 } }, [] {});
    }

    addRackModule(makeModuleDescriptor(Modules::compressor()));
    // STEREO — spec-driven; the editor injects the per-knob relevance predicates (they read apvts
    // atomics, which a static spec can't capture). WIDTH/TIME drive the Haas widener, which ONLY
    // runs in Pseudo-Stereo; ROOM drives the Kunstkopf early-reflection stage (Story 10.4), which
    // ONLY runs in Kunstkopf — in every other output mode the DSP ignores them, so the knobs are
    // greyed out there instead of sitting there looking live.
    {
        auto d = makeModuleDescriptor(Modules::stereo());
        auto* mode = apvts.getRawParameterValue (P::outputMode);
        auto pseudoOnly    = [mode] { return (int) mode->load() == (int) OutputMode::PseudoStereo; };
        auto kunstkopfOnly = [mode] { return (int) mode->load() == (int) OutputMode::Kunstkopf; };
        for (auto& el : d.body)
            if (auto* k = std::get_if<Knob> (&el))
            {
                if (k->paramId == P::stereoWidth || k->paramId == P::stereoTime)
                    k->activeWhen = pseudoOnly;
                else if (k->paramId == P::hrtfRoom)
                    k->activeWhen = kunstkopfOnly;
            }
        addRackModule(std::move(d));
    }
    addRackModule(makeModuleDescriptor(Modules::master()));

    // ---- GENERATORS ----
    // OSC 1-3 — spec-driven (OscSpecs.h). FREQ display-transform + AMP ring come from the spec.
    for (int i = 1; i <= 3; ++i)
        addRackModule(makeModuleDescriptor(Modules::osc(i)));
    // CROSS MOD now lives in the MODULATION zone (it shapes the oscillators, it is not a source).
    // Added below with the other modulators so its default within-zone position is natural.

    addRackModule(makeModuleDescriptor(Modules::sub()));
    addRackModule(makeModuleDescriptor(Modules::noise()));
    add(Rack::Zone::Generators, SizeClass::W8H1, ModuleType::Generator, "STRING - KARPLUS", P::karplusOn,   // W8: PLUCK + 5 knobs incl. PAN
        { Action{ "PLUCK", [this] { processor.pluckString(); }, {} },
          K(P::karplusFreq, "FREQ"),
          K(P::karplusDamping, "DAMP"), K(P::karplusStretch, "STR"),
          K(P::karplusAmp, "AMP"),   // AMP·PAN grouped last as the output stage (rack-wide convention)
          Kmod(P::karplusPan, "PAN", ModTarget::KarplusPan) });   // Epic 10: stereo placement + auto-pan target
    add(Rack::Zone::Generators, SizeClass::W10H1, ModuleType::Generator, "WAVETABLE", P::wavetableOn,   // W10: BANK+LOAD+7 knobs incl. FB+PAN (9 slots of 10)
        { Combo{ P::wavetableBank, "BANK",
                 std::function<juce::StringArray()>([] { return WavetableBankStore::instance().getNames(); }) },
          FileAction{ "LOAD WAV",
                      [this] (juce::File f)
                      {
                          int idx = WavetableBankStore::instance().loadWav(f);
                          if (idx >= 0)
                              if (auto* pr = processor.getAPVTS().getParameter(P::wavetableBank))
                                  pr->setValueNotifyingHost(pr->convertTo0to1((float) idx));
                      },
                      { juce::String(P::wavetableBank) },   // refresh the BANK combo after load
                      PresetIO::wavetablesFolder(), "*.wav" },   // open in the shipped examples folder
          Kmod(P::wavetablePosition, "POS", ModTarget::WavetablePosition), Kmod(P::wavetableFreq, "FREQ", ModTarget::WavetableFreq),
          Kmod(P::wavetableUniVoices, "VOICES", ModTarget::WavetableVoices), Kmod(P::wavetableUniDetune, "DETUNE", ModTarget::WavetableDetune),
          Kmod(P::wavetableFeedback, "FB", ModTarget::WavetableFeedback),   // Self-FM depth (body order matches OSC: … DETUNE, FB, AMP, PAN)
          Kmod(P::wavetableAmp, "AMP", ModTarget::WavetableAmp),   // AMP·PAN grouped last as the output stage (rack-wide convention)
          Kmod(P::wavetablePan, "PAN", ModTarget::WavetablePan) },   // Epic 10: stereo placement + auto-pan target
        [] { WavetableBankStore::instance().resetToBuiltIns(); });   // ↺ drops user-loaded banks → back to the standard list
    // SAMPLER (Story 12.1) — recordings as a generator: dynamic SET combo over the session's
    // SampleBankStore + LOAD (which COPIES the file into %AppData%\JASS\Samples so presets can
    // re-resolve it by name). Hand-built body like WAVETABLE; default-HIDDEN like COMPRESSOR
    // (show via MODULES) so the GENERATORS zone stays compact.
    {
        ModuleDescriptor d;
        d.sizeClass = SizeClass::W12H1; d.type = ModuleType::Generator;
        d.id = "sampler"; d.title = "SAMPLER";
        d.defaultZone = Rack::Zone::Generators;
        d.defaultVisible = true;    // visible from the start (user decision) — but disabled until switched on
        d.enableParam = P::samplerOn;
        // SET selects a STORE INDEX: bind by item index (indexIsValue), NOT via ComboBoxAttachment —
        // that maps item positions across the whole 0..31 range and lands on the wrong sample
        // (the classic combo-index bug class; found by ear: "Brass" played CH_01).
        Combo setCombo{ P::samplerSet, "SET",
                        std::function<juce::StringArray()>([] { return SampleBankStore::instance().getNames(); }) };
        setCombo.indexIsValue = true;
        // Standard width (2 slots), NOT the 3 it used to claim for long set names like
        // "SalamanderPiano" (user 2026-08-09: "beim SAMPLER ist die Combo noch zu breit").
        // Since every combo is now capped at one width, a wider one just stands out. The
        // full name still shows in the drop-down list; only the closed box may clip it.
        // Side benefit: one content slot back makes the SAMPLER cells - and its knobs -
        // a little larger (52 -> 56 px; still under the standard 62, accepted by the user).
        // A mapped (multisample) set is a chromatic INSTRUMENT — Loop mode starts its notes at
        // the shared loop phase, i.e. mid-sample, which no piano survives; and STRETCH buys
        // nothing when every zone transposes at most a couple of semitones, while costing CPU,
        // 60 ms engine lead and phase-vocoder character. When the USER picks a mapped set,
        // MODE follows to One-Shot and STRETCH switches off (requests 2026-08-04); preset loads
        // stay untouched (programmatic combo updates don't fire onUserSelect), and picking a
        // single-sample set never flips anything back — loops keep their setting.
        auto oneShotForMappedSet = [this] (int setIdx)
        {
            // Picking a set by hand ends any pending restore (12.6): the user's choice is the
            // one that must be saved, not the set a preset was still waiting for.
            PresetIO::pendingSamplerSetName.clear();
            const auto* s = SampleBankStore::instance().getSet (setIdx);
            if (s != nullptr && s->isMapped())
            {
                if (auto* pm = processor.getAPVTS().getParameter (P::samplerMode))
                    pm->setValueNotifyingHost (pm->convertTo0to1 (0.0f));   // One-Shot
                if (auto* ps = processor.getAPVTS().getParameter (P::samplerStretch))
                    ps->setValueNotifyingHost (0.0f);                        // STRETCH off
                // An instrument wants a note-off fade everywhere: lift REL off 0 so zones the
                // .sfz left without ampeg_release (Splendid's mid range) fade too. Only from 0 —
                // a deliberately set knob value survives the set switch; zones with own values
                // ignore REL either way. (user request 2026-08-04)
                if (auto* pr = processor.getAPVTS().getParameter (P::samplerRelease))
                    if (pr->getValue() <= 0.0f)
                        pr->setValueNotifyingHost (pr->convertTo0to1 (2.16f));   // user's pick (by ear)
                // ...and its tail must not be cut by the global ADSR: switch the ENVELOPE module
                // off, but ONLY when the sampler is the sole active generator — in a hybrid
                // preset the envelope belongs to the other generators too, and yanking it would
                // gut the whole voice. Cross-module, but through APVTS only (AD-9); precedent:
                // the mod matrix auto-enables source+target modules. (user request 2026-08-04)
                auto isOn = [this] (const juce::String& id)
                { return *processor.getAPVTS().getRawParameterValue (id) > 0.5f; };
                const bool otherGenOn = isOn (P::oscOn (1)) || isOn (P::oscOn (2)) || isOn (P::oscOn (3))
                                     || isOn (P::subOn) || isOn (P::noiseOn)
                                     || isOn (P::karplusOn) || isOn (P::wavetableOn);
                if (! otherGenOn)
                {
                    if (auto* pa = processor.getAPVTS().getParameter (P::adsrOn))
                        pa->setValueNotifyingHost (0.0f);   // sampler governs its own tail (12.4)
                    // ...and the output stage must not colour it: Stereo-Pan is the one mode
                    // that renders a stereo recording untouched (hard L/R). Kunstkopf/Binaural
                    // re-spatialise through HRIR/ITD — audibly wrong on a piano (user 2026-08-04);
                    // Mono/Pseudo-Stereo sum the mic channels (comb). Same guard: sole generator.
                    if (auto* po = processor.getAPVTS().getParameter (P::outputMode))
                        po->setValueNotifyingHost (po->convertTo0to1 ((float) (int) OutputMode::StereoPan));
                }
            }
        };
        setCombo.onUserSelect = oneShotForMappedSet;
        // Shared by LOAD (file or .sfz) and FOLDER (12.2): import via PresetIO (copies into
        // %AppData%\JASS\Samples for preset portability), select on success, explain on failure.
        auto importSource = [this, oneShotForMappedSet] (juce::File f)
        {
            juce::String err;
            const int idx = PresetIO::importSamplerSource(f, &err);
            if (idx >= 0)
            {
                if (auto* pr = processor.getAPVTS().getParameter(P::samplerSet))
                    pr->setValueNotifyingHost(pr->convertTo0to1((float) idx));
                oneShotForMappedSet(idx);   // importing an instrument is a user gesture too
            }
            else   // name the culprit — a generic limits lecture forces the user to guess
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    "SAMPLER", "Could not load \"" + f.getFileName() + "\": " + err);
        };
        // ROOT applies only to single samples — a mapped (multisample) set carries its own root
        // per zone, so the knob dims (per-knob relevance, like STEREO's WIDTH/TIME).
        // 12.4: the sampler's own note-off fade — the fallback for zones whose .sfz brought no
        // ampeg_release (those always win). When the loaded set carries one on EVERY zone, the
        // knob cannot change anything: it then dims like ROOT does for mapped sets, and — user
        // request 2026-08-08 — displays the release the instrument actually plays instead of its
        // own inert value. Decoupled display (toDisplay/fromDisplay), so the shown number follows
        // the SET; fromDisplay is identity because a dimmed knob cannot be dragged anyway.
        Knob relKnob = K(P::samplerRelease, "REL");
        auto currentSampleSet = [this]
        {
            return SampleBankStore::instance().getSet(
                static_cast<int>(*processor.getAPVTS().getRawParameterValue(P::samplerSet)));
        };
        relKnob.activeWhen = [currentSampleSet]
        {
            const auto* s = currentSampleSet();
            return s == nullptr || ! s->ownsRelease();
        };
        relKnob.toDisplay = [currentSampleSet](double base, double)
        {
            const auto* s = currentSampleSet();
            return (s != nullptr && s->ownsRelease()) ? (double) s->typicalRelease() : base;
        };
        relKnob.fromDisplay = [](double shown, double) { return shown; };

        Knob rootKnob = K(P::samplerRoot, "ROOT");
        rootKnob.activeWhen = [this]
        {
            const auto* s = SampleBankStore::instance().getSet(
                static_cast<int>(*processor.getAPVTS().getRawParameterValue(P::samplerSet)));
            return s == nullptr || ! s->isMapped();
        };
        // ROOT is a key, so it reads as one: the box says "C4", the hover adds the MIDI number
        // ("C4 · 60") — same convention as STEP SEQ and PERC NOTE (maintainer 2026-08-18).
        // The stored value stays the note number; typing a number into the box still works.
        rootKnob.textFromValue = [](double v)
        { return juce::MidiMessage::getMidiNoteName(juce::roundToInt(v), true, true, 4); };
        rootKnob.tooltipFromValue = [](double v)
        {
            const int note = juce::roundToInt(v);
            return juce::MidiMessage::getMidiNoteName(note, true, true, 4)
                 + juce::String(" · ") + juce::String(note);
        };
        d.body = {
            setCombo,
            FileAction{ "LOAD", importSource,
                        { juce::String(P::samplerSet) },   // refresh the SET combo after load
                        PresetIO::samplesFolder(), juce::String(SampleMapping::kAudioWildcard) + ";*.sfz" },
            // 12.2: import a whole folder as ONE multisample set (mapping derived from filenames
            // "Name_C3.wav" — or from an .sfz found inside the folder).
            FileAction{ "FOLDER", importSource,
                        { juce::String(P::samplerSet) },
                        PresetIO::samplesFolder(), "*", /*pickDirectory*/ true },
            Combo{ P::samplerMode, "MODE", juce::StringArray{ "One-Shot", "Loop", "Reverse", "Rev-Loop" } },
            // 12.3: pitch/time decoupling — grouped right beside MODE (both choose the playback
            // regime); renders caption-above like the knobs (review feedback 2026-08-04).
            Toggle{ P::samplerStretch, "STRETCH" },
            rootKnob, K(P::samplerStart, "START"), K(P::samplerEnd, "END"),
            K(P::samplerSpeed, "SPEED"),
            relKnob,
            Kmod(P::samplerLevel, "AMP", ModTarget::SamplerLevel),   // renamed from LEVEL (generator standard)
            Kmod(P::samplerPan,   "PAN",   ModTarget::SamplerPan) };
        addRackModule(std::move(d));
    }

    // ---- MODULATION ----
    // ADSR: the second unit-row is the REAL EnvelopeDisplay (attack→decay→sustain→release
    // curve), a Display body element (AD-5), owned by rackOwned so its lifetime is tied
    // to the editor.
    add(Rack::Zone::Modulation, SizeClass::W4U7, ModuleType::Modulator, "ENVELOPE - ADSR", P::adsrOn,
        { K(P::attack, "ATK"), K(P::decay, "DEC"), K(P::sustain, "SUS"), K(P::release, "REL"),
          Display{ rackOwned.add(new EnvelopeDisplay(apvts, juce::Colour(0xff22d3ee))), 4 } });
    // LFOs (indexed), ARP, GLIDE, PITCH ENV, MOD MATRIX — spec-driven. LFO 1 visible (id "lfo");
    // further LFOs hidden by default. MOD MATRIX builds its 4 SRC·DEST·AMT rows from the spec.
    for (int i = 1; i <= kNumLFOs; ++i)
    {
        auto d = makeModuleDescriptor(Modules::lfo(i));
        greyWhenSynced(d, P::lfoRate(i), P::lfoSyncDiv(i));
        addRackModule(std::move(d));
    }
    addRackModule(makeModuleDescriptor(Modules::arpeggiator()));
    // STEP SEQ (Story 15.1) — spec-driven; the editor only adds the mutual exclusion with the ARP.
    // Both REPLACE the held chord, so both running at once is not a thing; rather than a silent
    // precedence rule that leaves a lit ARP doing nothing, switching one on switches the other off,
    // so the rack always shows the truth. (The processor enforces the precedence as well, for the
    // case where a preset arrives with both set.)
    {
        auto d = makeModuleDescriptor(Modules::stepSeq());
        // Pin each step's on/off into the corner of its own pitch knob. The switch params are
        // declared showInBody=false so they claim no grid cell; ModuleFrame draws them top-right
        // and greys the knob when a step is off, through the same path as any inactive control.
        // ... and let each step sound while it is being edited (Story 15.3): a semitone offset is
        // a number, not a note, so writing a figure without hearing it is guesswork. The hook is
        // injected here because only the editor knows the keyboard state and the current octave.
        // ... and let the KEYBOARD write the figure (Story 15.4): each step knob knows its own index,
        // so touching it moves the write cursor there (AC7) and the ring marks where the next played
        // note will land. Both hooks are UI state the spec cannot express — only the editor owns the
        // cursor, the keyboard state and the current octave.
        for (auto& el : d.body)
            if (auto* k = std::get_if<Knob>(&el))
                if (k->paramId.startsWith("seqPitch"))
                {
                    const int step = k->paramId.substring(8).getIntValue();   // 1-based
                    k->toggleParamId = "seqStep" + k->paramId.substring(8);
                    k->audition = [this, step](int semis, bool sounding)
                    {
                        if (sounding) seqSetCursor(step - 1);   // a click selects, exactly as 15.3 sounds
                        auditionStep(semis, sounding);
                    };
                    k->highlightWhen = [this, step] { return seqCursor == step - 1; };
                    // …and mark the step the pattern is ON, the way PERC's grid marks its column
                    // (maintainer 2026-08-11). Ring = where writing goes, dot = what is sounding.
                    k->playingWhen = [this, step] { return processor.getSeqStep() == step - 1; };
                    // …and read out the NOTE, not the raw semitone count (maintainer 2026-08-18):
                    // the value stays the offset from the keyboard's current C, the display simply
                    // resolves it through the SAME reference audition sounds it with — box, preview
                    // and played figure always agree, and the octave keys re-text the boxes
                    // (keyPressed → rackBody->refreshNamedReadouts). 60 = C4, as everywhere in JASS.
                    k->textFromValue = [this](double v)
                    {
                        const int note = juce::jlimit(0, 127, 12 * kbBaseOctave + juce::roundToInt(v));
                        return juce::MidiMessage::getMidiNoteName(note, true, true, 4);
                    };
                    // Hover adds the MIDI number the box has no room for ("E1 · 40") — for
                    // transcribing from templates that count in numbers (maintainer 2026-08-18).
                    k->tooltipFromValue = [this](double v)
                    {
                        const int note = juce::jlimit(0, 127, 12 * kbBaseOctave + juce::roundToInt(v));
                        return juce::MidiMessage::getMidiNoteName(note, true, true, 4)
                             + juce::String(" · ") + juce::String(note);
                    };
                }
        // The reset ↺ empties the pattern and arms step entry at step 1 (AC1): the button that
        // clears a figure is precisely the moment one wants to fill it again, so recording needs no
        // control of its own. doReset() writes the defaults first and calls this after.
        d.onReset = [this] { seqSetCursor(0); };
        greyWhenSynced(d, P::seqRate, P::seqSync);
        addRackModule(std::move(d));
    }
    // PERC (Story 16.1) — the drum grid next to the note sequencer. Two things the spec cannot
    // declare: the step field is a painted component (128 switches would be six rack units at the
    // grid's 62 px cell), and the KIT list is dynamic, exactly like the SAMPLER's SET.
    {
        auto d = makeModuleDescriptor(Modules::perc());
        auto* grid = rackOwned.add(new PercGrid(apvts,
                                                [this] { return processor.getPercStep(); },
                                                [this](int lane) { processor.auditionPercLane(lane); }));
        // The grid goes FIRST so it fills row 1 and the controls wrap into row 2 beneath it. Cells
        // fill in body order, so this one insert is the whole layout.
        d.body.insert(d.body.begin(), Display{ grid, 19 });
        // Item 0 is "(no kit)", so the value is the store index PLUS ONE and a fresh PERC points at
        // nothing instead of at whatever sample sits at index 0 — which on this machine was
        // "Drums_110BPM", a one-shot loop offered as a drum kit (maintainer 2026-08-11).
        // Only MAPPED sets are offered: a single WAV would put the same recording on all four lanes
        // at four pitches, which is never what a drum track wants (maintainer 2026-08-11). The
        // filtering is why the combo carries an explicit value list — its positions are no longer
        // store indices, and a value that quietly follows the list is the oldest bug in this rack.
        Combo kitCombo{ P::percKit, "KIT",
                        std::function<juce::StringArray()>([]
                        {
                            juce::StringArray items { "(no kit)" };
                            const auto& store = SampleBankStore::instance();
                            for (int i = 0; i < store.getNumSets(); ++i)
                                if (const auto* s = store.getSet(i); s != nullptr && s->isMapped())
                                    items.add(s->getName());
                            return items;
                        }) };
        kitCombo.itemValues = []
        {
            juce::Array<int> values { 0 };   // 0 = "(no kit)"
            const auto& store = SampleBankStore::instance();
            for (int i = 0; i < store.getNumSets(); ++i)
                if (const auto* s = store.getSet(i); s != nullptr && s->isMapped())
                    values.add(i + 1);       // the parameter stays store index + 1
            return values;
        };
        kitCombo.indexIsValue = true;   // ... with itemValues supplying the value per position
        d.body.insert(d.body.begin() + 1, kitCombo);
        // NOTE reads out the instrument (decision B, 2026-08-10): the zone's own name if the kit
        // brought one, else the General MIDI drum map, else the note name. The MIDI number rides
        // along ("Kick · 36", 2026-08-18) so a GM drum-map template transfers without guessing.
        for (auto& el : d.body)
            if (auto* k = std::get_if<Knob>(&el))
                if (k->paramId.startsWith("percNote"))
                    k->textFromValue = [this](double v)
                    {
                        const auto* kit = SampleBankStore::instance().getSet(
                            static_cast<int>(*processor.getAPVTS().getRawParameterValue(P::percKit)) - 1);
                        return PercNames::forNote(kit, (int) v) + juce::String(" · ") + juce::String((int) v);
                    };
        greyWhenSynced(d, P::percRate, P::percSync);
        addRackModule(std::move(d));
    }
    addRackModule(makeModuleDescriptor(Modules::glide()));
    addRackModule(makeModuleDescriptor(Modules::pitchEnv()));
    addRackModule(makeModuleDescriptor(Modules::chaos()));   // LFO expansion — Lorenz mod source
    // CROSS MOD — spec-driven; the editor injects the derived lit/dim predicate (reads apvts atomics,
    // which a static spec can't capture). Lit = mixModeOn AND both SELECTED operand OSCs enabled.
    {
        auto d = makeModuleDescriptor(Modules::crossmod());
        auto* selA = apvts.getRawParameterValue (P::mixSrcA);
        auto* selB = apvts.getRawParameterValue (P::mixSrcB);
        std::array<std::atomic<float>*, 3> oscOn { apvts.getRawParameterValue (P::oscOn (1)),
                                                   apvts.getRawParameterValue (P::oscOn (2)),
                                                   apvts.getRawParameterValue (P::oscOn (3)) };
        d.enabledWhen = [selA, selB, oscOn]
        {
            const int a = juce::jlimit (0, 2, (int) selA->load());
            const int b = juce::jlimit (0, 2, (int) selB->load());
            return oscOn[(size_t) a]->load() >= 0.5f && oscOn[(size_t) b]->load() >= 0.5f;
        };
        addRackModule(std::move(d));
    }
    // MOD MATRIX — APVTS params are spec-driven (ModMatrixSpecs.h), but the BODY is hand-built here:
    // each slot's PARAM combo is DEPENDENT on its MODULE selection (which params exist depends on the
    // picked module), and a static spec can't read APVTS. The editor supplies the per-slot provider +
    // ComboDependency (clamp PARAM if out of range, then re-list) where apvts is available.
    {
        ModuleDescriptor d;
        // Full rack width since QUANT: a fifth control per slot makes 24 cells per row, and the
        // two columns W28 saved would push the combos below ~52 px. W30 keeps them at ~55 —
        // still below the 62 px knob guideline, but the only knob (AMT) spans 2 cells and is
        // capped by the row height anyway, so only combo text pays (the popup shows full names).
        // Height: 7 quarter units = 207 px instead of 238 (Story 7.4). Two rows at the standard knob
        // size, captions and value boxes intact — the maintainer's call: "Beschriftungen werden NICHT
        // geopfert". (176 px is reachable, but only by dropping the repeated captions.)
        d.sizeClass = SizeClass::W30U7; d.type = ModuleType::Modulator;   // 8 slots (4/row × 2),
        d.id = "modmatrix"; d.title = "MOD MATRIX"; d.defaultZone = Rack::Zone::Modulation;   // roomy combos + knobs
        d.enableParam = P::modMatrixOn;

        const juce::StringArray srcItems { "LFO 1", "Envelope", "Velocity", "LFO 2", "LFO 3", "LFO 4", "Chaos X", "Chaos Y" };   // == ModSource
        juce::StringArray modItems;
        for (int i = 0; i < ModDest::kNumModules; ++i) modItems.add (ModDest::moduleLabel (i));   // == ModDest order

        for (int n = 1; n <= ModMatrixConfig::kNumSlots; ++n)
        {
            const juce::String modId = P::modSlotModule (n);
            const juce::String parId = P::modSlotParam  (n);
            d.body.push_back (C (P::modSlotSource (n), "SRC", srcItems));
            d.body.push_back (C (modId,                "MOD", modItems));
            // PARAM: the params of whichever MODULE this slot currently selects (re-listed on change).
            // indexIsValue: the selected item index IS the param value (0/1/2), so a 2-item module
            // maps its 2nd entry to 1 (not the ComboBoxAttachment's index/(numItems-1) mismap).
            Combo paramCombo { parId, "PARAM",
                std::function<juce::StringArray()> ([this, modId]
                {
                    const int m = (int) processor.getAPVTS().getRawParameterValue (modId)->load();
                    juce::StringArray items;
                    for (int p = 0; p < ModDest::numParams (m); ++p) items.add (ModDest::paramLabel (m, p));
                    return items;
                }) };
            paramCombo.indexIsValue = true;
            d.body.push_back (paramCombo);
            // AMT takes TWO slots (Story 7.5): a 1-slot cell here is 62 px wide but 100 px tall, and
            // a knob is capped by the narrower side — so the row's height went to waste next to a
            // small knob. Two slots make the cell 108 px and the knob reaches the 65 px the height
            // offers. The module keeps its 28 columns; the combos give up 2 px of width for it.
            // QUANT: per-slot scale mask for pitch routings (Off/Chrom/Major/Minor/Penta) — a
            // stepped source (S&H, Chaos) on FREQ becomes a melody instead of detune. Items match
            // the spec's Choice strings (attachment maps by index; kept short to fit the cell).
            // Placed BEFORE AMT: the knob stays the routing's LAST control, so the green activity
            // dot keeps anchoring beside AMT (paintOverChildren anchors to the group's final cell)
            // and the four combos read as one cluster. The mask only acts on FREQ routings, so on
            // any other target the combo is greyed via activeWhen (maintainer, 2026-08-18).
            Combo quantCombo = C (P::modSlotQuant (n), "QUANT",
                                  juce::StringArray { "Off", "Chrom", "Major", "Minor", "Penta" });
            auto* qMod = apvts.getRawParameterValue (modId);
            auto* qPar = apvts.getRawParameterValue (parId);
            quantCombo.activeWhen = [qMod, qPar]
            {
                return ModDest::targetOf ((int) qMod->load(), (int) qPar->load()) == LFOTarget::Frequency;
            };
            d.body.push_back (quantCombo);
            Knob amt = K (P::modSlotAmount (n), "AMT");
            amt.slots = 2;
            d.body.push_back (amt);
            // MODULE changed → if PARAM is now beyond the new module's param count, snap it back to 0.
            d.comboDeps.push_back (ComboDependency { modId, parId,
                [this, parId] (int newModule)
                {
                    auto& a = processor.getAPVTS();
                    if (auto* pp = a.getParameter (parId))
                        if ((int) a.getRawParameterValue (parId)->load() >= ModDest::numParams (newModule))
                            pp->setValueNotifyingHost (pp->convertTo0to1 (0.0f));
                } });
        }
        // Per-slot activity highlight: a slot is "active" when its MOD combo != Off (index 0). The
        // frame dims inactive slots and draws a lit dot on active ones (groupSize 5 =
        // SRC/MOD/PARAM/AMT/QUANT — the group ALSO drives the row layout's gap logic, so a stale
        // count here shifts every fifth control and clips the row's right edge).
        d.slotActivity.groupSize = 5;
        d.slotActivity.isActive  = [this] (int slot)
        {
            return (int) processor.getAPVTS().getRawParameterValue (P::modSlotModule (slot + 1))->load() != 0;
        };
        addRackModule (std::move (d));
    }

    // ---- PROCESSING (spec-driven) ----
    addRackModule(makeModuleDescriptor(Modules::filter()));
    // PROCESSING — spec-driven (Modules::*). Display-only combo labels (e.g. DISTORTION
    // "Soft Clip") live in the spec's displayChoices; canonical strings stay in choices.
    addRackModule(makeModuleDescriptor(Modules::formant()));
    addRackModule(makeModuleDescriptor(Modules::distortion()));
    addRackModule(makeModuleDescriptor(Modules::wavefold()));
    addRackModule(makeModuleDescriptor(Modules::bitcrush()));
    addRackModule(makeModuleDescriptor(Modules::phaser()));
    addRackModule(makeModuleDescriptor(Modules::chorus()));
    {
        // DELAY's free-running knob is TIME rather than RATE, but the relationship is identical.
        auto d = makeModuleDescriptor(Modules::delay());
        greyWhenSynced(d, P::delayTime, P::delaySyncDiv);
        addRackModule(std::move(d));
    }
    addRackModule(makeModuleDescriptor(Modules::reverb()));
    // Real visualizers (own instances, separate from the legacy ones behind the rack —
    // one-parent rule). setShowTitle(false): the module header already shows the title.
    // Sharing the one WaveformCapture across instances is safe (updateSnapshot is idempotent
    // per frame). Sample rate reaches them via the capture (set in prepareToPlay).
    auto* scope = new WaveformDisplay(processor.getWaveformCapture());
    scope->setShowTitle(false);
    scope->setEnableSource(apvts.getRawParameterValue(P::scopeOn));   // scopeOn off => freeze+blank
    rackOwned.add(scope);
    // visualOnly: the two VISUALIZATION modules are the only ones in the rack that nothing can be
    // heard from, so hiding them frees their height for good (see ModuleDescriptor::visualOnly).
    // Together they are one row of 2 units plus the zone header — measured at 286 px (1732 → 1446
    // of a 1929 px budget), which is what makes room for another module of that size without
    // touching the readability floor.
    add(Rack::Zone::Visualization, SizeClass::W12H2, ModuleType::Processor, "OSCILLOSCOPE", P::scopeOn,
        { Display{ scope, 12 } },
        [scope] { scope->resetTimeRange(); },   // ↺ restores the 10 ms default time-base
        /*visualOnly*/ true);
    auto* spec = new SpectrumDisplay(processor.getWaveformCapture());
    spec->setShowTitle(false);
    spec->setEnableSource(apvts.getRawParameterValue(P::spectrumOn));   // spectrumOn off => freeze+blank
    rackOwned.add(spec);
    // SPECTRUM has no adjustable state yet — its ↺ is a uniform-anatomy placeholder (a no-op
    // for now) so every module carries the same header controls; wire real params here later.
    add(Rack::Zone::Visualization, SizeClass::W12H2, ModuleType::Processor, "SPECTRUM", P::spectrumOn,
        { Display{ spec, 12 } },
        [] { },
        /*visualOnly*/ true);

    // The on-screen keyboard is itself a module (INPUT zone) so it can be hidden like any
    // other — e.g. when playing via an external MIDI keyboard. It wraps the existing keyboard
    // as a Display (AD-5; the editor owns its lifetime). enableParam keyboardOn is a UI-only
    // placeholder (drives only the dim state for now; the keyboard stays playable). The onReset
    // is a no-op placeholder so the header carries the uniform reset ↺ like every other module.
    // Full-width single row (W30H1). Its own info icon carries the play/shortcut help.
    add(Rack::Zone::Input, SizeClass::W30H1, ModuleType::Generator, "KEYBOARD", P::keyboardOn,
        { Display{ keyboard.get(), 24 } },
        [] { });

    // Added LAST so the opaque rack covers the legacy body; the header chrome stays live.
    addAndMakeVisible(*rackBody);
}

void SynthyEditor::resized()
{
    auto area = getLocalBounds().reduced(12);

    // ===== Header — Save/Load/Random/Reset left, Title + Current State center =====
    // (Master + Stereo moved out of the header into the MASTER BUS rack zone.) The header
    // is kept compact; the freed space gives the title + preset name room to breathe.
    auto headerRow = area.removeFromTop(64);
    // The title is centred over the FULL header width so "J A S S" sits in the true middle
    // of the window; the left cluster only overlays the left edge, clear of the centred text.
    g_titleBounds = headerRow;
    spinningTitle.setBounds(g_titleBounds);   // animated wordmark spans the full header row
    // MODULES show/hide menu button overlays the right edge (clear of the centred title).
    modulesBtn.setBounds(headerRow.removeFromRight(120).reduced(8, 17));
    // Help-language selector sits just left of the MODULES button (Story 6.1).
    langBox.setBounds(headerRow.removeFromRight(66).reduced(4, 20));
    // Left cluster: the file buttons (SAVE/LOAD/DELETE on top, RANDOM/RESET below) AND the
    // current-preset name belong together (the preset name is about what was loaded/saved).
    // Three columns since DELETE joined; the block widened so no button got smaller.
    auto leftGroup = headerRow.removeFromLeft(415);
    auto leftBtns = leftGroup.removeFromLeft(225);
    auto row1 = leftBtns.removeFromTop(30);
    saveBtn.setBounds(row1.removeFromLeft(75).reduced(3, 2));
    loadBtn.setBounds(row1.removeFromLeft(75).reduced(3, 2));
    deleteBtn.setBounds(row1.reduced(3, 2));
    auto row2 = leftBtns.removeFromTop(30);
    randomBtn.setBounds(row2.removeFromLeft(row2.getWidth() / 2).reduced(3, 2));
    resetBtn.setBounds(row2.reduced(3, 2));
    leftGroup.removeFromLeft(10);
    presetNameLabel.setBounds(leftGroup);   // grouped with the load/save controls
    area.removeFromTop(8);

    // The whole body below the header row belongs to the rack — the on-screen keyboard is
    // now a rack module (INPUT zone), no longer a separate bottom band.
    juce::ignoreUnused(area);
    if (rackBody)
    {
        auto rb = getLocalBounds().reduced(12);
        rb.removeFromTop(64 + 8);    // header row + gap (mirrors the header band above)
        rackBody->setBounds(rb);
    }
}
