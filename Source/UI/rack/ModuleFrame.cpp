#include "ModuleFrame.h"

namespace rack
{
    namespace
    {
        // typeColour() is shared from ModuleDescriptor.h (single source of the palette).

        juce::Label* makeCaption (juce::OwnedArray<juce::Label>& store, const juce::String& text)
        {
            auto* l = store.add (new juce::Label ({}, text));
            l->setJustificationType (juce::Justification::centred);
            // Font comes from SynthyLookAndFeel::getLabelFont (the one uniform UI size);
            // no per-label size here so captions match the value boxes and combos.
            l->setInterceptsMouseClicks (false, false);
            return l;
        }

        // Mirror a parameter's (float) range onto a (double) Slider so a decoupled
        // display-transform knob spans and skews exactly like its param (Story 1.4).
        juce::NormalisableRange<double> toDoubleRange (const juce::NormalisableRange<float>& r)
        {
            return { (double) r.start, (double) r.end, (double) r.interval,
                     (double) r.skew, r.symmetricSkew };
        }
    }

    ModuleFrame::ModuleFrame (juce::AudioProcessorValueTreeState& a, ModuleDescriptor d)
        : apvts (a), desc (std::move (d))
    {
        assertFitsClass (desc);   // debug guardrail (Story 1.1)
        buildHeader();
        buildBody();

        if (desc.enableParam.isNotEmpty())
            enableValue = apvts.getRawParameterValue (desc.enableParam);

        // Poll-and-repaint-on-change (mirrors EnvelopeDisplay) whenever the module has a
        // dynamic active state — either a single enable param or a derived predicate.
        if (enableValue != nullptr || desc.enabledWhen)
        {
            dimmed = ! moduleEnabled();
            startTimerHz (20);
        }
    }

    ModuleFrame::~ModuleFrame() { stopTimer(); }

