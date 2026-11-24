#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "WaveformDisplay.h"
#include "SpectrumDisplay.h"
#include "PresetBankPanel.h"          // PRESETS quick-access bank (F1..F12)
#include "rack/SynthyLookAndFeel.h"   // the single shared look (AD-7), moved into rack/
#include "rack/Rack.h"
#include "HelpPanel.h"                // movable per-module help panel (Story 6.1)
#include "PercGrid.h"                 // PERC step field + instrument names (Story 16.1)

// On-screen keyboard that always spreads its full note range across its own width, so
// the keys fill the module with no blank gap on the right. Now that the KEYBOARD lives in
// the rack (Input zone) the frame — not the editor — sizes it, so the fill maths must run
// in the component's own resized() rather than the editor's. setKeyWidth() only re-enters
// resized() when the width actually changes (JUCE guards it), so this settles in one step.
class FillWidthKeyboard : public juce::MidiKeyboardComponent
{
public:
    using juce::MidiKeyboardComponent::MidiKeyboardComponent;

    // Show what the STEP SEQs are playing — one note per layer (Story 21.8), each in its layer's
    // colour. Their notes never enter the MidiKeyboardState (they would come back as held keys and
    // a pattern would re-root itself on its own output), so the component cannot know them: the
    // editor pushes them in from the processor's atomics instead. Purely a paint state — nothing
    // here sounds, and -1 means "that pattern is between notes".
    void setPatternNotes (int noteA, int noteB)
    {
        if (noteA == patternNotes[0] && noteB == patternNotes[1])
            return;
        patternNotes[0] = noteA;
        patternNotes[1] = noteB;
        repaint();
    }

    void resized() override
    {
        juce::MidiKeyboardComponent::resized();
        int whiteKeys = 0;
        for (int n = getRangeStart(); n <= getRangeEnd(); ++n)
            if (! juce::MidiMessage::isMidiNoteBlack(n)) ++whiteKeys;
        if (whiteKeys > 0 && getWidth() > 0)
            setKeyWidth((float) getWidth() / (float) whiteKeys);
        placeSplitButton();
    }

    // The pattern's note is drawn like a pressed key, in the MODULATION colour so it reads as
    // "played by the machine" rather than "held by you" — the two happen at the same time, since
    // the key you hold is the pattern's root.
    void drawWhiteNote (int midiNote, juce::Graphics& g, juce::Rectangle<float> area,
                        bool isDown, bool isOver, juce::Colour lineColour, juce::Colour textColour) override
    {
        juce::MidiKeyboardComponent::drawWhiteNote (midiNote, g, area, isDown, isOver, lineColour, textColour);
        paintPatternNote (midiNote, g, area);
    }

    void drawBlackNote (int midiNote, juce::Graphics& g, juce::Rectangle<float> area,
                        bool isDown, bool isOver, juce::Colour noteFillColour) override
    {
        juce::MidiKeyboardComponent::drawBlackNote (midiNote, g, area, isDown, isOver, noteFillColour);
        paintPatternNote (midiNote, g, area);
    }

    // Hover readout: name + MIDI number of the key under the mouse ("A3 · 57"), pinned to the
    // top-right corner — a fixed place to look, not a tooltip chasing the cursor. Exists for
    // transcribing from templates: sheet or GM tables name notes, SAMPLER ROOT / PERC NOTE
    // count numbers, and this shows both before the key is pressed.
    void mouseMove (const juce::MouseEvent& e) override
    {
        setHoverNote (getNoteAndVelocityAtPosition (e.position).note);
        juce::MidiKeyboardComponent::mouseMove (e);
    }

    void mouseExit (const juce::MouseEvent& e) override
    {
        setHoverNote (-1);
        juce::MidiKeyboardComponent::mouseExit (e);
    }

