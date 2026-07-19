#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "WaveformDisplay.h"
#include "SpectrumDisplay.h"
#include "rack/SynthyLookAndFeel.h"   // the single shared look (AD-7), moved into rack/
#include "rack/Rack.h"
#include "HelpPanel.h"                // movable per-module help panel (Story 6.1)

// On-screen keyboard that always spreads its full note range across its own width, so
// the keys fill the module with no blank gap on the right. Now that the KEYBOARD lives in
// the rack (Input zone) the frame — not the editor — sizes it, so the fill maths must run
// in the component's own resized() rather than the editor's. setKeyWidth() only re-enters
// resized() when the width actually changes (JUCE guards it), so this settles in one step.
class FillWidthKeyboard : public juce::MidiKeyboardComponent
{
public:
    using juce::MidiKeyboardComponent::MidiKeyboardComponent;
    void resized() override
    {
        juce::MidiKeyboardComponent::resized();
        int whiteKeys = 0;
        for (int n = getRangeStart(); n <= getRangeEnd(); ++n)
            if (! juce::MidiMessage::isMidiNoteBlack(n)) ++whiteKeys;
        if (whiteKeys > 0 && getWidth() > 0)
            setKeyWidth((float) getWidth() / (float) whiteKeys);
    }
};

// A compact ADSR curve preview: attack ramp → decay to the sustain level →
// sustain hold → release tail. The A/D/R segment widths are drawn proportional
// to their durations (always filling the strip), the sustain segment is a fixed
// hold, and the sustain knob sets the hold height. Self-contained: it polls the
// four envelope params and only repaints when one of them moves.
class EnvelopeDisplay : public juce::Component, private juce::Timer
{
public:
    EnvelopeDisplay(juce::AudioProcessorValueTreeState& apvts, juce::Colour colour)
        : col(colour)
    {
        pA = apvts.getRawParameterValue("attack");
        pD = apvts.getRawParameterValue("decay");
        pS = apvts.getRawParameterValue("sustain");
        pR = apvts.getRawParameterValue("release");
        startTimerHz(20);
    }
    ~EnvelopeDisplay() override { stopTimer(); }

    void paint(juce::Graphics&) override;

private:
    void timerCallback() override
    {
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
                     private juce::Timer
{
public:
    explicit SynthyEditor(SynthyProcessor&);
    ~SynthyEditor() override { stopTimer(); setLookAndFeel(nullptr); }

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& e) override;   // right-click title => 3D-anim toggle menu
    void timerCallback() override;

private:
    SynthyProcessor& processor;
    SynthyLookAndFeel lnf;

    // Preset save/load (shared .synthy JSON format) + randomize + reset
    juce::TextButton saveBtn { "SAVE" }, loadBtn { "LOAD" }, randomBtn { "RANDOM" }, resetBtn { "RESET" };
    juce::TextButton modulesBtn { "MODULES" };   // show/hide menu (Story 4.2)
    void showModulesMenu();
    void refitHeight();   // recompute window height from the rack's visible content (AD-12)
    std::unique_ptr<juce::FileChooser> presetChooser;
    juce::Label presetNameLabel;                 // shows the currently loaded preset
    juce::String shownLabel;                     // last text pushed to the label (change-detect)
    void setPresetName(const juce::String& name);
    void updatePresetLabel();                    // composes "Preset: X" / "Current State"

    // On-screen keyboard (auto-play drone is handled automatically by the processor)
    std::unique_ptr<FillWidthKeyboard> keyboard;   // lives in the rack's Input zone (hideable)
    int kbBaseOctave = 4;   // computer-keyboard octave (z / x shift it)
    bool keyboardPlayable = true;   // mirrors keyboardOn: false => dimmed AND input-blocked
    bool modalWasOpen = false;      // edge-detect: a modal popup (e.g. MODULES) closing => refocus keyboard

    // Layout bounds for paint() — the centred title band.
    juce::Rectangle<int> g_titleBounds;
    SpinningTitle3D spinningTitle;   // animated 3D "J A S S" wordmark + subtitle (fills g_titleBounds)
    bool title3DAnimated = true;     // persisted: 3D header animation on/off (right-click title)
    static juce::File titleAnimFile();       // %AppData%\Synthy setting for the 3D-title toggle
    static bool loadTitleAnimated();         // persisted flag, else true
    static void saveTitleAnimated(bool on);

    // The rack IS the editor body now (legacy per-module panels removed in Story 3.3):
    // buildRack() assembles every module as a declarative descriptor (AD-1) and
    // the Rack owns all placement (AD-2). The header chrome + keyboard sit in their own
    // bands.
    std::unique_ptr<rack::Rack> rackBody;
    juce::OwnedArray<juce::Component> rackOwned;   // owns the rack's Display components (ADSR curve, scope, spectrum)
    void buildRack();

    // Online help (Story 6.1): a header language selector + one shared movable HelpPanel.
    juce::ComboBox langBox;                 // EN / DE
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
