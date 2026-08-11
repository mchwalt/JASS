#include "ModuleFrame.h"
#include "../HelpTextStore.h"
#include "../../DSP/ModMatrixCatalog.h"   // ModDest::oscParamSlot — per-OSC ring routing

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

        // Make THIS frame's OWN controls never grab keyboard focus on click, so a module revealed
        // AFTER the editor's one-time dropFocus pass (auto-shown on preset load) doesn't steal focus
        // from the on-screen keyboard. ONLY the frame's own widgets are touched (ownedWidgets + the
        // header icons) — NOT external Display components (the KEYBOARD, scope, spectrum, ADSR curve),
        // which the frame merely hosts; the keyboard must stay the sole keyboard-focus holder. Value
        // boxes still grab focus on demand (their TextEditor is created per-edit, unaffected here).
        auto noFocus = [] (juce::Component* c)
        {
            if (c != nullptr) { c->setWantsKeyboardFocus (false); c->setMouseClickGrabsKeyboardFocus (false); }
        };
        for (auto* w : ownedWidgets) noFocus (w);
        noFocus (enableBtn.get());
        noFocus (&resetBtn);
        noFocus (infoBtn.get());

        if (desc.enableParam.isNotEmpty())
            enableValue = apvts.getRawParameterValue (desc.enableParam);

        // Seed the dependent-combo watch cache with the current values so the first poll only fires
        // on a REAL change (e.g. a user picking a MODULE, or a preset load), not on startup.
        lastWatched.resize (desc.comboDeps.size(), 0);
        for (size_t i = 0; i < desc.comboDeps.size(); ++i)
            if (auto* raw = apvts.getRawParameterValue (desc.comboDeps[i].watchParamId))
                lastWatched[i] = (int) raw->load();

        // Apply the mode-dependent knob states NOW (not on the first tick 50 ms later), so an
        // irrelevant knob never flashes up live before greying out.
        updateCondKnobs();

        // Poll-and-repaint-on-change (mirrors EnvelopeDisplay) whenever the module has a
        // dynamic active state — an enable param, a derived predicate, dependent combos, or a
        // per-knob relevance predicate.
        if (enableValue != nullptr || desc.enabledWhen || ! desc.comboDeps.empty() || ! condKnobs.empty())
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

        // Reset ↺ is shown when the module has resettable parameters (a Knob/Combo/Toggle
        // bound to a paramId) OR carries an extra onReset action (e.g. a display's internal
        // time-base). (resized() reserves the slot regardless so header geometry stays uniform.)
        const bool hasResettableParams = std::any_of (desc.body.begin(), desc.body.end(),
            [] (const BodyElement& el)
            {
                if (auto* k = std::get_if<Knob>   (&el)) return k->paramId.isNotEmpty();
                if (auto* c = std::get_if<Combo>  (&el)) return c->paramId.isNotEmpty();
                if (auto* t = std::get_if<Toggle> (&el)) return t->paramId.isNotEmpty();
                return false;
            });
        if (hasResettableParams || desc.onReset)
        {
            resetBtn.setTint (typeColour (desc.type));
            resetBtn.setTooltip ("Reset this module to default");
            resetBtn.onClick = [this] { doReset(); };
            addAndMakeVisible (resetBtn);
        }

        // Online-help info icon (Story 6.1): shown only when a help text exists for this
        // module's help slug (helpId(), which may alias several instances to one text).
        // Clicking it asks the editor (via onHelp) to show the shared HelpPanel; onHelp carries
        // the module id so the panel title stays instance-specific ("LFO 3", not "LFO 1").
        if (HelpTextStore::instance().has (helpId()))
        {
            infoBtn = std::make_unique<IconButton> (IconButton::Kind::Info);
            infoBtn->setTint (typeColour (desc.type));
            infoBtn->setTooltip ("What does this module do?");
            infoBtn->onClick = [this] { if (onHelp) onHelp (desc.id); };
            addAndMakeVisible (*infoBtn);
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

                // Named read-out (PERC NOTE: "Kick" instead of "36"). valueFromText has to be given
                // too, or typing into the box would parse the name back as a number and land on 0.
                if (k->textFromValue)
                {
                    s->textFromValueFunction = k->textFromValue;
                    s->valueFromTextFunction = [] (const juce::String& t) { return t.getDoubleValue(); };
                    s->updateText();
                }

                if (k->modTarget != ModTarget::Off)
                    ringKnobs.push_back ({ s, k->modTarget });

                cells.push_back ({ s, makeCaption (ownedCaptions, k->label), 1 });
                if (auto* cap = cells.back().caption) addAndMakeVisible (*cap);

                // Mode-dependent knob (e.g. STEREO WIDTH/TIME outside Pseudo-Stereo): the timer
                // disables + dims this one knob while the module stays live. Applied there, not
                // here, so the initial state is set by the first poll.
                if (k->activeWhen)
                    condKnobs.push_back ({ s, cells.back().caption, k->activeWhen });

                // Per-knob ON/OFF switch (STEP SEQ steps): a checkbox in this cell's top-right,
                // dimming the knob when off via the same condKnobs path as activeWhen. The
                // predicate is built HERE from the parameter, so no editor injection is needed.
                if (k->toggleParamId.isNotEmpty())
                {
                    auto* tb = static_cast<juce::ToggleButton*> (ownedWidgets.add (new juce::ToggleButton()));
                    tb->setWantsKeyboardFocus (false);   // never steal focus from the on-screen keyboard
                    addAndMakeVisible (*tb);
                    buttonAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                        apvts, k->toggleParamId, *tb));
                    cells.back().toggle = tb;
                    if (auto* v = apvts.getRawParameterValue (k->toggleParamId))
                        condKnobs.push_back ({ s, cells.back().caption,
                                               [v] { return v->load() > 0.5f; } });

                    // Bringing a rest back sounds the step once (15.3), so you hear what you just
                    // restored. The switch has no value of its own — it shares the knob's cell and
                    // previews the knob's pitch. Nothing here releases the note: there is no
                    // gesture to end, so the editor's safety cutoff closes it.
                    if (k->audition)
                        tb->onClick = [tb, s, aud = k->audition]
                        {
                            if (tb->getToggleState())
                                aud ((int) s->getValue(), true);
                        };
                }

                // Preview while editing (Story 15.3): re-trigger on every step of the knob so a
                // drag scrubs the scale, and release when the drag ends. Chained ONTO whatever
                // onValueChange already does — for a transform knob that is the write-back.
                if (k->audition)
                {
                    // A CLICK sounds the step too, without changing it — the quickest way to ask
                    // "what is on this step?". That is why the note is not released unconditionally
                    // at the end of the gesture: a click and its release are milliseconds apart, so
                    // a plain click has to ring on and be closed by the editor's timeout, while a
                    // drag ends when the button does. The flag tells the two apart.
                    auto moved = std::make_shared<bool> (false);
                    s->onValueChange = [s, moved, aud = k->audition, prev = std::move (s->onValueChange)]
                    {
                        if (prev) prev();
                        // Only a real gesture may sound. onValueChange also fires when a preset or
                        // host automation moves the knob, and loading a preset must stay silent.
                        // Dragging keeps the button captured even off the cell, while the wheel
                        // never presses one — so either is enough to call it a gesture.
                        if (s->isMouseButtonDown (true) || s->isMouseOver (true))
                        {
                            *moved = true;
                            aud ((int) s->getValue(), true);
                        }
                    };
                    s->onDragStart = [s, moved, aud = k->audition]
                    {
                        *moved = false;
                        aud ((int) s->getValue(), true);
                    };
                    s->onDragEnd = [moved, aud = k->audition] { if (*moved) aud (0, false); };
                }
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
                if (c->indexIsValue)
                {
                    // Item INDEX == param value. Bypass ComboBoxParameterAttachment (its value is
                    // index/(numItems-1), which mismaps a variable item count against a fixed range —
                    // MOD MATRIX PARAM). Sync combo→param by index; param→combo via refreshCombo.
                    if (auto* raw = apvts.getRawParameterValue (c->paramId))
                        box->setSelectedItemIndex ((int) raw->load(), juce::dontSendNotification);
                    juce::ComboBox* boxPtr = box;
                    const juce::String pid = c->paramId;
                    const auto userHook = c->onUserSelect;   // user-gesture-only (see descriptor)
                    box->onChange = [this, boxPtr, pid, userHook]
                    {
                        const int idx = juce::jmax (0, boxPtr->getSelectedItemIndex());
                        if (auto* pp = apvts.getParameter (pid))
                            pp->setValueNotifyingHost (pp->convertTo0to1 ((float) idx));
                        if (userHook)
                            userHook (idx);
                    };
                    indexValueCombos.push_back ({ c->paramId, box });   // timer resyncs it from the param
                }
                else
                    comboAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                        apvts, c->paramId, *box));
                // A combo needs more width than a knob to show its item text — two slots by
                // default; descriptors may widen (SAMPLER SET shows user-named sets in full).
                cells.push_back ({ box, makeCaption (ownedCaptions, c->label), juce::jmax (1, c->slots) });
                if (auto* cap = cells.back().caption) addAndMakeVisible (*cap);
            }
            else if (auto* t = std::get_if<Toggle> (&el))
            {
                // Body toggle renders like every other captioned element: NAME above, checkbox
                // below (review feedback 2026-08-04 — button-side text was unreadable squeezed
                // between knobs). The button itself carries no text.
                auto* btn = static_cast<juce::ToggleButton*> (ownedWidgets.add (new juce::ToggleButton()));
                addAndMakeVisible (*btn);
                buttonAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                    apvts, t->paramId, *btn));
                cells.push_back ({ btn, makeCaption (ownedCaptions, t->label), 1 });
                if (auto* cap = cells.back().caption) addAndMakeVisible (*cap);
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
                auto startDir  = fa->startFolder;
                auto wildcard  = fa->wildcard;
                auto pickDir   = fa->pickDirectory;
                btn->onClick = [this, cb, refreshes, startDir, wildcard, pickDir]
                {
                    if (fileChooserActive)   // a dialog is already open — ignore re-entrant clicks
                        return;              // (prevents destroying the in-flight chooser mid-callback)
                    fileChooserActive = true;
                    fileChooser = std::make_unique<juce::FileChooser> (pickDir ? "Select a folder" : "Select a file",
                                                                       startDir, wildcard);
                    juce::Component::SafePointer<ModuleFrame> self (this);
                    fileChooser->launchAsync (
                        juce::FileBrowserComponent::openMode
                            | (pickDir ? juce::FileBrowserComponent::canSelectDirectories
                                       : juce::FileBrowserComponent::canSelectFiles),
                        [self, cb, refreshes, pickDir] (const juce::FileChooser& fc)
                        {
                            if (self == nullptr) return;   // frame destroyed while the dialog was open
                            auto f = fc.getResult();
                            if (cb && (pickDir ? f.isDirectory() : f.existsAsFile()))
                            {
                                cb (f);
                                for (const auto& id : refreshes) self->refreshCombo (id);   // re-list bank combo
                            }
                            self->fileChooserActive = false;   // dialog closed — allow the next open
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

        // Extra non-param reset (e.g. the Oscilloscope's time-base, or WAVETABLE dropping its
        // user-loaded banks). Run it, then re-poll any dynamic-provider combo so its item list
        // reflects the post-reset state (e.g. the BANK list shrinks back to the built-ins) and the
        // now-default selection is re-applied.
        if (desc.onReset) desc.onReset();
        for (auto& dc : dynCombos)
            refreshCombo (dc.paramId);
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
        // Info icon slot reserved unconditionally (like enable/reset) so titles line up
        // across every module; only occupied when this module has help (Story 6.1).
        auto infoSlot = header.removeFromRight (20);
        if (infoBtn != nullptr)
            infoBtn->setBounds (infoSlot);
        header.removeFromLeft (11);   // room for the status-LED dot painted at the header's left edge
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

            if (cell.caption != nullptr)   // knob/combo/toggle: NAME caption on top, widget below
            {
                auto cr = cellR.reduced (2);
                const int capH = 13;   // fits the uniform 13pt caption font
                const bool isKnob   = dynamic_cast<SynthySlider*> (cell.widget) != nullptr;
                const bool isButton = dynamic_cast<juce::Button*> (cell.widget) != nullptr;
                // A knob's widget height includes its value box (TextBoxBelow, 14px); a combo
                // is just the short box. Name sits ABOVE, so the block is caption + widget.
                const int wH = isKnob ? (KnobSize::Small + 8 + 14) : kComboH;

                if (isKnob)
                {
                    // Knob block (NAME + rotary + value box) centred vertically in the cell.
                    // ONE fixed knob size everywhere (AD-3), CENTRED horizontally; the slider
                    // draws the rotary in its top square and the value box in the bottom 14 px.
                    const int blockH = capH + wH;
                    const int top = juce::jmax (cr.getY(), cr.getCentreY() - blockH / 2);
                    cell.caption->setBounds (cr.getX(), top, cr.getWidth(), capH);
                    const int sw = juce::jmin (cr.getWidth(), 62);
                    cell.widget->setBounds (cr.getCentreX() - sw / 2, top + capH, sw, wH);
                    // The optional switch rides in the cell's top-right corner, on the caption's
                    // line: it belongs to this knob, so it must not claim a cell of its own.
                    if (cell.toggle != nullptr)
                        cell.toggle->setBounds (cr.getRight() - 16, top - 1, 16, capH + 2);
                }
                else if (isButton)
                {
                    // Captioned toggle: NAME above, the bare checkbox glyph centred below on the
                    // shared widget centre-line (the glyph draws at the LEFT of the bounds, so
                    // a narrow box centred on the cell centres the glyph itself).
                    const int boxY = juce::jmax (cr.getY() + capH, cr.getCentreY() - wH / 2);
                    cell.caption->setBounds (cr.getX(), boxY - capH, cr.getWidth(), capH);
                    cell.widget->setBounds (cr.getCentreX() - 12, boxY, 24, wH);
                }
                else
                {
                    // Combo: box centred on the cell centre-line — EXACTLY like the press
                    // buttons (LOAD WAV etc., which use withSizeKeepingCentre) — so all
                    // interactive widgets sit on one line; its NAME caption sits directly above.
                    // Width is CAPPED and centred, exactly as a knob is capped at 62 px: one width
                    // for every combo in the rack, instead of "whatever this module's cell happens
                    // to be". Wider cells now leave air rather than a stretched box.
                    const int boxY = juce::jmax (cr.getY() + capH, cr.getCentreY() - wH / 2);
                    const int cw = juce::jmin (cr.getWidth(), kComboW * juce::jmax (1, span) / 2);
                    cell.caption->setBounds (cr.getX(), boxY - capH, cr.getWidth(), capH);
                    cell.widget->setBounds (cr.getCentreX() - cw / 2, boxY, cw, wH);
                }
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

        // Module status LED (left of the title): lit green when the module is active (enabled), a
        // hollow grey ring when off. Same language as the MOD MATRIX per-slot dots — one glance tells
        // you what's live. resized() insets the title to leave room. (Painted after the header so it
        // sits on top; updated whenever the frame repaints on an enable change.)
        {
            juce::Rectangle<float> dot (6.0f, kHeaderH * 0.5f - 3.0f, 6.0f, 6.0f);
            if (moduleEnabled()) { g.setColour (juce::Colour (0xff7bd88f)); g.fillEllipse (dot); }
            else                 { g.setColour (juce::Colours::white.withAlpha (0.28f)); g.drawEllipse (dot, 1.0f); }
        }
    }

    void ModuleFrame::paintOverChildren (juce::Graphics& g)
    {
        if (dimmed)
        {
            // dim the BODY only; the header stays lit (FR7)
            auto body = getLocalBounds().withTrimmedTop (kHeaderH);
            g.setColour (juce::Colour (0xff15181d).withAlpha (0.55f));
            g.fillRect (body);
        }

        // Per-slot activity (MOD MATRIX): dim inactive (Off) slots and mark each slot with a dot —
        // filled/lit when the slot is wired, hollow when Off — so it's obvious at a glance which of
        // the routing slots are live. Purely visual (drawn over the children); the controls stay
        // fully clickable so an Off slot can still be assigned.
        if (desc.slotActivity.groupSize > 0 && ! slotActiveCache.empty())
        {
            const int gs = desc.slotActivity.groupSize;
            for (int gi = 0; gi < (int) slotActiveCache.size(); ++gi)
            {
                juce::Rectangle<int> b; bool has = false;
                juce::Component* lastW   = nullptr;   // the group's final control (the AMT knob)
                juce::Component* prevW   = nullptr;   //   …and the one before it (the PARAM combo)
                juce::Label*     lastCap = nullptr;   // their NAME captions (may be null)
                juce::Label*     prevCap = nullptr;
                for (int c = gi * gs; c < gi * gs + gs && c < (int) cells.size(); ++c)
                {
                    if (auto* w = cells[(size_t) c].widget)
                    {
                        auto wb = w->getBounds();
                        if (cells[(size_t) c].caption != nullptr) wb = wb.getUnion (cells[(size_t) c].caption->getBounds());
                        b = has ? b.getUnion (wb) : wb; has = true;
                        prevW = lastW; lastW = w;
                        prevCap = lastCap; lastCap = cells[(size_t) c].caption;
                    }
                }
                if (! has) continue;

                const bool active = slotActiveCache[(size_t) gi] != 0;
                if (! active)   // dim the whole inactive slot
                {
                    g.setColour (juce::Colour (0xff15181d).withAlpha (0.45f));
                    g.fillRect (b.expanded (2));
                }
                // Status dot immediately LEFT of the slot's final control (the AMT knob): lit
                // green = active, hollow grey = Off. It sits ON THE HEIGHT OF the knob's NAME
                // caption ("AMT") — beside the label line, not floating between the widgets —
                // and is anchored to that cell (not the group's corner) so every slot in a row
                // carries its dot right beside its knob. Fallback without a caption: the rotary
                // centre (a SynthySlider's bottom 14 px is its value box, see resized). In tight
                // layouts the dot is re-centred into the actual gap so it never sits ON the
                // neighbouring cell.
                const float d  = 6.0f;
                const auto  wb = lastW->getBounds();
                float cy, anchorL;      // dot centre-line + left edge of the AMT cell's content
                if (lastCap != nullptr)
                {
                    cy      = lastCap->getBounds().toFloat().getCentreY();
                    anchorL = (float) juce::jmin (lastCap->getX(), wb.getX());
                }
                else
                {
                    const int rotaryH = dynamic_cast<SynthySlider*> (lastW) != nullptr ? wb.getHeight() - 14
                                                                                       : wb.getHeight();
                    cy      = (float) wb.getY() + (float) rotaryH * 0.5f;
                    anchorL = (float) wb.getX();
                }
                // Right edge of the previous cell (caption OR widget, whichever reaches further).
                float neighbourR = 0.0f; bool hasNeighbour = false;
                if (prevW   != nullptr) { neighbourR = (float) prevW->getRight(); hasNeighbour = true; }
                if (prevCap != nullptr) { neighbourR = juce::jmax (neighbourR, (float) prevCap->getRight()); hasNeighbour = true; }
                float dotX = anchorL - 3.0f - d;
                if (hasNeighbour && dotX < neighbourR + 1.0f)
                    dotX = (neighbourR + anchorL - d) * 0.5f;
                juce::Rectangle<float> dot (dotX, cy - d * 0.5f, d, d);
                if (active) { g.setColour (juce::Colour (0xff7bd88f)); g.fillEllipse (dot); }
                else        { g.setColour (juce::Colours::white.withAlpha (0.28f)); g.drawEllipse (dot, 1.0f); }
            }
        }
    }

    void ModuleFrame::timerCallback()
    {
        // Dependent combos (MOD MATRIX): when a watched param (MODULE) changes, let the descriptor
        // clamp the dependent param (PARAM) if it is now out of range, then re-list the PARAM combo.
        for (size_t i = 0; i < desc.comboDeps.size(); ++i)
        {
            const auto& dep = desc.comboDeps[i];
            auto* raw = apvts.getRawParameterValue (dep.watchParamId);
            if (raw == nullptr) continue;
            const int now = (int) raw->load();
            if (now == lastWatched[i]) continue;
            lastWatched[i] = now;
            if (dep.onWatchChanged) dep.onWatchChanged (now);   // e.g. clamp the PARAM param
            refreshCombo (dep.refreshParamId);                  // re-list the dependent combo
        }

        // Resync indexIsValue combos whose param changed WITHOUT a MODULE change (preset load / host
        // automation) — they have no attachment, so nothing else would update their displayed item.
        for (auto& iv : indexValueCombos)
        {
            if (iv.second == nullptr) continue;
            if (auto* raw = apvts.getRawParameterValue (iv.first))
            {
                const int want = (int) raw->load();
                if (want != iv.second->getSelectedItemIndex() && want < iv.second->getNumItems())
                    iv.second->setSelectedItemIndex (want, juce::dontSendNotification);
            }
        }

        // Per-slot activity (MOD MATRIX): recompute each slot's active flag; repaint on any change so
        // paintOverChildren can dim the inactive slots and draw the lit/hollow dots.
        if (desc.slotActivity.groupSize > 0 && desc.slotActivity.isActive)
        {
            const int gs        = desc.slotActivity.groupSize;
            const int numGroups = (int) cells.size() / gs;
            bool changed = false;
            if ((int) slotActiveCache.size() != numGroups) { slotActiveCache.assign ((size_t) numGroups, (char) -1); changed = true; }
            for (int gi = 0; gi < numGroups; ++gi)
            {
                const char a = desc.slotActivity.isActive (gi) ? (char) 1 : (char) 0;
                if (slotActiveCache[(size_t) gi] != a) { slotActiveCache[(size_t) gi] = a; changed = true; }
            }
            if (changed) repaint();
        }

        updateCondKnobs();

        if (enableValue == nullptr && ! desc.enabledWhen) return;
        const bool en = moduleEnabled();
        // A derived (predicate) enabler has no attachment — keep its display toggle in sync.
        if (enableValue == nullptr && enableBtn != nullptr)
            enableBtn->setToggleState (en, juce::dontSendNotification);
        const bool off = ! en;
        if (off != dimmed) { dimmed = off; repaint(); }
    }

    void ModuleFrame::updateCondKnobs()
    {
        // A knob that does not apply in the current mode is switched off rather than hidden: the
        // module keeps its layout (no reflow when the mode changes) and the greyed knob still shows
        // its stored value. setEnabled stops the mouse; the alpha carries the visual cue, since a
        // custom slider LookAndFeel need not honour isEnabled by itself.
        for (auto& ck : condKnobs)
        {
            const char want = ck.predicate() ? (char) 1 : (char) 0;
            if (ck.active == want) continue;
            ck.active = want;
            const bool on = (want == 1);
            if (ck.slider != nullptr)
            {
                ck.slider->setEnabled (on);
                ck.slider->setAlpha (on ? 1.0f : 0.35f);
            }
            if (ck.caption != nullptr)
                ck.caption->setAlpha (on ? 1.0f : 0.35f);
        }
    }

    void ModuleFrame::updateLiveFeed (const LiveModFeed& ringByTarget, double playedRatio)
    {
        const bool en = moduleEnabled();

        // Modulation rings (Story 8.1): every knob whose modTarget currently receives
        // periodic (LFO) modulation shows the moving ring, driven by that target's summed
        // amount; the rest are driven to 0. A disabled module shows no rings. The
        // SynthySlider itself repaints only on a meaningful change (NFR5).
        // Per-OSC (Epic 8.3): an OSC module's FREQ/AMP/DETUNE knob also picks up its own
        // oscillator's ring amount, so a routing to a single OSC lights only that OSC's knob
        // (the global "Alle OSC" amount still arrives via byTarget).
        int oscIdx = -1;
        if (desc.id.length() == 4 && desc.id.startsWith ("osc"))
            if (const int d = desc.id[3] - '0'; d >= 1 && d <= 3) oscIdx = d - 1;

        for (auto& rk : ringKnobs)
        {
            float amt = ringByTarget.byTarget[(size_t) rk.target];
            if (oscIdx >= 0)
                if (const int slot = ModDest::oscParamSlot (rk.target); slot >= 0)
                    amt += ringByTarget.osc[oscIdx][slot];
            rk.slider->setModAmount (en ? amt : 0.0f);
        }

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
