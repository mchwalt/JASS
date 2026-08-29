#pragma once
#include <JuceHeader.h>
#include "ModuleDescriptor.h"
#include "IconButton.h"

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
        // frame's knobs: animate modulation rings on every knob whose modTarget currently
        // receives modulation (ringByTarget[(int) target], gated by this module's enable),
        // and refresh any display-transform knob (FREQ = base × played ratio). No audio-thread
        // work — just applies values already read from the processor's atomics.
        void updateLiveFeed (const LiveModFeed& ringByTarget, double playedRatio);

        // Module identity (stable slug from the descriptor) — used by the Rack to look a
        // module up (e.g. so the spacebar can trigger STRING-KARPLUS' PLUCK button).
        const juce::String& moduleId() const noexcept { return desc.id; }
        // Online-help resource slug: the descriptor's helpId if set, else the module id. Lets
        // instanced modules (LFO 1..4, OSC 1..3) share one help text without duplicating the .md.
        juce::String helpId() const { return desc.helpId.isNotEmpty() ? desc.helpId : desc.id; }
        // Display title (for the show/hide MODULES menu, Story 4.2).
        const juce::String& moduleTitle() const noexcept { return desc.title; }
        // Enable param id ("" if the module has no on/off) — lets the Rack couple
        // visibility to enable (hide ⇒ disable, show ⇒ enable once; Story 4.2).
        const juce::String& enableParamId() const noexcept { return desc.enableParam; }

        // A module that only DRAWS (scope, spectrum): hiding it changes nothing you can hear, so
        // the rack may take it at its word — see ModuleDescriptor::visualOnly.
        bool isVisualOnly() const noexcept { return desc.visualOnly; }
        // Visibly "press" this module's first Action button (shows the press animation AND
        // fires its onClick) — lets a keyboard shortcut mirror the on-screen button.
        void clickFirstAction();

        // Fired when the header info icon is clicked (Story 6.1); carries this module's id so
        // the editor can resolve its title + help text (in the active language) and show the
        // shared HelpPanel. Only present/shown when HelpTextStore has an entry for the id.
        std::function<void(const juce::String& id)> onHelp;

    private:
        void timerCallback() override;
        void buildHeader();
        void buildBody();
        void doReset();

        // Apply each Knob::activeWhen predicate: an irrelevant knob is disabled (ignores the
        // mouse) and dimmed, together with its caption. Only touches widgets on a real change.
        void updateCondKnobs();

    public:
        // Re-poll a dynamic-provider combo's items (after an Action/FileAction that lists
        // it in .refreshes fired), then re-apply the param's current selection so the
        // ComboBoxAttachment stays consistent (AD-4 declarative combo refresh). Public since
        // 12.6: the editor re-lists the SET combo while the background preload adds sets.
        void refreshCombo (const juce::String& paramId);

        // Re-run every named read-out (Knob::textFromValue): a display can depend on EDITOR
        // state the parameter never sees — STEP SEQ's note names follow the computer keyboard's
        // octave, so the octave keys must re-text the boxes without any value changing.
        void refreshNamedReadouts();

    private:

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
            juce::Button*    toggle  = nullptr;   // optional per-knob on/off (top-right corner)
        };

        juce::AudioProcessorValueTreeState& apvts;
        ModuleDescriptor desc;

        juce::Label titleLabel;
        std::unique_ptr<juce::ToggleButton> enableBtn;   // only if enableParam set
        IconButton resetBtn { IconButton::Kind::Reset };
        std::unique_ptr<IconButton> infoBtn;              // only if HelpTextStore has this id

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
        // Controls carrying a relevance predicate (Knob::activeWhen / Combo::activeWhen): polled
        // by the timer, which disables + dims just that control (and its caption) when the
        // predicate is false. Component-level (setEnabled/setAlpha), so knobs and combos share it.
        // `active` caches the last applied state so we only touch the widgets on a real change.
        // `dimOnly` keeps the mouse alive and only dims: a STEP SEQ rest's pitch survives (the
        // SPACE rule) and must stay editable without re-enabling the step first (maintainer
        // 2026-08-26) — unlike a mode-irrelevant knob, whose value truly does not apply.
        struct CondKnob { juce::Component* widget; juce::Label* caption; std::function<bool()> predicate;
                          bool dimOnly = false; char active = -1; };
        // Knobs carrying Knob::highlightWhen: polled by the timer, ringed by paintOverChildren while
        // the predicate holds. `on` caches the last state so we only repaint on a real change.
        // `ring` false => the mark is a lit dot (a sequencer playhead) instead of a ring (the write
        // cursor). Both kinds share one list so the timer polls them in one pass.
        struct MarkedKnob { juce::Component* widget; juce::Label* caption; std::function<bool()> predicate;
                            bool ring = true; size_t cellIndex = 0; char on = -1; };
        std::vector<MarkedKnob> markedKnobs;

        std::vector<RingKnob>  ringKnobs;
        std::vector<XformKnob> xformKnobs;
        std::vector<CondKnob>  condKnobs;

        // Row toggle (15.7, desc.altRowTitle): knobs carrying an altParamId own a SECOND slider
        // on the same cell bounds; the header latch flips which of the two is visible. The cell's
        // primary widget stays the main slider — corner switch, ring and playhead anchor to it,
        // and both views share them.
        struct AltKnob { SynthySlider* main; SynthySlider* alt; };
        std::vector<AltKnob> altKnobs;
        std::unique_ptr<juce::TextButton> altRowBtn;   // header latch; only when altRowTitle set
        juce::OwnedArray<juce::TextButton> actionBtns; // header one-shot actions (15.8), see desc.headerActions
        bool altRowActive = false;
        void applyAltRow();   // show/hide the pairs per altRowActive
        double liveRatio = 1.0;   // latest played-note ratio (1.0 = base); read by write-back

        // Combos built from a dynamic provider (e.g. the Wavetable bank list). Recorded so
        // an Action/FileAction can re-poll them declaratively via refreshCombo (Story 1.5).
        struct DynCombo { juce::String paramId; juce::ComboBox* box; std::function<juce::StringArray()> provider; };
        std::vector<DynCombo> dynCombos;

        // Dependent combos (MOD MATRIX): desc.comboDeps links a watched param to a combo to re-list.
        // lastWatched caches each watched value so the timer only reacts to real changes.
        std::vector<int> lastWatched;

        // indexIsValue combos have NO ComboBoxAttachment, so nothing resyncs them when their param
        // changes without a MODULE change (preset load / host automation). The timer polls these and
        // re-selects the item matching the param value.
        std::vector<std::pair<juce::String, juce::ComboBox*>> indexValueCombos;

        // Optional value list per indexIsValue combo (Knob-free equivalent of a lookup table). When
        // a combo declares one, the parameter stores values[position] instead of the position, so a
        // FILTERED list cannot silently re-point the parameter. Empty => position is the value.
        std::vector<std::pair<juce::String, std::function<juce::Array<int>()>>> comboValues;
        // Position of `value` in that combo's value list; without a list the value IS the position.
        // -1 when the current value is not in the (filtered) list — the box then shows nothing,
        // which is the honest answer: what is selected is not among the things offered.
        int comboPositionFor (const juce::String& paramId, int value) const;
        juce::Array<int> valuesFor (const juce::String& paramId) const;

        std::vector<juce::Button*> actionButtons;   // Action-button widgets, in body order (for clickFirstAction)

        // Per-slot activity (MOD MATRIX, desc.slotActivity): cached active state per slot so the timer
        // only repaints on change; paintOverChildren dims inactive slots + draws the lit/hollow dots.
        std::vector<char> slotActiveCache;

        // Vertical bands between repeated control GROUPS (MOD MATRIX's routing slots), computed in
        // resized() from the pixels the cell grid cannot use and painted in the dim colour. Empty
        // for every module that does not repeat a group.
        std::vector<juce::Rectangle<int>> groupGaps;

        static constexpr int kHeaderH = 22;
        static constexpr int kComboH  = 22;   // combo box: short (half-height), wide, left-aligned
        // ONE width for every combo, the way AD-3 gives every knob ONE diameter. Before this a
        // combo simply filled its cell, and since the cell is body-width / nCols, the same control
        // came out 106 px wide in MOD MATRIX and 120 px in FILTER — visibly ragged across the rack
        // for no reason. 106 is the width MOD MATRIX has always had and which reads fine there
        // (its MOD combo shows "DISTORTION"), so it is the proven lower bound rather than a guess.
        // A combo declaring more slots than the default 2 scales with them — nothing does today
        // (SAMPLER SET gave up its extra slot: once every combo shares one width, a wider one just
        // stands out), but the mechanism stays for a genuinely long item list.
        static constexpr int kComboW  = 106;
        static constexpr int kButtonW = 90;   // Action/FileAction/Toggle button: capped width
        static constexpr int kButtonH = 26;   //   …and fixed height (never stretched to the cell)

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModuleFrame)
    };
}