    void paint (juce::Graphics& g) override
    {
        juce::MidiKeyboardComponent::paint (g);
        // SPLIT (21.6): a big "B" over B's keys and "A" over A's, near the bottom of the white keys.
        if (splitNote > getRangeStart() && splitNote <= getRangeEnd())
        {
            const float yb = (float) getHeight() - 34.0f;
            auto label = [&] (const juce::String& t, int from, int to, juce::Colour c)
            {
                const auto r0 = getRectangleForKey (from), r1 = getRectangleForKey (to);
                const juce::Rectangle<float> box (r0.getX(), yb, r1.getRight() - r0.getX(), 30.0f);
                g.setColour (c);
                g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
                g.drawText (t, box, juce::Justification::centred);
            };
            label ("B", getRangeStart(), splitNote - 1, rack::kLayerBColour);
            label ("A", splitNote, getRangeEnd(), rack::kLayerAColour.darker (0.9f));   // darker: legible on the white keys
        }
        if (hoverNote < 0)
            return;
        // Separator as an explicit UTF-8 escape: a bare "·" literal is non-ASCII, and MSVC
        // reads a BOM-less file as ANSI — the two UTF-8 bytes then render as mojibake.
        const auto text = juce::MidiMessage::getMidiNoteName (hoverNote, true, true, 4)   // 60 = C4, as everywhere in JASS
                        + juce::String::fromUTF8 (" \xc2\xb7 ") + juce::String (hoverNote);
        const auto box = getLocalBounds().toFloat().removeFromTop (26.0f).removeFromRight (130.0f).reduced (2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.75f));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
        g.drawText (text, box, juce::Justification::centred);
    }

private:
    void setHoverNote (int midiNote)
    {
        if (midiNote == hoverNote)
            return;
        hoverNote = midiNote;
        repaint();
    }

    void paintPatternNote (int midiNote, juce::Graphics& g, juce::Rectangle<float> area) const
    {
        // AD-16 SPLIT (21.6): the two halves in their layer's colour — B (below the split key)
        // violet, A cyan, as on the LAYERS buttons — and a bar on the split key's left edge, where
        // B ends and A begins. The letters are drawn over the whole range in paint().
        if (splitNote >= 0)
        {
            g.setColour (midiNote < splitNote ? rack::kLayerBColour.withAlpha (0.24f)
                                              : rack::kLayerAColour.withAlpha (0.14f));
            g.fillRect (area);
            if (midiNote == splitNote)
            {
                g.setColour (juce::Colours::white);
                g.fillRect (area.withWidth (3.0f));
            }
        }
        // The running pattern notes, A cyan, B violet; one key played by both shows both halves.
        const bool a = midiNote == patternNotes[0], b = midiNote == patternNotes[1];
        if (! a && ! b)
            return;
        auto r = area.reduced (1.0f);
        if (a && b)
        {
            g.setColour (rack::kLayerAColour.withAlpha (0.75f));
            g.fillRect (r.removeFromTop (r.getHeight() * 0.5f));
            g.setColour (rack::kLayerBColour.withAlpha (0.75f));
            g.fillRect (r);
            return;
        }
        g.setColour ((a ? rack::kLayerAColour : rack::kLayerBColour).withAlpha (0.75f));
        g.fillRect (r);
    }

