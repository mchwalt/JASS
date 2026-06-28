#include "ModuleFrame.h"

namespace rack
{
    namespace
    {
        // Desaturated identity tints, matching the rack mockup (Generator/Modulator/Processor).
        juce::Colour typeColour (ModuleType t)
        {
            switch (t)
            {
                case ModuleType::Generator: return juce::Colour (0xff5e9b96);
                case ModuleType::Modulator: return juce::Colour (0xff9384b6);
                case ModuleType::Processor: return juce::Colour (0xff6f86ad);
            }
            return juce::Colour (0xff6f86ad);
        }

        juce::Label* makeCaption (juce::OwnedArray<juce::Label>& store, const juce::String& text)
        {
            auto* l = store.add (new juce::Label ({}, text));
            l->setJustificationType (juce::Justification::centred);
            l->setFont (juce::FontOptions (10.0f));
            l->setInterceptsMouseClicks (false, false);
            return l;
        }
    }

    ModuleFrame::ModuleFrame (juce::AudioProcessorValueTreeState& a, ModuleDescriptor d)
        : apvts (a), desc (std::move (d))
    {
        assertFitsClass (desc);   // debug guardrail (Story 1.1)
        buildHeader();
        buildBody();

        if (desc.enableParam.isNotEmpty())
        {
            enableValue = apvts.getRawParameterValue (desc.enableParam);
            dimmed = (enableValue != nullptr && enableValue->load() < 0.5f);
            startTimerHz (20);   // poll-and-repaint-on-change (mirrors EnvelopeDisplay)
        }
    }

    ModuleFrame::~ModuleFrame() { stopTimer(); }

