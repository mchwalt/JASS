#include "PluginEditor.h"
#include "../DSP/WavetableBank.h"
#include "HelpTextStore.h"           // embedded EN/DE help texts (Story 6.1)
#include "../Audio/PresetIO.h"
#include "../Audio/Parameters.h"   // Parameters::ID for the rack
#include <map>
#include <memory>
#include <vector>
#include <utility>
#include <array>

// SynthyLookAndFeel now lives in Source/UI/rack/SynthyLookAndFeel.{h,cpp} (AD-7) —
// the rack framework owns the single shared look.

// --- Rack customization panel (Story 4.2) -----------------------------------
// A reorderable list of every module, grouped by zone. A left checkbox toggles
// visibility; dragging a module row reorders it (list order = on-screen order) and
// dragging it across a zone header moves it to that zone. All mutations go through the
// Rack API (setModuleVisible / setZoneVisible / applyLayoutOrder) — the single layout()
// path re-packs and the editor re-fits height (AD-10/AD-12). Shown in a CallOutBox.
namespace
{
    class RackCustomizePanel : public juce::Component
    {
    public:
        explicit RackCustomizePanel (rack::Rack& r) : rack (r)
        {
            rebuildRows();
            addAndMakeVisible (resetBtn);
            resetBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff334155));
            resetBtn.onClick = [this] { rack.resetLayout(); rebuildRows(); repaint(); };
            setSize (kW, listHeight() + kBtnH);
        }

        void resized() override
        {
            resetBtn.setBounds (juce::Rectangle<int> (0, listHeight(), getWidth(), kBtnH).reduced (6, 5));
        }

        int listHeight() const { return juce::jmax (kRowH, (int) rows.size() * kRowH); }

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
                    g.setColour (juce::Colours::white.withAlpha (row.visible ? 0.9f : 0.4f));
                    g.setFont (juce::FontOptions (13.0f));
                    g.drawText (row.title, rb.reduced (12, 0), juce::Justification::centredLeft);
                    g.setColour (juce::Colours::white.withAlpha (0.22f));
                    g.drawText ("::", rb.removeFromRight (18), juce::Justification::centred);   // drag hint
                }
                g.setColour (juce::Colours::black.withAlpha (0.30f));
                g.drawHorizontalLine (i * kRowH, 0.0f, (float) getWidth());
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
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
            if (! row.header) dragIndex = i;   // only module rows are draggable
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
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
        struct Row { bool header; rack::Rack::Zone zone; juce::String id, title; bool visible; };

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
                rows.push_back ({ true, z, {}, {}, true });
                for (const auto& m : rack.modulesInZone (z))
                    rows.push_back ({ false, z, m.id, m.title, m.visible });
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
        std::vector<Row> rows;
        juce::TextButton resetBtn { "Reset layout" };
        int dragIndex = -1;
        static constexpr int kW = 260, kRowH = 26, kBtnH = 30;
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