public:
    // SPLIT on (21.6): -1 = no split shown. Paint state only.
    void setSplitNote (int note)
    {
        if (note == splitNote) return;
        splitNote = note;
        repaint();
    }

    // SPLIT button (21.6; on/off since 2026-11-23): sits over the lowest keys, top left, where the
    // split is shown. Lit violet while the split is on; reads LEARN while a double-click waits for
    // a key. The editor owns the behaviour, the keyboard only hosts the button.
    void addSplitButton (std::function<void()> onClick)
    {
        splitButton.setClickingTogglesState (false);
        splitButton.setWantsKeyboardFocus (false);
        splitButton.setMouseClickGrabsKeyboardFocus (false);   // the keys keep playing
        splitButton.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff334155));
        splitButton.setColour (juce::TextButton::buttonOnColourId, rack::kLayerBColour);
        splitButton.setColour (juce::TextButton::textColourOffId,  juce::Colours::white.withAlpha (0.75f));
        splitButton.setColour (juce::TextButton::textColourOnId,   juce::Colours::white);
        splitButton.setTooltip ("Split on/off (key 3) - double-click (3, 3), then play a key: it becomes the split key");
        splitButton.onClick = std::move (onClick);
        addAndMakeVisible (splitButton);
        resized();
    }

    void setSplitState (bool on, bool learning)
    {
        splitButton.setToggleState (on || learning, juce::dontSendNotification);
        const juce::String label (learning ? "LEARN" : "SPLIT");
        if (splitButton.getButtonText() != label)
            splitButton.setButtonText (label);
    }

private:
    void placeSplitButton()
    {
        splitButton.setBounds (4, 3, 64, 22);
    }

    juce::TextButton splitButton { "SPLIT" };
    int patternNotes[2] { -1, -1 };   // 21.8: A's and B's running STEP SEQ note
    int hoverNote = -1;
    int splitNote = -1;
};

// LAYERS (AD-16, Story 21.6): which layer the rack edits — two buttons, A and B, each on its own
// or both together (A+B: both play, the rack shows every module either layer uses, a knob on an A
// frame moves B too, on/off stays per layer — see Rack::viewFrameIsB). One of
// them is always lit: clicking the only lit button does nothing; see toggle(). Writes `layerFocus`
// (0 = A, 1 = B, 2 = A+B); polls it, so a preset load or RESET shows up here.
class LayerSelector : public juce::Component, private juce::Timer
{
public:
    explicit LayerSelector (juce::AudioProcessorValueTreeState& s) : apvts (s)
    {
        for (auto* b : { &btnA, &btnB })
        {
            addAndMakeVisible (*b);
            b->setClickingTogglesState (false);
            b->setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff334155));
            b->setColour (juce::TextButton::textColourOffId,  juce::Colours::white.withAlpha (0.55f));
            b->setColour (juce::TextButton::textColourOnId,   juce::Colours::white);
        }
        btnA.setColour (juce::TextButton::buttonOnColourId, rack::kLayerAColour.darker (0.35f));
        btnB.setColour (juce::TextButton::buttonOnColourId, rack::kLayerBColour);
        btnA.setTooltip ("Layer A (key 1) - click: add A (both play, edits go to both); double-click: A only");
        btnB.setTooltip ("Layer B (key 2) - click: add B (both play, edits go to both); double-click: B only");
        btnA.onClick = [this] { toggle (true); };
        btnB.onClick = [this] { toggle (false); };
        sync();
        startTimerHz (10);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (2);
        const int h = juce::jmin (r.getHeight(), 34);
        r = r.withSizeKeepingCentre (r.getWidth(), h);
        btnA.setBounds (r.removeFromLeft (r.getWidth() / 2).reduced (2, 0));
        btnB.setBounds (r.reduced (2, 0));
    }

private:
    void timerCallback() override { sync(); }

    int focus() const
    {
        const auto* f = apvts.getRawParameterValue (Parameters::ID::layerFocus);
        return f != nullptr ? juce::roundToInt (f->load()) : 0;
    }

    void sync()
    {
        const int f = focus();
        btnA.setToggleState (f != 1, juce::dontSendNotification);
        btnB.setToggleState (f != 0, juce::dontSendNotification);
    }

public:
    // Keys 1 / 2 (21.6): exactly a click on A / B.
    void press (bool isA) { toggle (isA); }

