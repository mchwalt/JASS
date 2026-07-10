#pragma once
#include <JuceHeader.h>
#include "ModuleDescriptor.h"

namespace rack
{
    // Renders ONE ModuleDescriptor into the uniform module anatomy: a header strip
    // (title + optional enable toggle + reset ↺) over a body of widgets flowed into
    // the size class's slot grid. The frame owns its child widgets AND their APVTS
    // attachments (AD-6); it dims its body when the module's enable param is off
    // (AD-5/FR7). It lays out its OWN children inside whatever bounds the Rack gives
    // it (Story 1.3) — this resized() is the single body-layout site (NFR1); no
    // per-module class ever lays itself out.
    class ModuleFrame : public juce::Component,
                        private juce::Timer
    {
    public:
        ModuleFrame (juce::AudioProcessorValueTreeState& apvts, ModuleDescriptor descriptor);
        ~ModuleFrame() override;

        void resized() override;
        void paint (juce::Graphics&) override;
        void paintOverChildren (juce::Graphics&) override;

        // Push the current live feed (read by the ONE editor timer, AD-8) into this
        // frame's knobs: animate modulation rings on knobs whose modTarget matches the
        // LFO's active target (gated by this module's enable), and refresh any
        // display-transform knob (FREQ = base × played ratio). No audio-thread work —
        // just applies values already read from the processor's atomics.
        void updateLiveFeed (bool lfoOn, ModTarget activeTarget, float lfoValue, double playedRatio);

        // Module identity (stable slug from the descriptor) — used by the Rack to look a
        // module up (e.g. so the spacebar can trigger STRING-KARPLUS' PLUCK button).
        const juce::String& moduleId() const noexcept { return desc.id; }
        // Display title (for the show/hide MODULES menu, Story 4.2).
        const juce::String& moduleTitle() const noexcept { return desc.title; }
        // Visibly "press" this module's first Action button (shows the press animation AND
        // fires its onClick) — lets a keyboard shortcut mirror the on-screen button.
        void clickFirstAction();

    private:
        void timerCallback() override;
        void buildHeader();
        void buildBody();
        void doReset();

        // Re-poll a dynamic-provider combo's items (after an Action/FileAction that lists
        // it in .refreshes fired), then re-apply the param's current selection so the
        // ComboBoxAttachment stays consistent (AD-4 declarative combo refresh).
        void refreshCombo (const juce::String& paramId);

        // Enabled/lit = param-enable AND derived-predicate. A module may have EITHER (a real
        // enable param OR a derived condition) or BOTH (Mix-Mode: mixModeOn AND osc1&&osc2).
        // Absent signals are treated as "on", so a module with neither is always-on.
        bool moduleEnabled() const
        {
            const bool paramOn   = (enableValue == nullptr) || enableValue->load() >= 0.5f;
            const bool derivedOn = ! desc.enabledWhen || desc.enabledWhen();
            return paramOn && derivedOn;
        }

        struct Cell
        {
            juce::Component* widget  = nullptr;   // the primary widget in this cell
            juce::Label*     caption = nullptr;   // optional caption below it (knob/combo)
            int              slots   = 1;         // grid slots this cell spans
        };

        juce::AudioProcessorValueTreeState& apvts;
        ModuleDescriptor desc;

        juce::Label titleLabel;
        std::unique_ptr<juce::ToggleButton> enableBtn;   // only if enableParam set
        juce::TextButton resetBtn;

        std::vector<Cell> cells;
        juce::OwnedArray<juce::Component> ownedWidgets;   // owns non-Display body widgets
        juce::OwnedArray<juce::Label>     ownedCaptions;

        std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>>   sliderAtt;
        std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt;
        std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>>   buttonAtt;
        std::unique_ptr<juce::FileChooser> fileChooser;
        bool fileChooserActive = false;   // guards re-entrant FileAction clicks while a dialog is open

        std::atomic<float>* enableValue = nullptr;   // raw value of the enable param (nullptr => always-on)
        bool dimmed = false;

        // --- live-feed targets (Story 1.4) ---
        // Knobs carrying a modTarget: their ring is animated by updateLiveFeed when the
        // LFO targets that destination and the module is enabled.
        struct RingKnob { SynthySlider* slider; ModTarget target; };
        // Display-transform knobs are DECOUPLED from their param (no SliderAttachment):
        // they show toDisplay(base, ratio) and write fromDisplay(shown, ratio) back.
        struct XformKnob { SynthySlider* slider; juce::String paramId;
                           std::function<double(double,double)> toDisplay, fromDisplay; };
        std::vector<RingKnob>  ringKnobs;
        std::vector<XformKnob> xformKnobs;
        double liveRatio = 1.0;   // latest played-note ratio (1.0 = base); read by write-back

        // Combos built from a dynamic provider (e.g. the Wavetable bank list). Recorded so
        // an Action/FileAction can re-poll them declaratively via refreshCombo (Story 1.5).
        struct DynCombo { juce::String paramId; juce::ComboBox* box; std::function<juce::StringArray()> provider; };
        std::vector<DynCombo> dynCombos;

        std::vector<juce::Button*> actionButtons;   // Action-button widgets, in body order (for clickFirstAction)

        static constexpr int kHeaderH = 22;
        static constexpr int kComboH  = 22;   // combo box: short (half-height), wide, left-aligned
        static constexpr int kButtonW = 90;   // Action/FileAction/Toggle button: capped width
        static constexpr int kButtonH = 26;   //   …and fixed height (never stretched to the cell)

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModuleFrame)
    };
}
