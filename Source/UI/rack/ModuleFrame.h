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

    private:
        void timerCallback() override;
        void buildHeader();
        void buildBody();
        void doReset();

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

        static constexpr int kHeaderH = 22;
        static constexpr int kComboH  = 22;   // combo box: short (half-height), wide, left-aligned
        static constexpr int kButtonW = 90;   // Action/FileAction/Toggle button: capped width
        static constexpr int kButtonH = 26;   //   …and fixed height (never stretched to the cell)

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModuleFrame)
    };
}