private:
    // Maintainer 2026-11-23: a single press on an unlit layer adds it (A+B); a single press on a lit
    // one does nothing; a quick double press (or double-click, within the system's double-click
    // time) keeps only the pressed layer. So B, B quickly goes from A to B and A, A back - pressed
    // slowly, the second press changes nothing. No press ever drops the layer you pressed.
    void toggle (bool isA)
    {
        sync();   // the buttons may be up to one timer tick behind the parameter (review)
        const auto now = juce::Time::getMillisecondCounter();
        const bool quick = lastPress != 0 && lastWasA == isA
                        && (int) (now - lastPress) <= juce::MouseEvent::getDoubleClickTimeout();
        lastPress = quick ? 0u : now;   // a third quick press starts a new pair
        lastWasA  = isA;
        const int own = isA ? 0 : 1;
        const int cur = focus();
        const int f = quick ? own : (cur == 1 - own ? 2 : cur);
        if (f == cur) return;
        if (auto* p = apvts.getParameter (Parameters::ID::layerFocus))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 ((float) f));
            p->endChangeGesture();
        }
        sync();
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::TextButton btnA { "A" }, btnB { "B" };
    juce::uint32 lastPress = 0;   // ms counter of the last single press (0 = none pending)
    bool lastWasA = true;
};

// A compact ADSR curve preview: attack ramp → decay to the sustain level →
// sustain hold → release tail. The A/D/R segment widths are drawn proportional
// to their durations (always filling the strip), the sustain segment is a fixed
// hold, and the sustain knob sets the hold height. Self-contained: it polls the
// four envelope params and only repaints when one of them moves.
class EnvelopeDisplay : public juce::Component, private juce::Timer
{
public:
    // `layerPrefix` (AD-16, 21.6): "" = layer A's envelope, "b" = layer B's.
    EnvelopeDisplay(juce::AudioProcessorValueTreeState& apvts, juce::Colour colour,
                    const juce::String& layerPrefix = {})
        : col(colour)
    {
        pA = apvts.getRawParameterValue(layerPrefix + Parameters::ID::attack);
        pD = apvts.getRawParameterValue(layerPrefix + Parameters::ID::decay);
        pS = apvts.getRawParameterValue(layerPrefix + Parameters::ID::sustain);
        pR = apvts.getRawParameterValue(layerPrefix + Parameters::ID::release);
        startTimerHz(20);
    }
    ~EnvelopeDisplay() override { stopTimer(); }

    void paint(juce::Graphics&) override;

private:
    void timerCallback() override
    {
        if (pA == nullptr || pD == nullptr || pS == nullptr || pR == nullptr) return;   // paint() guards too
        const float a = pA->load(), d = pD->load(), s = pS->load(), r = pR->load();
        if (a != lA || d != lD || s != lS || r != lR)
        {
            lA = a; lD = d; lS = s; lR = r;
            repaint();
        }
    }

    std::atomic<float> *pA = nullptr, *pD = nullptr, *pS = nullptr, *pR = nullptr;
    float lA = -1, lD = -1, lS = -1, lR = -1;
    juce::Colour col;
};

// A slowly Y-rotating, extruded 3D rendering of the "J A S S" wordmark — the old-school
// 3D-text-screensaver look, drawn with layered affine glyph fills (no OpenGL): the flat glyph
// outline is filled once per depth slice, each slice x-scaled by cos(angle) and x-shifted by
// (sliceDepth · sin(angle)), painted back-to-front with a depth shade. Purely decorative:
// transparent + ignores the mouse; also paints the static subtitle beneath it.
class SpinningTitle3D : public juce::Component, private juce::Timer
{
public:
    SpinningTitle3D()
    {
        setInterceptsMouseClicks(false, false);
        startTimerHz(30);
    }
    ~SpinningTitle3D() override { stopTimer(); }

    void resized() override;
    void paint(juce::Graphics&) override;