SynthyEditor::SynthyEditor(SynthyProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lnf);

    // Preset Save / Load (shared .synthy JSON)
    addAndMakeVisible(saveBtn);
    addAndMakeVisible(loadBtn);
    saveBtn.onClick = [this]
    {
        presetChooser = std::make_unique<juce::FileChooser>(
            "Save preset", PresetIO::presetsFolder(), "*.synthy");
        auto flags = juce::FileBrowserComponent::saveMode
                   | juce::FileBrowserComponent::canSelectFiles
                   | juce::FileBrowserComponent::warnAboutOverwriting;
        presetChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File{}) return;
            if (! f.hasFileExtension("synthy")) f = f.withFileExtension("synthy");
            PresetIO::saveToFile(processor.getAPVTS(), f, f.getFileNameWithoutExtension());
            processor.markPresetClean();   // current state now matches the saved file
            setPresetName(f.getFileNameWithoutExtension());
        });
    };
    loadBtn.onClick = [this]
    {
        presetChooser = std::make_unique<juce::FileChooser>(
            "Load preset", PresetIO::presetsFolder(), "*.synthy");
        auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        presetChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File{}) return;
            PresetIO::loadFromFile(processor.getAPVTS(), f);
            processor.markPresetClean();   // current state now matches the loaded file
            setPresetName(f.getFileNameWithoutExtension());
            if (rackBody) rackBody->reloadLayoutFromState();   // reflect the loaded layout (Story 4.3)
        });
    };

    addAndMakeVisible(randomBtn);
    randomBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff6d28d9));
    randomBtn.onClick = [this] { processor.randomize(); setPresetName("Random");
                                 if (rackBody) rackBody->enforceHiddenDisabled(); };

    addAndMakeVisible(resetBtn);
    resetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff475569));
    resetBtn.onClick = [this] { processor.resetToDefault(); setPresetName("Init");
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
        // Live-update an already-open panel.
        if (helpPanel && helpPanel->isVisible() && currentHelpId.isNotEmpty() && rackBody)
            if (auto* f = rackBody->moduleById(currentHelpId))
                helpPanel->setContent(f->moduleTitle(),
                                      HelpTextStore::instance().get(currentHelpId, currentLang));
    };
    addAndMakeVisible(langBox);

    // Current-preset display
    presetNameLabel.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    presetNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaab3c0));
    presetNameLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(presetNameLabel);
    setPresetName(processor.getCurrentPresetName());   // restored from LiveState

    // On-screen keyboard (shares the processor's MidiKeyboardState → plays the
    // active generators with full ADSR per note, transposed relative to C4).
    keyboard = std::make_unique<FillWidthKeyboard>(
        processor.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard);
    keyboard->setAvailableRange(21, 108);  // A0 .. C8 (full 88-key piano)
    keyboard->setKeyWidth(20.0f);          // FillWidthKeyboard::resized() spreads keys to fill its width
    keyboard->setKeyPressBaseOctave(kbBaseOctave);
    keyboard->setMidiChannelsToDisplay(1);   // only highlight played (ch.1) notes, not the ch.16 drone
    // Allow playing via the computer keyboard (a, w, s, e, d, ... map to notes;
    // z / x shift the octave; the keyboard must have focus — grabbed on launch/click).
    // The keyboard is added to the rack as a Display module (Input zone) in buildRack();
    // the editor owns its lifetime, the frame parents + sizes it.
    keyboard->setWantsKeyboardFocus(true);
    juce::Component::SafePointer<juce::MidiKeyboardComponent> kbPtr(keyboard.get());
    juce::MessageManager::callAsync([kbPtr]() mutable { if (kbPtr) kbPtr->grabKeyboardFocus(); });

    // The editor itself must NOT grab keyboard focus either (a click on the empty
    // background would otherwise steal it from the keyboard). z / x still reach our
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
        rackBody->onModuleHelp = [this](const juce::String& id) { showModuleHelp(id); };

    // Size the editor: fixed design width, height derived from the rack's VISIBLE content.
    // Re-run on every show/hide via Rack::onLayoutChanged (AD-12). Must be after the rack +
    // all chrome exist so resized() sees every component.
    refitHeight();

    // Drive the OSC FREQ-knob display (played frequency).
    startTimerHz(30);
}

void SynthyEditor::refitHeight()
{
    // Fixed design width; height follows the rack's visible content so the full rack always
    // fits without scrolling. The auto-fit-down transform scales the whole editor on smaller
    // displays. (kBodyTop/kBodyBottom mirror the bands reserved in resized().)
    constexpr int kDesignW    = 1520;
    constexpr int kBodyTop    = 72;   // header row + gap (matches resized())
    constexpr int kBodyBottom = 0;    // no reserved band — the keyboard is a rack module now
    constexpr int kMargin     = 12;   // getLocalBounds().reduced(12)
    const int rackW = kDesignW - 2 * kMargin;
    const int rackH = rackBody ? rackBody->preferredHeight(rackW) : 800;
    const int designH = juce::jmax(1015, rackH + kBodyTop + kBodyBottom + 2 * kMargin);
    setSize(kDesignW, designH);

    if (auto* disp = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto ua = disp->userBounds;        // excludes the taskbar
        const double chrome = 90.0;              // title bar + a little breathing room
        const double sH = (ua.getHeight() - chrome) / (double) designH;
        const double sW =  ua.getWidth()            / (double) kDesignW;
        const double scale = juce::jlimit(0.5, 1.0, juce::jmin(sH, sW));
        // Reset to identity when no scaling is needed, so re-fitting to a shorter rack on a
        // large display clears any transform set for an earlier, taller layout.
        setTransform(scale < 0.999 ? juce::AffineTransform::scale((float) scale)
                                   : juce::AffineTransform());
    }
}