    void ModuleFrame::buildHeader()
    {
        titleLabel.setText (desc.title, juce::dontSendNotification);
        titleLabel.setJustificationType (juce::Justification::centredLeft);
        titleLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        addAndMakeVisible (titleLabel);

        if (desc.enableParam.isNotEmpty())
        {
            enableBtn = std::make_unique<juce::ToggleButton>();
            addAndMakeVisible (*enableBtn);
            buttonAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                apvts, desc.enableParam, *enableBtn));
        }

        if (! desc.resetParams.empty())
        {
            resetBtn.setButtonText (juce::String::fromUTF8 ("\xE2\x86\xBA"));   // ↺
            resetBtn.setTooltip ("Reset this module to default");
            auto c = typeColour (desc.type);
            resetBtn.setColour (juce::TextButton::buttonColourId, c.withAlpha (0.25f));
            resetBtn.setColour (juce::TextButton::textColourOffId, c);
            resetBtn.onClick = [this] { doReset(); };
            addAndMakeVisible (resetBtn);
        }
    }

    void ModuleFrame::buildBody()
    {
        const int knobD = sizeClassSpec (desc.sizeClass).knobDiameter;

        for (auto& el : desc.body)
        {
            if (auto* k = std::get_if<Knob> (&el))
            {
                auto* s = static_cast<SynthySlider*> (ownedWidgets.add (new SynthySlider()));
                s->setKnobDiameter (knobD);
                addAndMakeVisible (*s);
                sliderAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                    apvts, k->paramId, *s));
                cells.push_back ({ s, makeCaption (ownedCaptions, k->label), 1 });
                if (auto* cap = cells.back().caption) addAndMakeVisible (*cap);
            }
            else if (auto* c = std::get_if<Combo> (&el))
            {
                auto* box = static_cast<juce::ComboBox*> (ownedWidgets.add (new juce::ComboBox()));
                if (auto* statik = std::get_if<juce::StringArray> (&c->items))
                    box->addItemList (*statik, 1);
                else if (auto* provider = std::get_if<std::function<juce::StringArray()>> (&c->items))
                    if (*provider) box->addItemList ((*provider)(), 1);
                addAndMakeVisible (*box);
                comboAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                    apvts, c->paramId, *box));
                cells.push_back ({ box, makeCaption (ownedCaptions, c->label), 1 });
                if (auto* cap = cells.back().caption) addAndMakeVisible (*cap);
            }
            else if (auto* t = std::get_if<Toggle> (&el))
            {
                auto* btn = static_cast<juce::ToggleButton*> (ownedWidgets.add (new juce::ToggleButton (t->label)));
                addAndMakeVisible (*btn);
                buttonAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                    apvts, t->paramId, *btn));
                cells.push_back ({ btn, nullptr, 1 });
            }
            else if (auto* act = std::get_if<Action> (&el))
            {
                auto* btn = static_cast<juce::TextButton*> (ownedWidgets.add (new juce::TextButton (act->label)));
                btn->onClick = act->onClick;
                addAndMakeVisible (*btn);
                cells.push_back ({ btn, nullptr, 1 });
            }
            else if (auto* fa = std::get_if<FileAction> (&el))
            {
                auto* btn = static_cast<juce::TextButton*> (ownedWidgets.add (new juce::TextButton (fa->label)));
                auto cb = fa->onChoose;
                btn->onClick = [this, cb]
                {
                    fileChooser = std::make_unique<juce::FileChooser> ("Select a file");
                    fileChooser->launchAsync (
                        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                        [cb] (const juce::FileChooser& fc)
                        {
                            auto f = fc.getResult();
                            if (cb && f.existsAsFile()) cb (f);
                        });
                };
                addAndMakeVisible (*btn);
                cells.push_back ({ btn, nullptr, 1 });
            }
            else if (auto* cap = std::get_if<Caption> (&el))
            {
                auto* l = makeCaption (ownedCaptions, cap->text);
                addAndMakeVisible (*l);
                cells.push_back ({ l, nullptr, 1 });
            }
            else if (auto* disp = std::get_if<Display> (&el))
            {
                if (disp->component != nullptr)   // null-safe (1.1 review carry-over)
                {
                    addAndMakeVisible (*disp->component);
                    cells.push_back ({ disp->component, nullptr, juce::jmax (1, disp->slots) });
                }
            }
        }
    }

    void ModuleFrame::doReset()
    {
        for (const auto& id : desc.resetParams)
            if (auto* p = apvts.getParameter (id))
                p->setValueNotifyingHost (p->getDefaultValue());
    }

    void ModuleFrame::resized()
    {
        auto r = getLocalBounds();

        // --- header strip ---
        auto header = r.removeFromTop (kHeaderH).reduced (4, 2);
        if (! desc.resetParams.empty())
            resetBtn.setBounds (header.removeFromRight (20));
        if (enableBtn != nullptr)
            enableBtn->setBounds (header.removeFromRight (24));
        titleLabel.setBounds (header);

        // --- body slot grid (the single body-layout site) ---
        auto body = r.reduced (5, 4);
        const auto spec = sizeClassSpec (desc.sizeClass);
        const int nCols = juce::jmax (1, spec.cols * 3);
        const int total = juce::jmax (1, bodySlots (desc.body));
        const int nRows = (total + nCols - 1) / nCols;
        const int cellW = body.getWidth()  / nCols;
        const int cellH = body.getHeight() / juce::jmax (1, nRows);

        int col = 0, row = 0;
        for (auto& cell : cells)
        {
            const int span = juce::jlimit (1, nCols, cell.slots);
            if (col + span > nCols) { col = 0; ++row; }

            juce::Rectangle<int> cellR (body.getX() + col * cellW,
                                        body.getY() + row * cellH,
                                        cellW * span, cellH);

            if (cell.caption != nullptr)   // knob/combo: widget on top, caption below
            {
                auto cr = cellR.reduced (2);
                cell.caption->setBounds (cr.removeFromBottom (12));
                cell.widget->setBounds (cr);
            }
            else if (cell.widget != nullptr)
            {
                cell.widget->setBounds (cellR.reduced (2));
            }

            col += span;
            if (col >= nCols) { col = 0; ++row; }
        }
    }

    void ModuleFrame::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff2a2f38));
        g.fillRoundedRectangle (r, 6.0f);

        // header bar
        auto header = r.removeFromTop ((float) kHeaderH);
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.fillRect (header);

        // identity strip along the top edge
        g.setColour (typeColour (desc.type));
        g.fillRect (getLocalBounds().removeFromTop (2));

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
        g.drawHorizontalLine (kHeaderH, 0.0f, (float) getWidth());
    }

    void ModuleFrame::paintOverChildren (juce::Graphics& g)
    {
        if (! dimmed) return;
        // dim the BODY only; the header stays lit (FR7)
        auto body = getLocalBounds().withTrimmedTop (kHeaderH);
        g.setColour (juce::Colour (0xff15181d).withAlpha (0.55f));
        g.fillRect (body);
    }

    void ModuleFrame::timerCallback()
    {
        if (enableValue == nullptr) return;
        const bool off = enableValue->load() < 0.5f;
        if (off != dimmed) { dimmed = off; repaint(); }
    }
}