    // Toggle the animation (persisted app setting). When off, the component draws the plain
    // legacy 2D wordmark and stops its timer (no CPU). Portable: pure juce::Graphics either way.
    void setAnimate(bool shouldAnimate)
    {
        if (animate == shouldAnimate) return;
        animate = shouldAnimate;
        if (animate) startTimerHz(30); else stopTimer();
        repaint();
    }
    bool isAnimating() const noexcept { return animate; }

private:
    void timerCallback() override
    {
        angle += 0.026f;   // ~ one turn every ~4 s
        if (angle >= juce::MathConstants<float>::twoPi)
            angle -= juce::MathConstants<float>::twoPi;
        repaint();
    }
    void rebuildGlyphPath();

    juce::Path glyphPath;   // "J A S S" outline, centred at origin, scaled to the component
    float angle = 0.0f;
    bool  animate = true;   // false => plain static 2D title (legacy look)

    static constexpr int   kLayers = 18;     // extrusion slices (back → front)
    static constexpr float kDepth  = 9.0f;   // extrusion depth in px
};

class SynthyEditor : public juce::AudioProcessorEditor,
                     private juce::Timer,
                     private juce::MidiKeyboardState::Listener   // notes played WHILE recording a
                                                                 // STEP SEQ figure (Story 15.4)
{
public:
    explicit SynthyEditor(SynthyProcessor&);
    ~SynthyEditor() override
    {
        stopTimer();
        processor.onStateReloaded = nullptr;   // the async re-load must not reach a dead editor
        processor.getKeyboardState().removeListener (this);
        for (int l = 0; l < SynthyProcessor::kNumLanes; ++l)
            processor.setSeqRecordArmed (l, false);   // a closed window must never leave a sequencer
                                                      // rootless — it would look enabled and stay silent
        stopAudition();   // the timer is gone — close a running STEP SEQ preview by hand
        // Dismiss the MODULES call-out NOW (before rackBody is destroyed): its RackCustomizePanel
        // holds a reference to *rackBody, so a callout left open when the editor closes would dangle.
        if (auto* co = modulesCallout.getComponent())
            co->dismiss();
        if (auto* dw = standaloneWin.getComponent())   // detach our title-bar look before it dies
            dw->setLookAndFeel(nullptr);
        setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    bool keyStateChanged(bool isKeyDown) override;   // F1..F12 preset-bank triggers (event-driven)
    void mouseDown(const juce::MouseEvent& e) override;   // right-click title => 3D-anim toggle menu
    void timerCallback() override;

private:
    SynthyProcessor& processor;
    SynthyLookAndFeel lnf;

    // Preset save/load (shared .synthy JSON format) + delete + randomize + reset
    juce::TextButton saveBtn { "SAVE" }, loadBtn { "LOAD" }, randomBtn { "RANDOM" }, resetBtn { "RESET" };
    juce::TextButton deleteBtn { "DELETE" };   // delete a preset FILE (to the recycle bin, confirmed)
    juce::TextButton modulesBtn { "MODULES" };   // show/hide menu (Story 4.2)
    void showModulesMenu();
    void refitHeight();   // recompute window height from the rack's visible content (AD-12)
    double fitScale = 0.0;   // the ONE display-fit scale (0 = not computed yet); see refitHeight()
    // The screen the current fit was measured on. Dragging the window to a monitor of a different
    // size or scaling has to re-fit — nothing else triggers refitHeight() when only the position
    // changes, and the window would keep the other screen's scale.
    juce::Rectangle<float> lastFitDisplay;   // JUCE 9: userBounds is float
    double lastFitDisplayScale = 0.0;
    // The help panel opts OUT of that scale so its text stays readable — these three keep the
    // sizing, the "cancel the transform" and the resulting on-screen size in one place.
    double helpScale = 1.0;   // magnification the panel is currently shown at (>= 1.0)
    void fitHelpContent(const juce::String& title, const juce::String& body);
    int  helpPanelWidthOnScreen() const;
    void placeHelpPanel(int wantX, int wantY);
    std::unique_ptr<juce::FileChooser> presetChooser;
    juce::Label presetNameLabel;                 // shows the currently loaded preset
    juce::String shownLabel;                     // last text pushed to the label (change-detect)
    void setPresetName(const juce::String& name);
    void updatePresetLabel();                    // composes "Preset: X" / "Current State"
    void loadPresetFile(const juce::File& f);    // shared LOAD path (LOAD button + bank F-keys)
    void doublePercPattern();                    // PERC header x2: pattern doubles behind itself
    // STEP SEQ header LOAD MIDI / SAVE MIDI (15.8) — of the layer whose module was clicked (21.8).
    void chooseMidiImport(int lane);
    void chooseMidiExport(int lane);
    void importMidiFigure(const juce::File& f, int lane);   // the import itself: figure into that STEP SEQ

    // Preset quick-access bank (F1..F12) — a MASTER BUS module. The 12 slot assignments are a
    // GLOBAL app setting (PresetIO::PresetBanks.json), not per-preset.
    PresetBankPanel* presetBank = nullptr;             // owned by rackOwned; typed handle for the F-keys
    std::array<juce::String, 12> presetSlots;          // slot -> preset name (mirrors the panel + the file)
    void triggerPresetSlot(int slot);                  // load the preset assigned to a slot (no-op if empty)
    void assignPresetSlot(int slot);                   // open the assign dialog for a slot
    void clearBankSlotsFor(const juce::String& name);  // remove every F-key assignment of this preset name
    void resetPresetBank();                            // restore factory bank (demos on F1..F4) — RESET button
    void pollPresetHotkeys();                          // F1..F12 edge detection (keyStateChanged + timer fallback)
    // F1..F12 (preset bank) — handled in keyStateChanged (event-driven): the on-screen keyboard
    // consumes only its note keys, so F-key transitions bubble up. A single press on a filled slot
    // loads; a DOUBLE press opens the assign dialog.
    bool          fKeyDown[12]       { };   // last observed physical state (press/release edge detect)
    juce::uint32  fKeyLastPressMs[12]{ };   // time of the previous press (double-press detection)

    // On-screen keyboard (auto-play drone is handled automatically by the processor)
    // Computer-key playing is OURS, not JUCE's (its mappings are cleared): MidiKeyboardComponent
    // keys its internal state by NOTE NUMBER, so shifting the octave under a held key desynchronises
    // it — the note-off would land on the new octave. We remember the note each PHYSICAL key started,
    // so a held note keeps playing (and releases correctly) across any number of octave shifts.
    struct ComputerKey
    {
        juce::KeyPress key;
        int offsetFromC = 0;
        int sounding    = -1;   // MIDI note this key started, or -1
    };
    void buildComputerKeyMap();                    // fills computerKeys (QWERTZ layout)
    void updateComputerKeys (bool allowNoteOn);    // reconcile physical key state -> notes
    void retuneSoundingComputerKeys();             // held keys follow an octave shift
    void releaseComputerKeys();                    // note-off everything we started
    std::vector<ComputerKey> computerKeys;
    std::unique_ptr<FillWidthKeyboard> keyboard;   // lives in the rack's Input zone (hideable)
    LayerSelector* layerSelector = nullptr;        // 21.6: owned by rackOwned; keys 1 / 2 press it
    void pressSplitButton();                       // the keyboard's SPLIT button (or key 3): split on/off; double = LEARN
    juce::uint32 lastSplitPress = 0;               // ms counter of the last single SPLIT press (0 = none pending)
    bool layerKeyDown[2] {};                       // keys 1 / 2 held: auto-repeat is no double press
    void syncSplitButton();                        // its lit state and label from layerMode + the learn wait
    bool splitKeyDown = false;                     // key 3 edge detection (keyPressed auto-repeats)

    // Without a TooltipWindow instance JUCE shows NO tooltips at all — every setTooltip in the
    // rack (module reset/info buttons, the full-text knob read-outs) was silent until this line.
    juce::TooltipWindow tooltipWindow { this, 400 };
    int kbBaseOctave = 4;   // computer-keyboard octave (Up / Down arrows shift it)

    // Preview of a STEP SEQ step while it is edited (Story 15.3). The step's value is an offset in
    // semitones, so it needs a reference — seqPitchReference(): the LATCHED root while a figure is
    // running (what you click is what the loop plays), otherwise the computer keyboard's current C.
    // Story 21.8: every STEP SEQ function takes the LANE (= the layer whose module it is: 0 = A,
    // 1 = B); the preview plays on that layer's audition channel, so B's steps sound in B.
    void auditionStep (int lane, int semitones, bool sounding, bool accented = false);
    void stopAudition();              // close a running preview, whichever lane it belongs to
    // GATE preview: sound the step for gate% of the current step length (TIE / SLIDE = one full
    // step), re-triggering on every call so a scrubbed gate is heard length by length.
    void auditionStepGated (int lane, int semitones, bool accented, int gate);
    int  seqPitchReference (int lane) const;   // the root a step offset actually sounds against (15.6)
    std::array<int, SynthyProcessor::kNumLanes> seqRefShown { -1, -1 };   // last reference the note boxes were texted with
    int auditionNote  = -1;   // MIDI note currently previewing, or -1
    int auditionChannel = SynthyProcessor::kAuditionChannel;   // ...and the channel it sounds on
    bool auditionAccented = false;   // 15.2: preview velocity of the sounding audition note
    int auditionTicks = 0;    // 30 Hz timer ticks until the safety release (a wheel or a typed
                              // value has no drag end that could close the note)

    // --- Writing a figure by playing it (Story 15.4) ------------------------------------------
    // The write cursor is the ONE step the next played note goes into: 0-based, -1 = not recording.
    // Pure UI state — nothing about it belongs in a preset (AC9). Reset (↺) arms it at step 0, a
    // click on any step knob moves it there, and it clears itself past LEN or when STEP SEQ goes off.
    // One cursor for the whole rack (21.8): it sits in ONE lane's figure at a time.
    int  seqCursor = -1;
    int  seqCursorLane = 0;
    void seqSetCursor (int lane, int step);  // -1 stops; also drives that lane's record flag
    void seqStopCursor() { seqSetCursor (seqCursorLane, -1); }

    // --- Step pages (16.3) ---------------------------------------------------------------------
    // View state for the two paged sequencer grids: which 48-step page is shown, and whether the
    // display follows the playhead (the catch convention: a MANUAL page pick pauses FOLLOW so the
    // page cannot flip away under an editing cursor; re-latching jumps to the playing page). The
    // ModuleFrames poll these through their descriptor's paging hooks and rebuild/repaint on a
    // change; the timer below does the auto-flip. Pure UI state — never in a preset.
    std::array<int,  SynthyProcessor::kNumLanes> seqShownPage { 0, 0 };   // per STEP SEQ (21.8)
    std::array<bool, SynthyProcessor::kNumLanes> seqFollow    { true, true };
    int  percShownPage = 0;
    bool percFollow    = true;
    PercGrid* percGrid = nullptr;            // typed pointer (rackOwned owns it): page offset pushes
    int  seqPlayingPage (int lane) const;   // page under that lane's playhead, -1 when not running
    int  percPlayingPage() const;
    void setSeqPage  (int lane, int page, bool manual);   // manual = a page-button pick (pauses FOLLOW)
    void setPercPage (int page, bool manual);
    void seqWriteNote (int midiNote);        // write the cursor's step from a played note, advance
    void seqAdvanceCursor();                 // next step, or stop past LEN
    void seqSkipStep();                      // SPACE: switch this step OFF (a rest) and move on
    // handleNoteOn runs on the AUDIO thread for hardware MIDI (MidiKeyboardState is pumped in
    // processBlock), so it only parks the note here; the 30 Hz timer picks it up on the message
    // thread and does the parameter write. -1 = nothing pending.
    std::atomic<int> seqPendingNote { -1 };
    std::array<bool, SynthyProcessor::kNumLanes> seqOnLast { false, false };   // edge: "STEP SEQ was switched off" (ends a recording)
    void handleNoteOn  (juce::MidiKeyboardState*, int midiChannel, int midiNote, float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int midiChannel, int midiNote, float velocity) override;
    bool keyboardPlayable = true;   // mirrors keyboardOn: false => dimmed AND input-blocked
    bool modalWasOpen = false;      // edge-detect: a modal popup (e.g. MODULES) closing => refocus keyboard

    // Layout bounds for paint() — the centred title band.
    juce::Rectangle<int> g_titleBounds;
    SpinningTitle3D spinningTitle;   // animated 3D "J A S S" wordmark + subtitle (fills g_titleBounds)
    // Standalone: our look for the JUCE wrapper's title bar ("JUCE", left-aligned) + a safe handle
    // to that top-level window so we can detach the look in the destructor.
    std::unique_ptr<juce::LookAndFeel> standaloneTitleLnF;
    juce::Component::SafePointer<juce::DocumentWindow> standaloneWin;
    juce::Component::SafePointer<juce::CallOutBox> modulesCallout;   // MODULES call-out; dismissed on teardown
    bool title3DAnimated = true;     // persisted: 3D header animation on/off (right-click title)
    int  loadedFormatVersion = 4;    // FormatVersion of the last preset loaded via the LOAD dialog
                                     // (shown in the right-click title info menu; AC6 of Story 9.2)
    static juce::File titleAnimFile();       // %AppData%\Synthy setting for the 3D-title toggle
    static bool loadTitleAnimated();         // persisted flag, else true
    static void saveTitleAnimated(bool on);

    // The rack IS the editor body now (legacy per-module panels removed in Story 3.3):
    // buildRack() assembles every module as a declarative descriptor (AD-1) and
    // the Rack owns all placement (AD-2). The header chrome + keyboard sit in their own
    // bands.
    // DECLARATION ORDER MATTERS (members destruct in reverse): rackOwned holds the Display
    // components (ADSR curve, scope, spectrum, preset bank) that the rack's ModuleFrames reference
    // by raw pointer. rackBody MUST be declared AFTER rackOwned so it is destroyed FIRST — otherwise
    // the frames would dangle over already-freed Displays during teardown.
    int shownSampleSets = -1;   // set count the SET combo was last listed with (12.6 background preload)
    juce::OwnedArray<juce::Component> rackOwned;   // owns the rack's Display components (ADSR curve, scope, spectrum)
    std::unique_ptr<rack::Rack> rackBody;
    bool shiftShownOnReset = false;   // 7.6: last Shift state shown on the module ↺ icons
    void buildRack();

    // Online help (Story 6.1): a header language selector + one shared movable HelpPanel.
    juce::ComboBox langBox;                 // EN / DE
    // Story 22.1: which controller profile moves the knobs — "Auto" (from the MIDI input names) or a
    // hand pick. Standalone only (hidden in a DAW). Refilled when the Controllers folder changes.
    juce::ComboBox controllerBox;
    int controllerBoxRevision = -1;
    void refreshControllerBox();
    juce::String currentLang { "EN" };      // active help language (persisted as a global app setting)
    std::unique_ptr<HelpPanel> helpPanel;   // reused for every module; closed via ✕ / ESC
    juce::String currentHelpId;             // module/zone id currently shown (for live re-render on language switch)
    void showModuleHelp(const juce::String& id);
    void showZoneHelp(rack::Rack::Zone zone);   // shared HelpPanel for a group header's info icon
    static juce::File uiLanguageFile();     // %AppData%\Synthy settings file for the language choice
    static juce::String loadUiLanguage();   // persisted language, else "EN"
    static void saveUiLanguage(const juce::String& lang);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthyEditor)
};