void SynthyEditor::showModulesMenu()
{
    if (! rackBody) return;
    // The reorderable customization list (Story 4.2) in a call-out anchored to the button.
    // Parent = nullptr (desktop) so the editor's auto-fit transform doesn't skew mouse coords.
    auto panel = std::make_unique<RackCustomizePanel>(*rackBody);
    juce::CallOutBox::launchAsynchronously(std::move(panel),
                                           modulesBtn.getScreenBounds(),
                                           nullptr);
}

void SynthyEditor::timerCallback()
{
    double ratio = processor.getCurrentNoteRatio();

    // Live modulation rings: route the current LFO value to whichever knob(s) the
    // LFO targets (0 Frequency, 1 Amplitude, 2 FilterCutoff), but only when the LFO is
    // enabled (lfoOn).
    auto& apvts = processor.getAPVTS();
    float lfo = processor.getLfoDisplayValue();
    bool lfoActive = *apvts.getRawParameterValue("lfoOn") > 0.5f;
    int target = (int) *apvts.getRawParameterValue("lfoTarget");

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
            else if (keyboard->hasKeyboardFocus (true))
                keyboard->giveAwayKeyboardFocus();
        }
    }

    // The live feed drives the rack (AD-8): ONE timer, rack fans out to its frames.
    // ModTarget has a +1 offset vs the raw lfoTarget (ModTarget::None = 0; raw 0 = Frequency).
    if (rackBody)
    {
        const rack::ModTarget activeT = lfoActive ? static_cast<rack::ModTarget>(target + 1)
                                                  : rack::ModTarget::None;
        rackBody->updateLiveFeed(lfoActive, activeT, lfo, ratio);
    }

    // Keep the header label in sync: it reacts both to the (async-restored)
    // preset name and to live edits flipping the "modified" flag.
    updatePresetLabel();
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

