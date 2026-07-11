#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "WaveformDisplay.h"
#include "SpectrumDisplay.h"
#include "rack/SynthyLookAndFeel.h"   // the single shared look (AD-7), moved into rack/
#include "rack/Rack.h"
#include "HelpPanel.h"                // movable per-module help panel (Story 6.1)

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

class SynthyEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit SynthyEditor(SynthyProcessor&);
    ~SynthyEditor() override { stopTimer(); setLookAndFeel(nullptr); }

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
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
    std::unique_ptr<juce::MidiKeyboardComponent> keyboard;
    int kbBaseOctave = 4;   // computer-keyboard octave (z / x shift it)

    // Layout bounds for paint() — the centred "J A S S" title.
    juce::Rectangle<int> g_titleBounds;

    // The rack IS the editor body now (legacy per-module panels removed in Story 3.3):
    // buildSampleRack() assembles every module as a declarative descriptor (AD-1) and
    // the Rack owns all placement (AD-2). The header chrome + keyboard sit in their own
    // bands. (Names kept as "sample*" from the Story-1.3 scaffold; purely historical.)
    std::unique_ptr<rack::Rack> sampleRack;
    juce::OwnedArray<juce::Component> sampleOwned;   // owns the rack's Display components (ADSR curve, scope, spectrum)
    void buildSampleRack();

    // Online help (Story 6.1): a header language selector + one shared movable HelpPanel.
    juce::ComboBox langBox;                 // EN / DE
    juce::String currentLang { "EN" };      // active help language (persisted as a global app setting)
    std::unique_ptr<HelpPanel> helpPanel;   // reused for every module; closed via ✕ / ESC
    juce::String currentHelpId;             // module id currently shown (for live re-render on language switch)
    void showModuleHelp(const juce::String& id);
    static juce::File uiLanguageFile();     // %AppData%\Synthy settings file for the language choice
    static juce::String loadUiLanguage();   // persisted language, else "EN"
    static void saveUiLanguage(const juce::String& lang);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthyEditor)
};