    void ModuleFrame::buildHeader()
    {
        titleLabel.setText (desc.title, juce::dontSendNotification);
        titleLabel.setJustificationType (juce::Justification::centredLeft);
        titleLabel.setComponentID ("moduleTitle");   // SynthyLookAndFeel gives this its bold font
        addAndMakeVisible (titleLabel);

        // The enabler toggle is shown only when the module HAS an enable source: a real
        // enable param (interactive) or a derived predicate (read-only, shows the computed
        // state, e.g. a Mix-Mode-style osc1&&osc2). A module with NEITHER — a pure passive
        // display like the Scope/Spectrum — shows no toggle (there is nothing to enable).
        // Header geometry stays uniform: resized() reserves the enable slot unconditionally.
        if (desc.enableParam.isNotEmpty())
        {
            enableBtn = std::make_unique<juce::ToggleButton>();
            addAndMakeVisible (*enableBtn);
            buttonAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                apvts, desc.enableParam, *enableBtn));
        }
        else if (desc.enabledWhen)
        {
            // Derived-only enabler (no user param): read-only display of the computed state.
            enableBtn = std::make_unique<juce::ToggleButton>();
            addAndMakeVisible (*enableBtn);
            enableBtn->setToggleState (moduleEnabled(), juce::dontSendNotification);
            enableBtn->setInterceptsMouseClicks (false, false);
        }

        // Reset ↺ is shown only when the module has resettable parameters (a Knob/Combo/
        // Toggle bound to a paramId). A pure passive display (Scope/Spectrum) has none, so
        // it gets no reset — matching its lack of an enabler. (resized() still reserves the
        // slot so header geometry stays uniform.)
        const bool hasResettableParams = std::any_of (desc.body.begin(), desc.body.end(),
            [] (const BodyElement& el)
            {
                if (auto* k = std::get_if<Knob>   (&el)) return k->paramId.isNotEmpty();
                if (auto* c = std::get_if<Combo>  (&el)) return c->paramId.isNotEmpty();
                if (auto* t = std::get_if<Toggle> (&el)) return t->paramId.isNotEmpty();
                return false;
            });
        if (hasResettableParams)
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
                // Anatomy: NAME caption ABOVE the knob, numeric VALUE box BELOW it (like the
                // legacy/C# UI) — the value read-out is existing behaviour and must survive the
                // migration (FR13). The box is editable in place (double-click) and shows the
                // parameter's unit suffix when it declares one.
                s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 14);
                if (auto* rp = apvts.getParameter (k->paramId))
                    if (const auto unit = rp->getLabel(); unit.isNotEmpty())
                        s->setTextValueSuffix (" " + unit);
                addAndMakeVisible (*s);

                // A knob with a FULLY-set display-transform pair is DECOUPLED from its
                // param (AD-4): it shows a derived value (base × ratio) and writes the
                // base back on edit. A missing OR half-set pair => normal bound knob.
                const bool hasXform = (k->toDisplay != nullptr && k->fromDisplay != nullptr);
                if (hasXform)
                {
                    // Mirror the param's range/skew so the decoupled knob feels identical.
                    if (auto* rp = apvts.getParameter (k->paramId))
                        s->setNormalisableRange (toDoubleRange (rp->getNormalisableRange()));

                    const auto id       = k->paramId;
                    const auto fromDisp = k->fromDisplay;
                    s->onValueChange = [this, s, id, fromDisp]
                    {
                        if (auto* p = apvts.getParameter (id))
                        {
                            // ratio<=0 (no note) or disabled module => identity, no bad write-back.
                            const double er   = (moduleEnabled() && liveRatio > 0.0) ? liveRatio : 1.0;
                            const double base = fromDisp (s->getValue(), er);
                            p->setValueNotifyingHost (p->convertTo0to1 ((float) base));
                        }
                    };
                    // Initialise the display from the current base (before the first live tick).
                    if (auto* raw = apvts.getRawParameterValue (id))
                        s->setValue ((double) raw->load(), juce::dontSendNotification);
                    xformKnobs.push_back ({ s, id, k->toDisplay, k->fromDisplay });
                }
                else
                {
                    sliderAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                        apvts, k->paramId, *s));
                }

                if (k->modTarget != ModTarget::None)
                    ringKnobs.push_back ({ s, k->modTarget });

                cells.push_back ({ s, makeCaption (ownedCaptions, k->label), 1 });
                if (auto* cap = cells.back().caption) addAndMakeVisible (*cap);
            }
            else if (auto* c = std::get_if<Combo> (&el))
            {
                auto* box = static_cast<juce::ComboBox*> (ownedWidgets.add (new juce::ComboBox()));
                if (auto* statik = std::get_if<juce::StringArray> (&c->items))
                    box->addItemList (*statik, 1);
                else if (auto* provider = std::get_if<std::function<juce::StringArray()>> (&c->items))
                {
                    if (*provider) box->addItemList ((*provider)(), 1);
                    dynCombos.push_back ({ c->paramId, box, *provider });   // re-pollable via refreshCombo
                }
                addAndMakeVisible (*box);
                comboAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                    apvts, c->paramId, *box));
                // A combo needs more width than a knob to show its item text — give it
                // two internal slots so the dropdown isn't cramped/truncated.
                cells.push_back ({ box, makeCaption (ownedCaptions, c->label), 2 });
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
                auto onClick   = act->onClick;
                auto refreshes = act->refreshes;
                btn->onClick = [this, onClick, refreshes]
                {
                    if (onClick) onClick();
                    for (const auto& id : refreshes) refreshCombo (id);   // declarative combo refresh
                };
                actionButtons.push_back (btn);   // so a shortcut (e.g. spacebar) can trigger it
                addAndMakeVisible (*btn);
                cells.push_back ({ btn, nullptr, 1 });
            }
            else if (auto* fa = std::get_if<FileAction> (&el))
            {
                auto* btn = static_cast<juce::TextButton*> (ownedWidgets.add (new juce::TextButton (fa->label)));
                auto cb        = fa->onChoose;
                auto refreshes = fa->refreshes;
                btn->onClick = [this, cb, refreshes]
                {
                    if (fileChooserActive)   // a dialog is already open — ignore re-entrant clicks
                        return;              // (prevents destroying the in-flight chooser mid-callback)
                    fileChooserActive = true;
                    fileChooser = std::make_unique<juce::FileChooser> ("Select a file");
                    fileChooser->launchAsync (
                        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                        [this, cb, refreshes] (const juce::FileChooser& fc)
                        {
                            auto f = fc.getResult();
                            if (cb && f.existsAsFile())
                            {
                                cb (f);
                                for (const auto& id : refreshes) refreshCombo (id);   // re-list bank combo
                            }
                            fileChooserActive = false;   // dialog closed — allow the next open
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
        // Reset ALL of the module's parameters to their factory defaults, EXCEPT the
        // enable flag. The body is the single source of truth for which params the
        // module owns (every Knob/Combo/Toggle carries its paramId), so we derive the
        // reset set from it — this can never drift out of sync with the module's
        // controls the way a hand-maintained list would.
        auto resetId = [this] (const juce::String& id)
        {
            if (id.isEmpty() || id == desc.enableParam)
                return;
            if (auto* p = apvts.getParameter (id))
                p->setValueNotifyingHost (p->getDefaultValue());
        };

        for (const auto& el : desc.body)
        {
            if (auto* k = std::get_if<Knob>   (&el)) resetId (k->paramId);
            else if (auto* c = std::get_if<Combo>  (&el)) resetId (c->paramId);
            else if (auto* t = std::get_if<Toggle> (&el)) resetId (t->paramId);
        }
    }

    void ModuleFrame::resized()
    {
        auto r = getLocalBounds();

        // --- header strip ---
        // Reserve the right-side slots UNCONDITIONALLY so the title region is identical
        // in every module (uniform header geometry): reset is always present, and the
        // enable slot stays reserved even when a module has no toggle — so titles and
        // ↺ buttons line up across the whole rack.
        auto header = r.removeFromTop (kHeaderH).reduced (4, 2);
        // The enable toggle sits at the FAR top-right; the reset ↺ directly to its left.
        // Both slots are reserved unconditionally so header geometry is identical across
        // every module (a module without an enable just leaves that slot empty).
        auto enableSlot = header.removeFromRight (24);
        if (enableBtn != nullptr)
            enableBtn->setBounds (enableSlot);
        resetBtn.setBounds (header.removeFromRight (20));
        titleLabel.setBounds (header);

        // --- body slot grid (the single body-layout site) ---
        auto body = r.reduced (5, 4);
        const auto spec = sizeClassSpec (desc.sizeClass);

        // The body fills the module width from its CONTENT, decoupled from the grid column
        // span AND from the knob size (PROTOTYPE): lay the content slots (combo=2, display=N,
        // else 1) across `units` rows. nCols is derived so the cells fill the full width with
        // no trailing empty cells — the cause of the old "module doesn't use its space" look.
        const int units = juce::jmax (1, spec.units);
        int rawTotal = 0;
        for (auto& cell : cells)
            rawTotal += juce::jmax (1, cell.slots);
        rawTotal = juce::jmax (1, rawTotal);
        const int nCols = juce::jmax (1, (rawTotal + units - 1) / units);   // ceil(rawTotal / units)

        // Row count from the cells we actually place (sum of their clamped spans), NOT
        // from bodySlots(desc.body): a skipped null-Display would inflate the count and
        // leave a phantom gap. (deferred 1.2 review item)
        int total = 0;
        for (auto& cell : cells)
            total += juce::jlimit (1, nCols, cell.slots);
        total = juce::jmax (1, total);
        const int nRows = (total + nCols - 1) / nCols;
        const int cellW = body.getWidth()  / nCols;
        const int cellH = body.getHeight() / juce::jmax (1, nRows);

        int col = 0, row = 0;
        for (auto& cell : cells)
        {
            const int span = juce::jlimit (1, nCols, cell.slots);
            if (col + span > nCols) { col = 0; ++row; }

            // Never lay a (spanning) cell out below the body's bottom edge: bound the
            // row to the grid we sized for. (deferred 1.2 review item)
            const int placeRow = juce::jmin (row, nRows - 1);
            juce::Rectangle<int> cellR (body.getX() + col * cellW,
                                        body.getY() + placeRow * cellH,
                                        cellW * span, cellH);

            if (cell.caption != nullptr)   // knob/combo: NAME caption on top, widget below
            {
                auto cr = cellR.reduced (2);
                const int capH = 13;   // fits the uniform 13pt caption font
                const bool isKnob = dynamic_cast<SynthySlider*> (cell.widget) != nullptr;
                // A knob's widget height includes its value box (TextBoxBelow, 14px); a combo
                // is just the short box. Name sits ABOVE, so the block is caption + widget.
                const int wH = isKnob ? (KnobSize::Small + 8 + 14) : kComboH;
                const int blockH = capH + wH;
                // Centre the caption+widget block vertically so the name sits DIRECTLY above
                // its widget — never floating to the top of a tall (e.g. L) cell.
                const int top = juce::jmax (cr.getY(), cr.getCentreY() - blockH / 2);

                cell.caption->setBounds (cr.getX(), top, cr.getWidth(), capH);   // NAME on top

                if (isKnob)
                {
                    // ONE fixed knob size everywhere (AD-3), CENTRED — the slider draws the
                    // rotary in its top square and the value box in the bottom 14 px. Width
                    // a touch wider than the knob so the value text isn't truncated.
                    const int sw = juce::jmin (cr.getWidth(), 62);
                    cell.widget->setBounds (cr.getCentreX() - sw / 2, top + capH, sw, wH);
                }
                else
                    // Combo: short (half-height) + wide (2 slots), LEFT-aligned, under its name.
                    cell.widget->setBounds (cr.getX(), top + capH, cr.getWidth(), wH);
            }
            else if (cell.widget != nullptr)
            {
                auto cr = cellR.reduced (2);
                if (dynamic_cast<juce::Button*> (cell.widget) != nullptr)
                    // Toggle / Action / FileAction button: fixed height, capped width,
                    // centred — so it never stretches to fill a tall (e.g. L) cell.
                    cell.widget->setBounds (cr.withSizeKeepingCentre (juce::jmin (cr.getWidth(), kButtonW),
                                                                      juce::jmin (cr.getHeight(), kButtonH)));
                else
                    cell.widget->setBounds (cr);   // static Caption text fills the cell
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
        if (enableValue == nullptr && ! desc.enabledWhen) return;
        const bool en = moduleEnabled();
        // A derived (predicate) enabler has no attachment — keep its display toggle in sync.
        if (enableValue == nullptr && enableBtn != nullptr)
            enableBtn->setToggleState (en, juce::dontSendNotification);
        const bool off = ! en;
        if (off != dimmed) { dimmed = off; repaint(); }
    }

    void ModuleFrame::updateLiveFeed (bool lfoOn, ModTarget activeTarget, float lfoValue, double playedRatio)
    {
        const bool en = moduleEnabled();

        // Modulation rings: only the enabled module whose knob matches the active LFO
        // target shows the moving ring; every other ring knob is driven to 0. The
        // SynthySlider itself repaints only on a meaningful change (NFR5).
        for (auto& rk : ringKnobs)
            rk.slider->setModAmount ((lfoOn && en && rk.target == activeTarget) ? lfoValue : 0.0f);

        // Display transforms: show base × ratio. Guard (AD-4): a disabled module or a
        // non-positive ratio (no note sounding) => identity, so we never divide a stale
        // ratio nor corrupt the base param.
        liveRatio = playedRatio;
        const double er = (en && playedRatio > 0.0) ? playedRatio : 1.0;
        for (auto& xk : xformKnobs)
        {
            if (xk.slider->isMouseButtonDown())   // don't fight the user mid-drag
                continue;
            if (auto* raw = apvts.getRawParameterValue (xk.paramId))
                xk.slider->setValue (xk.toDisplay ((double) raw->load(), er), juce::dontSendNotification);
        }
    }

    void ModuleFrame::refreshCombo (const juce::String& paramId)
    {
        for (auto& dc : dynCombos)
        {
            if (dc.paramId != paramId || dc.box == nullptr || ! dc.provider)
                continue;

            dc.box->clear (juce::dontSendNotification);
            dc.box->addItemList (dc.provider(), 1);   // item ids are 1-based (index + 1)

            // Re-apply the param's current selection so the ComboBoxAttachment and the box
            // don't drift after we repopulated the items (clearing doesn't touch the param).
            if (auto* raw = apvts.getRawParameterValue (paramId))
                dc.box->setSelectedId ((int) raw->load() + 1, juce::dontSendNotification);
        }
    }

    void ModuleFrame::clickFirstAction()
    {
        if (! actionButtons.empty() && actionButtons.front() != nullptr)
            actionButtons.front()->triggerClick();   // press animation + fires onClick
    }
}