bool SynthyEditor::keyPressed(const juce::KeyPress& key)
{
    // ESC closes an open help panel (Story 6.1) — fallback in case the panel lost focus.
    if (key == juce::KeyPress::escapeKey && helpPanel && helpPanel->isVisible())
    {
        helpPanel->setVisible(false);
        return true;
    }
    // z / x shift the computer-keyboard octave (these keys aren't note keys).
    auto c = key.getTextCharacter();
    if (c == 'z' || c == 'Z' || c == 'x' || c == 'X')
    {
        int dir = (c == 'z' || c == 'Z') ? -1 : 1;
        kbBaseOctave = juce::jlimit(1, 7, kbBaseOctave + dir);
        if (keyboard)
            keyboard->setKeyPressBaseOctave(kbBaseOctave);
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

    // Title: big "J A S S" with the full name as a small subtitle beneath it.
    {
        auto titleArea = g_titleBounds;
        auto subArea = titleArea.removeFromBottom(20);
        g.setFont(juce::FontOptions(26.0f, juce::Font::bold));
        g.setColour(juce::Colour(0xff40c0ff));
        g.drawText("J A S S", titleArea, juce::Justification::centred);
        g.setFont(juce::FontOptions(13.0f));
        g.setColour(juce::Colour(0xff8899aa));
        g.drawText("Just Another Simple Synthesizer", subArea, juce::Justification::centred);
    }
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
    const juce::String title = (f != nullptr) ? f->moduleTitle() : id;
    helpPanel->setContent(title, HelpTextStore::instance().get(id, currentLang));   // sets size first
    currentHelpId = id;

    // Anchor beside the clicked module: to its right if there's room, else to its left,
    // aligned to its top — then clamp fully inside the editor (above the keyboard band).
    int x = getWidth() / 2 - HelpPanel::kWidth / 2, y = 90;
    if (f != nullptr)
    {
        auto tl = getLocalPoint(f, juce::Point<int>(0, 0));   // module top-left in editor coords
        x = tl.x + f->getWidth() + 8;
        if (x + HelpPanel::kWidth > getWidth() - 8)
            x = tl.x - HelpPanel::kWidth - 8;                 // no room right → place left
        y = tl.y;
    }
    const int maxX = juce::jmax(8, getWidth()  - HelpPanel::kWidth        - 8);
    const int maxY = juce::jmax(8, getHeight() - helpPanel->getHeight()   - 80);   // keep clear of keyboard
    helpPanel->setTopLeftPosition(juce::jlimit(8, maxX, x), juce::jlimit(8, maxY, y));

    helpPanel->setVisible(true);
    helpPanel->toFront(false);   // false = do NOT steal keyboard focus (keeps the on-screen
                                 // keyboard playable via the computer keys). ESC is handled by
                                 // the editor's keyPressed, so the panel needs no focus.
}

juce::File SynthyEditor::uiLanguageFile()
{
    return PresetIO::synthyFolder().getChildFile("ui-language.txt");
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

    // small builders to keep the descriptor list readable
    auto K = [](juce::String id, juce::String lbl) { return Knob{ std::move(id), std::move(lbl) }; };
    auto C = [](juce::String id, juce::String lbl, juce::StringArray items)
             { return Combo{ std::move(id), std::move(lbl), std::move(items) }; };
    // Story 1.4 (verification wiring; folded into the real descriptors in 1.5/2.2):
    // a knob tagged as an LFO ring target …
    auto Kmod = [](juce::String id, juce::String lbl, ModTarget mt)
             { Knob k{ std::move(id), std::move(lbl) }; k.modTarget = mt; return k; };
    // … and a FREQ knob with the played-frequency display transform (base × ratio).
    auto Kfreq = [](juce::String id, juce::String lbl)
             {
                 Knob k{ std::move(id), std::move(lbl) };
                 k.modTarget    = ModTarget::Frequency;
                 k.toDisplay    = [](double base,  double ratio) { return base  * ratio; };
                 k.fromDisplay  = [](double shown, double ratio) { return shown / ratio; };
                 return k;
             };

    auto add = [&](Rack::Zone zone, SizeClass sc, ModuleType type, juce::String title,
                   juce::String enableParam, std::vector<BodyElement> body,
                   std::function<void()> onReset = {})
    {
        ModuleDescriptor d;
        d.sizeClass = sc; d.type = type;
        // Stable slug from the title (e.g. "OSC 1" -> "osc1") — the RackLayout key for
        // show/hide + drag-drop (AD-10). Derived once here so every module gets one.
        d.id = title.toLowerCase().retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789");
        d.title = std::move(title);
        d.defaultZone = zone;   // AD-10: zone declared on the descriptor
        d.enableParam = std::move(enableParam); d.body = std::move(body);
        d.onReset = std::move(onReset);   // extra non-param reset (e.g. scope time-base)
        rackBody->addModule(std::move(d));
    };

    // OSC WAVE items MUST match the oscWave param's choice ORDER — the ComboBoxAttachment
    // maps by index, so a different order mislabels every waveform (same class of bug as the
    // LFO-WAVE fix in Story 2.1). oscWave = { Sine, Sawtooth, Square, Triangle } (Parameters.h).
    const juce::StringArray waves { "Sine", "Sawtooth", "Square", "Triangle" };

    // ---- MASTER BUS (top row; PROTOTYPE: decisions A+B) ----
    // Stereo becomes a normal module whose Enable IS stereoOn (no special-case header
    // chrome); Master is the new XS class (a single knob). Demonstrates "everything is a
    // module" before we formalise FR14 / the XS size class via correct-course.
    add(Rack::Zone::MasterBus, SizeClass::W3H1, ModuleType::Processor, "STEREO", P::stereoOn,
        { K(P::stereoWidth, "WIDTH"), K(P::stereoTime, "TIME") });
    add(Rack::Zone::MasterBus, SizeClass::W3H1, ModuleType::Processor, "MASTER", P::masterOn,
        { K(P::masterVol, "VOL"), K(P::syncTempo, "TEMPO") });   // TEMPO drives Tempo-Sync (host BPM overrides in a DAW)

    // ---- GENERATORS ----
    auto addOsc = [&](int i)
    {
        // M (not S): six controls now — WAVE, FREQ, AMP, VOICES, DETUNE + FB (Self-FM).
        // S is meant for 3–4 controls; a sixth would cram it, so bump one class up.
        add(Rack::Zone::Generators, SizeClass::W8H1, ModuleType::Generator,
            "OSC " + juce::String(i), P::oscOn(i),
            { C(P::oscWave(i), "WAVE", waves), Kfreq(P::oscFreq(i), "FREQ"),
              Kmod(P::oscAmp(i), "AMP", ModTarget::Amplitude), K(P::oscUniVoices(i), "VOICES"),
              K(P::oscUniDetune(i), "DETUNE"), K(P::oscFeedback(i), "FB") });
    };
    addOsc(1);
    addOsc(2);
    addOsc(3);
    // CROSS MOD comes AFTER OSC 3 in the default order (user pref 2026-07-12). It couples two
    // SELECTABLE oscillators (Source A / Source B) via RingMod or FM; disabled => plain additive sum.
    {
        // The derived lit/dimmed state reads the shared params only (AD-9), no module refs:
        // CROSS MOD is meaningful when the two SELECTED source OSCs are both enabled. Grab the
        // relevant atomics once (stable for the APVTS lifetime).
        auto* selA  = apvts.getRawParameterValue (P::mixSrcA);
        auto* selB  = apvts.getRawParameterValue (P::mixSrcB);
        std::array<std::atomic<float>*, 3> oscOn { apvts.getRawParameterValue (P::oscOn (1)),
                                                   apvts.getRawParameterValue (P::oscOn (2)),
                                                   apvts.getRawParameterValue (P::oscOn (3)) };
        ModuleDescriptor mix;
        mix.sizeClass = SizeClass::W5H1; mix.type = ModuleType::Generator;   // MODE + Source A + Source B
        // Renamed MIX MODE -> CROSS MOD (Option B: Additive dropped, module-off = additive).
        // Internal id stays "mixmode" so RackLayout persistence keeps matching.
        mix.id = "mixmode"; mix.title = "CROSS MOD";
        mix.enableParam = P::mixModeOn;   // enable = coupling on; off => plain additive sum
        const juce::StringArray oscItems { "OSC 1", "OSC 2", "OSC 3" };
        mix.body = { C(P::mixMode, "MODE", { "RingMod", "FM" }),
                     C(P::mixSrcA, "SRC A", oscItems),
                     C(P::mixSrcB, "SRC B", oscItems) };
        // Lit = mixModeOn AND both SELECTED sources enabled (a UI cue; the audio additive
        // fallback keys off mixModeOn / a==b only).
        mix.enabledWhen = [selA, selB, oscOn]
        {
            const int a = juce::jlimit (0, 2, (int) selA->load());
            const int b = juce::jlimit (0, 2, (int) selB->load());
            return oscOn[(size_t) a]->load() >= 0.5f && oscOn[(size_t) b]->load() >= 0.5f;
        };
        mix.defaultZone = Rack::Zone::Generators;   // AD-10: zone on the descriptor
        rackBody->addModule(std::move(mix));
    }

    add(Rack::Zone::Generators, SizeClass::W3H1, ModuleType::Generator, "SUB", P::subOn,
        { C(P::subWave, "WAVE", { "Sine", "Square" }), K(P::subLevel, "LEVEL") });
    add(Rack::Zone::Generators, SizeClass::W3H1, ModuleType::Generator, "NOISE", P::noiseOn,
        { C(P::noiseType, "TYPE", { "White", "Pink", "Brown", "Blue" }), K(P::noiseAmp, "AMP") });
    add(Rack::Zone::Generators, SizeClass::W6H1, ModuleType::Generator, "STRING - KARPLUS", P::karplusOn,
        { Action{ "PLUCK", [this] { processor.pluckString(); }, {} },
          K(P::karplusFreq, "FREQ"), K(P::karplusAmp, "AMP"),
          K(P::karplusDamping, "DAMP"), K(P::karplusStretch, "STR") });
    add(Rack::Zone::Generators, SizeClass::W8H1, ModuleType::Generator, "WAVETABLE", P::wavetableOn,
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
                      { juce::String(P::wavetableBank) } },   // refresh the BANK combo after load
          Kmod(P::wavetablePosition, "POS", ModTarget::WavetablePosition), K(P::wavetableFreq, "FREQ"), K(P::wavetableAmp, "AMP"),
          K(P::wavetableUniVoices, "VOICES"), K(P::wavetableUniDetune, "DETUNE") });

    // ---- MODULATION ----
    // ADSR: the second unit-row is the REAL EnvelopeDisplay (attack→decay→sustain→release
    // curve), a Display body element (AD-5), owned by rackOwned so its lifetime is tied
    // to the editor.
    add(Rack::Zone::Modulation, SizeClass::W4H2, ModuleType::Modulator, "ENVELOPE - ADSR", P::adsrOn,
        { K(P::attack, "ATK"), K(P::decay, "DEC"), K(P::sustain, "SUS"), K(P::release, "REL"),
          Display{ rackOwned.add(new EnvelopeDisplay(apvts, juce::Colour(0xff22d3ee))), 4 } });
    // LFO WAVE must list the lfoWave param's OWN choices in order — the ComboBoxAttachment
    // maps by index, so the shared `waves` array (a different order) would mislabel every
    // waveform (Story 2.1 AC3).
    // W8H1 (was W6H1): the SYNC combo (Tempo-Sync) is a 5th control. SYNC=Free => RATE knob;
    // else RATE is driven by the note division at the host/Sync tempo (RATE knob then ignored).
    add(Rack::Zone::Modulation, SizeClass::W8H1, ModuleType::Modulator, "LFO", P::lfoOn,
        { C(P::lfoWave, "WAVE", { "Sine", "Triangle", "Square", "Sawtooth" }),
          C(P::lfoTarget, "TARGET", { "Frequency", "Amplitude", "Filter Cutoff", "Wavetable Pos", "Formant Vowel", "Filter Reso", "Wavefold Drive" }),
          K(P::lfoRate, "RATE"), C(P::lfoSyncDiv, "SYNC", SyncDivision::kNames), K(P::lfoDepth, "DEPTH") });
    add(Rack::Zone::Modulation, SizeClass::W6H1, ModuleType::Modulator, "ARPEGGIATOR", P::arpOn,
        { C(P::arpMode, "MODE", { "Up", "Down", "UpDown", "Random" }),
          K(P::arpRate, "RATE"), K(P::arpOctaves, "OCT"), K(P::arpGate, "GATE") });
    // GLIDE / portamento: MODE = Mono (distinct classic glide) / Poly (per-voice), TIME = duration.
    add(Rack::Zone::Modulation, SizeClass::W3H1, ModuleType::Modulator, "GLIDE", P::glideOn,
        { C(P::glideMode, "MODE", { "Mono", "Poly" }), K(P::glideTime, "TIME") });

    // ---- PROCESSING ----
    // FILTER: TYPE combo + CUTOFF + RESO (= 4 slots, like DISTORTION) → M (4 cols) so the
    // combo isn't cramped. (Exact width tuning deferred to next session.)
    add(Rack::Zone::Processing, SizeClass::W4H1, ModuleType::Processor, "FILTER", P::filterOn,
        { C(P::filterType, "TYPE", { "Lowpass", "Highpass" }),
          Kmod(P::filterCutoff, "CUTOFF", ModTarget::FilterCutoff),
          Kmod(P::filterReso, "RESO", ModTarget::FilterResonance) });
    // M-class so the TYPE combo (2 slots) fits alongside DRIVE + MIX.
    // DISTORTION TYPE: display text is cosmetic ("Soft Clip"/"Hard Clip" read better) — the
    // ComboBoxAttachment maps by INDEX, so the canonical param/.synthy strings stay
    // "SoftClip"/"HardClip" (project-context: UI display may differ from the interop string).
    // Order/count MUST match distortionType's choices exactly, or the index mapping breaks.
    // FORMANT / vowel filter: VOWEL morphs A-E-I-O-U, RESO sharpens, MIX dry/wet. 3 knobs => W3H1.
    add(Rack::Zone::Processing, SizeClass::W3H1, ModuleType::Processor, "FORMANT", P::formantOn,
        { Kmod(P::formantVowel, "VOWEL", ModTarget::FormantVowel), K(P::formantReso, "RESO"), K(P::formantMix, "MIX") });
    add(Rack::Zone::Processing, SizeClass::W4H1, ModuleType::Processor, "DISTORTION", P::distortionOn,
        { C(P::distortionType, "TYPE", { "Soft Clip", "Hard Clip", "Foldback" }),
          K(P::distortionDrive, "DRIVE"), K(P::distortionMix, "MIX") });
    add(Rack::Zone::Processing, SizeClass::W3H1, ModuleType::Processor, "WAVEFOLD", P::wavefoldOn,
        { Kmod(P::wavefoldDrive, "DRIVE", ModTarget::WavefolderDrive), K(P::wavefoldSymmetry, "SYM"), K(P::wavefoldMix, "MIX") });
    add(Rack::Zone::Processing, SizeClass::W3H1, ModuleType::Processor, "BITCRUSH", P::bitcrushOn,
        { K(P::bitcrushBits, "BITS"), K(P::bitcrushRate, "RATE"), K(P::bitcrushMix, "MIX") });
    // PHASER / FLANGER: TYPE combo (2 slots) + 4 knobs => W6H1 (6 cols).
    add(Rack::Zone::Processing, SizeClass::W6H1, ModuleType::Processor, "PHASER", P::phaserOn,
        { C(P::phaserType, "TYPE", { "Phaser", "Flanger" }),
          K(P::phaserRate, "RATE"), K(P::phaserDepth, "DEPTH"),
          K(P::phaserFeedback, "FB"), K(P::phaserMix, "MIX") });
    add(Rack::Zone::Processing, SizeClass::W3H1, ModuleType::Processor, "CHORUS", P::chorusOn,
        { K(P::chorusRate, "RATE"), K(P::chorusDepth, "DEPTH"), K(P::chorusMix, "MIX") });
    // W5H1 (was W3H1): SYNC combo (Tempo-Sync) added. SYNC=Free => TIME knob; else TIME is the
    // note division at the host/Sync tempo (TIME knob then ignored).
    add(Rack::Zone::Processing, SizeClass::W5H1, ModuleType::Processor, "DELAY", P::delayOn,
        { K(P::delayTime, "TIME"), C(P::delaySyncDiv, "SYNC", SyncDivision::kNames),
          K(P::delayFeedback, "FB"), K(P::delayMix, "MIX") });
    add(Rack::Zone::Processing, SizeClass::W3H1, ModuleType::Processor, "REVERB", P::reverbOn,
        { K(P::reverbRoom, "ROOM"), K(P::reverbDamp, "DAMP"), K(P::reverbMix, "MIX") });
    // Real visualizers (own instances, separate from the legacy ones behind the rack —
    // one-parent rule). setShowTitle(false): the module header already shows the title.
    // Sharing the one WaveformCapture across instances is safe (updateSnapshot is idempotent
    // per frame). Sample rate reaches them via the capture (set in prepareToPlay).
    auto* scope = new WaveformDisplay(processor.getWaveformCapture());
    scope->setShowTitle(false);
    scope->setEnableSource(apvts.getRawParameterValue(P::scopeOn));   // scopeOn off => freeze+blank
    rackOwned.add(scope);
    add(Rack::Zone::Visualization, SizeClass::W12H2, ModuleType::Processor, "OSCILLOSCOPE", P::scopeOn,
        { Display{ scope, 12 } },
        [scope] { scope->resetTimeRange(); });   // ↺ restores the 10 ms default time-base
    auto* spec = new SpectrumDisplay(processor.getWaveformCapture());
    spec->setShowTitle(false);
    spec->setEnableSource(apvts.getRawParameterValue(P::spectrumOn));   // spectrumOn off => freeze+blank
    rackOwned.add(spec);
    // SPECTRUM has no adjustable state yet — its ↺ is a uniform-anatomy placeholder (a no-op
    // for now) so every module carries the same header controls; wire real params here later.
    add(Rack::Zone::Visualization, SizeClass::W12H2, ModuleType::Processor, "SPECTRUM", P::spectrumOn,
        { Display{ spec, 12 } },
        [] { });

    // The on-screen keyboard is itself a module (INPUT zone) so it can be hidden like any
    // other — e.g. when playing via an external MIDI keyboard. It wraps the existing keyboard
    // as a Display (AD-5; the editor owns its lifetime). enableParam keyboardOn is a UI-only
    // placeholder (drives only the dim state for now; the keyboard stays playable). The onReset
    // is a no-op placeholder so the header carries the uniform reset ↺ like every other module.
    // Full-width single row (W24H1). Its own info icon carries the play/shortcut help.
    add(Rack::Zone::Input, SizeClass::W24H1, ModuleType::Generator, "KEYBOARD", P::keyboardOn,
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
    // MODULES show/hide menu button overlays the right edge (clear of the centred title).
    modulesBtn.setBounds(headerRow.removeFromRight(120).reduced(8, 17));
    // Help-language selector sits just left of the MODULES button (Story 6.1).
    langBox.setBounds(headerRow.removeFromRight(66).reduced(4, 20));
    // Left cluster: the Save/Load/Random/Reset buttons AND the current-preset name belong
    // together (the preset name is about what was loaded/saved). Buttons in a 2x2 block
    // with "Current State" beside them.
    auto leftGroup = headerRow.removeFromLeft(340);
    auto leftBtns = leftGroup.removeFromLeft(150);
    auto row1 = leftBtns.removeFromTop(30);
    saveBtn.setBounds(row1.removeFromLeft(row1.getWidth() / 2).reduced(3, 2));
    loadBtn.setBounds(row1.reduced(3, 2));
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
