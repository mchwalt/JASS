#include "ModuleFrame.h"
#include "../HelpTextStore.h"
#include "../../DSP/ModMatrixCatalog.h"   // ModDest::oscParamSlot — per-OSC ring routing
#include "../../Audio/PresetIO.h"         // presetBaseline01 — double-click = the preset's value

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

        // Three-state step switch (15.2): a click cycles OFF (rest) → ON → ACCENTED → OFF — the
        // TR-909's second-press gesture — over TWO bool parameters (the step's on/off and its
        // accent). ParameterAttachments keep it repainting on preset/host changes WITHOUT firing
        // the cycle: only a real mouse click advances the state (the #56 lesson — a loader replay
        // must never read as a gesture). Drawn by hand so the third state fits the checkbox look:
        // empty = rest, tick = on, filled + tick = accented.
        class StepSwitch : public juce::Button
        {
        public:
            StepSwitch (juce::AudioProcessorValueTreeState& state,
                        const juce::String& onId, const juce::String& accId)
                : juce::Button ({}),
                  onValue  (state.getRawParameterValue (onId)),
                  accValue (state.getRawParameterValue (accId)),
                  onAtt  (*state.getParameter (onId),  [this] (float) { repaint(); }),
                  accAtt (*state.getParameter (accId), [this] (float) { repaint(); })
            {
            }

            bool isOn()       const { return onValue  != nullptr && onValue->load()  > 0.5f; }
            bool isAccented() const { return accValue != nullptr && accValue->load() > 0.5f; }

            void clicked() override
            {
                // Complete gestures, so a host records single automation events. The accent is
                // written FIRST on the way up (on+accent land as one audible state change) and
                // cleared first on the way out.
                const bool on = isOn(), acc = isAccented();
                if (! on)       { accAtt.setValueAsCompleteGesture (0.0f); onAtt.setValueAsCompleteGesture (1.0f); }
                else if (! acc) { accAtt.setValueAsCompleteGesture (1.0f); }
                else            { accAtt.setValueAsCompleteGesture (0.0f); onAtt.setValueAsCompleteGesture (0.0f); }
            }

            // The drawn box keeps the button's ORIGINAL size; the bounds around it are a larger
            // hit target (the little box was too small to aim at — maintainer 2026-08-26). The
            // glyph stays anchored in the top-right corner, exactly where the whole button used
            // to sit, so the bigger target changes nothing visually.
            static constexpr int kGlyphW = 16, kGlyphH = 15;

            void paintButton (juce::Graphics& g, bool over, bool down) override
            {
                auto r = getLocalBounds().removeFromTop (kGlyphH).removeFromRight (kGlyphW)
                                         .toFloat().reduced (3.0f);
                const auto tick = findColour (juce::ToggleButton::tickColourId);
                const auto box  = findColour (juce::ToggleButton::tickDisabledColourId);
                if (isAccented())
                {
                    g.setColour (tick.withAlpha (0.45f));   // the "hot" fill behind the tick
                    g.fillRoundedRectangle (r, 3.0f);
                }
                g.setColour (over || down ? box.brighter (0.4f) : box);
                g.drawRoundedRectangle (r, 3.0f, 1.0f);
                if (isOn())
                {
                    g.setColour (tick);
                    auto t = r.reduced (2.5f);
                    juce::Path p;
                    p.startNewSubPath (t.getX(), t.getCentreY());
                    p.lineTo (t.getX() + t.getWidth() * 0.35f, t.getBottom() - 1.0f);
                    p.lineTo (t.getRight(), t.getY());
                    g.strokePath (p, juce::PathStrokeType (1.6f));
                }
            }

        private:
            std::atomic<float>* onValue;
            std::atomic<float>* accValue;
            juce::ParameterAttachment onAtt, accAtt;
        };
    }

    ModuleFrame::ModuleFrame (juce::AudioProcessorValueTreeState& a, ModuleDescriptor d)
        : apvts (a), desc (std::move (d))
    {
        assertFitsClass (desc);   // debug guardrail (Story 1.1)
        buildHeader();
        headerButtonAtts = buttonAtt.size();   // 16.3: a body rebuild clears only what buildBody adds
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

        // Pattern-length marker (16.2): the LEN param whose value places the red line.
        if (desc.lenMarkerLengthParam.isNotEmpty())
            markerLen = apvts.getRawParameterValue (desc.lenMarkerLengthParam);

        // Poll-and-repaint-on-change (mirrors EnvelopeDisplay) whenever the module has a
        // dynamic active state — an enable param, a derived predicate, dependent combos, a
        // per-knob relevance predicate, or the pattern-length marker.
        if (enableValue != nullptr || desc.enabledWhen || ! desc.comboDeps.empty()
            || ! condKnobs.empty() || ! markedKnobs.empty() || markerLen != nullptr
            || desc.paging.pageCount > 1)
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

        // Row toggle (15.7): a header latch that flips every altParamId knob between its two
        // meanings (STEP SEQ: PITCH ⇄ GATE — the BeatStep's "the knob row cycles its meaning").
        // It lives beside the reset because it is a VIEW switch, not a parameter: nothing about
        // it lands in presets, and a preset load never flips what the user is looking at.
        if (desc.altRowTitle.isNotEmpty())
        {
            altRowBtn = std::make_unique<juce::TextButton> (desc.altRowTitle);
            altRowBtn->setClickingTogglesState (true);
            altRowBtn->setTooltip ("Flip the step knobs between PITCH and " + desc.altRowTitle);
            altRowBtn->setWantsKeyboardFocus (false);
            altRowBtn->onClick = [this] { altRowActive = altRowBtn->getToggleState(); applyAltRow(); };
            addAndMakeVisible (*altRowBtn);
        }

        // Header one-shot actions (15.8): STEP SEQ's LOAD MIDI / SAVE MIDI. Plain buttons — the
        // work lives in the descriptor's callback (the editor opens the chooser and runs the
        // import/export); the frame only puts a clickable, tooltipped word in the title bar.
        for (const auto& act : desc.headerActions)
        {
            auto* b = actionBtns.add (new juce::TextButton (act.title));
            b->setTooltip (act.tooltip);
            b->setWantsKeyboardFocus (false);
            if (act.onToggle)   // a latch, like the row toggle — the callback gets the state
            {
                b->setClickingTogglesState (true);
                b->onClick = [b, cb = act.onToggle] { cb (b->getToggleState()); };
            }
            else
                b->onClick = act.onClick;
            addAndMakeVisible (*b);
        }

        // Step pages (16.3): '<' / read-out / '>' plus the FOLLOW latch. Prev/next rather than
        // one button per page (maintainer 2026-09-01: "morgen komme ich auf die Idee, weitere
        // Pages hinzuzufügen") — the header stays this size whatever kMaxSteps grows to. The
        // read-out says shown/total and, with a dot, the page the pattern is playing on.
        // Stepping by hand drops FOLLOW so the display cannot flip away under an editing cursor
        // (the Logic/Cubase catch convention); re-latching jumps to the playing page and resumes.
        if (desc.paging.pageCount > 1)
        {
            auto makeStep = [this] (std::unique_ptr<juce::TextButton>& slot,
                                    const juce::String& glyph, int dir)
            {
                slot = std::make_unique<juce::TextButton> (glyph);
                slot->setTooltip (juce::String (dir < 0 ? "Previous" : "Next")
                                  + " page of steps (stepping by hand pauses FOLLOW)");
                slot->setWantsKeyboardFocus (false);
                slot->onClick = [this, dir]
                {
                    const int p = shownPage() + dir;
                    if (desc.paging.setPage && p >= 0 && p < desc.paging.pageCount)
                        desc.paging.setPage (p);
                };
                addAndMakeVisible (*slot);
            };
            makeStep (pagePrevBtn, "<", -1);
            makeStep (pageNextBtn, ">",  1);
            pageLabel = std::make_unique<juce::Label>();
            pageLabel->setJustificationType (juce::Justification::centred);
            pageLabel->setInterceptsMouseClicks (false, false);
            pageLabel->setTooltip ("Shown page / pages; the dot marks the page that is playing");
            addAndMakeVisible (*pageLabel);

            followBtn = std::make_unique<juce::TextButton> ("FOLLOW");
            followBtn->setClickingTogglesState (true);
            followBtn->setToggleState (desc.paging.getFollow ? desc.paging.getFollow() : true,
                                       juce::dontSendNotification);
            followBtn->setTooltip ("Follow the playhead: the shown page flips with the running "
                                   "pattern. Picking a page by hand pauses it; latch again to resume.");
            followBtn->setWantsKeyboardFocus (false);
            followBtn->onClick = [this]
            {
                if (desc.paging.setFollow) desc.paging.setFollow (followBtn->getToggleState());
            };
            addAndMakeVisible (*followBtn);
        }

        // Display fold latch (16.2): toggles the module between its full size (Display cells
        // visible) and collapsedSize. Toggle ON = shown, so the lit button reads as "the curve
        // is up". The rack is told through onFootprintChanged and re-packs live.
        if (desc.collapseTitle.isNotEmpty())
        {
            collapsedState = desc.startCollapsed;
            collapseBtn = std::make_unique<juce::TextButton> (desc.collapseTitle);
            collapseBtn->setClickingTogglesState (true);
            collapseBtn->setToggleState (! collapsedState, juce::dontSendNotification);
            collapseBtn->setTooltip ("Show or fold the " + desc.collapseTitle.toLowerCase()
                                     + " display; the rack re-packs around it");
            collapseBtn->setWantsKeyboardFocus (false);
            collapseBtn->onClick = [this]
            {
                collapsedState = ! collapseBtn->getToggleState();
                applyCollapsed();
                if (onFootprintChanged) onFootprintChanged();
            };
            addAndMakeVisible (*collapseBtn);
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
        // Seed only — resized() sets the real diameter per cell (one standard, capped by the cell).
        const int knobD = KnobSize::Standard;

        // Step pages (16.3): the body is built FOR the shown page — a paged knob cell binds the
        // shown page's params instead of page A's. desc.body itself always keeps the page-A ids
        // (the editor's injected lambdas translate themselves through the shared page state).
        builtPage = shownPage();
        hasPagedCells = false;

        for (auto& el : desc.body)
        {
            if (auto* k0 = std::get_if<Knob> (&el))
            {
                // Page translation (16.3): a shallow copy with the four bound ids shifted by
                // builtPage * stepsPerPage. Everything below reads through `k`, so page A and
                // page D are built by literally the same code.
                Knob pagedCopy;
                const Knob* k = k0;
                if (idIsPaged (k0->paramId))
                {
                    hasPagedCells = true;
                    pagedCopy = *k0;
                    pagedCopy.paramId = pagedId (pagedCopy.paramId);
                    if (pagedCopy.toggleParamId.isNotEmpty()) pagedCopy.toggleParamId = pagedId (pagedCopy.toggleParamId);
                    if (pagedCopy.accentParamId.isNotEmpty()) pagedCopy.accentParamId = pagedId (pagedCopy.accentParamId);
                    if (pagedCopy.altParamId.isNotEmpty())    pagedCopy.altParamId    = pagedId (pagedCopy.altParamId);
                    // The CAPTION counts along too: a step knob's label is its number, so page C
                    // must read 97..144, exactly as PERC's self-drawn number strip does.
                    if (pagedCopy.label.isNotEmpty() && pagedCopy.label.containsOnly ("0123456789"))
                        pagedCopy.label = juce::String (pagedCopy.label.getIntValue()
                                                        + builtPage * desc.paging.stepsPerPage);
                    k = &pagedCopy;
                }
                auto* s = static_cast<SynthySlider*> (ownedWidgets.add (new SynthySlider()));
                s->setKnobDiameter (knobD);
                if (k->coarseStep > 0) s->setCoarseStep (k->coarseStep);   // LEN: 8s bare, 1s shifted
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
                    // JUCE's SliderAttachment silently arms "double-click = parameter default".
                    // With click-to-audition that is a trap — two quick clicks on a STEP SEQ knob
                    // wiped its pitch back to 0 (maintainer 2026-08-26: nobody needs this). Off,
                    // rack-wide: a reset gesture that can fire by accident is not a reset gesture.
                    s->setDoubleClickReturnValue (false, 0.0);
                    // Instead, double-click restores the LOADED preset's value (same day's wish):
                    // while a knob is untouched that IS its current value — a no-op — and after
                    // twisting it while comparing, it is the one-gesture way back. Not wired for
                    // the decoupled display-transform knobs above: their shown value rides the
                    // live ratio, so "the preset's value" is not what their slider displays.
                    s->presetBaseline = [&a = apvts, id = k->paramId]() -> double
                    {
                        if (PresetIO::presetBaseline01)
                            if (const float v01 = PresetIO::presetBaseline01 (id); v01 >= 0.0f)
                                if (auto* p = a.getParameter (id))
                                    return (double) p->convertFrom0to1 (v01);
                        return std::numeric_limits<double>::quiet_NaN();
                    };
                }

                // Named read-out (PERC NOTE: "Kick" instead of "36"). valueFromText has to be given
                // too, or typing into the box would parse the name back as a number and land on 0.
                if (k->textFromValue)
                {
                    s->textFromValueFunction = k->textFromValue;
                    s->valueFromTextFunction = [] (const juce::String& t) { return t.getDoubleValue(); };
                    s->updateText();
                    // The value box can be narrower than the name (PERC NOTE: "HH Closed · 42"):
                    // hovering the knob shows the full read-out as a tooltip, kept in sync below
                    // and in refreshNamedReadouts. tooltipFromValue may say MORE than the box
                    // (STEP SEQ: box "E1", hover "E1 · 40"); default is the box text. Chained like
                    // the audition hook — a transform knob has already claimed onValueChange.
                    s->tooltipFromValue = k->tooltipFromValue ? k->tooltipFromValue : k->textFromValue;
                    s->refreshTooltip();
                    s->onValueChange = [s, prev = std::move (s->onValueChange)]
                    {
                        if (prev) prev();
                        s->refreshTooltip();
                    };
                }

                if (k->modTarget != ModTarget::Off)
                    ringKnobs.push_back ({ s, k->modTarget });

                cells.push_back ({ s, makeCaption (ownedCaptions, k->label), juce::jmax (1, k->slots) });
                if (auto* cap = cells.back().caption) addAndMakeVisible (*cap);

                // Pattern-length marker (16.2): remember the step-knob cells in figure order —
                // bodyOrder lists them musically, so index LEN-1 IS the figure's last step.
                if (desc.lenMarkerStepPrefix.isNotEmpty() && k->paramId.startsWith (desc.lenMarkerStepPrefix))
                    markerCells.push_back ((int) cells.size() - 1);

                // Mode-dependent knob (e.g. STEREO WIDTH/TIME outside Pseudo-Stereo): the timer
                // disables + dims this one knob while the module stays live. Applied there, not
                // here, so the initial state is set by the first poll.
                if (k->activeWhen)
                    condKnobs.push_back ({ s, cells.back().caption, k->activeWhen });

                // Highlight predicate (Story 15.4): the ring is drawn over the children, so the cell
                // keeps its widget and only gains a mark.
                // The cell index, not the toggle pointer: the step's on/off switch is created a few
                // lines further down, so it does not exist yet — and the playhead dot is anchored
                // to it at paint time.
                if (k->highlightWhen)
                    markedKnobs.push_back ({ s, cells.back().caption, k->highlightWhen, true, cells.size() - 1 });
                if (k->playingWhen)
                    markedKnobs.push_back ({ s, cells.back().caption, k->playingWhen, false, cells.size() - 1 });

                // Per-knob ON/OFF switch (STEP SEQ steps): a checkbox in this cell's top-right,
                // dimming the knob when off via the same condKnobs path as activeWhen. The
                // predicate is built HERE from the parameter, so no editor injection is needed.
                // dimOnly: a rest keeps its pitch (the SPACE rule), so the knob stays draggable
                // while off — dial in (and audition) a rest's note without re-enabling it first.
                if (k->toggleParamId.isNotEmpty())
                {
                    juce::Button* tb;
                    if (k->accentParamId.isNotEmpty())
                    {
                        // Three-state switch (15.2): off → on → accented, one gesture per click.
                        tb = static_cast<juce::Button*> (ownedWidgets.add (
                                 new StepSwitch (apvts, k->toggleParamId, k->accentParamId)));
                    }
                    else
                    {
                        auto* plain = static_cast<juce::ToggleButton*> (ownedWidgets.add (new juce::ToggleButton()));
                        buttonAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                            apvts, k->toggleParamId, *plain));
                        tb = plain;
                    }
                    tb->setWantsKeyboardFocus (false);   // never steal focus from the on-screen keyboard
                    addAndMakeVisible (*tb);
                    cells.back().toggle = tb;
                    if (auto* v = apvts.getRawParameterValue (k->toggleParamId))
                        condKnobs.push_back ({ s, cells.back().caption,
                                               [v] { return v->load() > 0.5f; }, true });

                    // Bringing a rest back sounds the step once (15.3), so you hear what you just
                    // restored. The switch has no value of its own — it shares the knob's cell and
                    // previews the knob's pitch. Nothing here releases the note: there is no
                    // gesture to end, so the editor's safety cutoff closes it.
                    // Guarded like the knob below: the ButtonAttachment replays a loaded preset
                    // through setToggleState(sendNotificationSync), so onClick also fires for every
                    // step the preset switches on — and previewing arms the write cursor (15.4).
                    // Only a click with the mouse actually on the switch is a gesture. (The
                    // three-state StepSwitch only ever changes state from a real click, but the
                    // mouse guard stays — one rule for both kinds.) The ON test reads the PARAM,
                    // not getToggleState — the StepSwitch never sets its Button toggle state.
                    if (k->audition)
                    {
                        auto* onRaw = apvts.getRawParameterValue (k->toggleParamId);
                        tb->onClick = [tb, s, onRaw, aud = k->audition]
                        {
                            if (onRaw != nullptr && onRaw->load() > 0.5f
                                && (tb->isMouseButtonDown (true) || tb->isMouseOver (true)))
                                aud ((int) s->getValue(), true);
                        };
                    }
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

                // Row toggle (15.7): a SECOND slider on this same cell, editing the alternate
                // param; the header latch flips which of the two is visible. Built like the main
                // knob — attachment, no double-click default, preset-baseline double-click, dims
                // with the step — but its read-out comes from altTextFromValue ("36%" / "TIE" /
                // "SLIDE") and its audition replays the STEP'S PITCH (the main slider holds the
                // semitones), so a gate edit is heard on the note it phrases.
                if (k->altParamId.isNotEmpty() && apvts.getParameter (k->altParamId) != nullptr)
                {
                    auto* g = static_cast<SynthySlider*> (ownedWidgets.add (new SynthySlider()));
                    g->setKnobDiameter (knobD);
                    g->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 14);
                    sliderAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                        apvts, k->altParamId, *g));
                    g->setDoubleClickReturnValue (false, 0.0);
                    g->presetBaseline = [&a = apvts, id = k->altParamId]() -> double
                    {
                        if (PresetIO::presetBaseline01)
                            if (const float v01 = PresetIO::presetBaseline01 (id); v01 >= 0.0f)
                                if (auto* p = a.getParameter (id))
                                    return (double) p->convertFrom0to1 (v01);
                        return std::numeric_limits<double>::quiet_NaN();
                    };
                    if (k->altTextFromValue)
                    {
                        g->textFromValueFunction = k->altTextFromValue;
                        g->valueFromTextFunction = k->altValueFromText
                            ? k->altValueFromText
                            : [] (const juce::String& t) { return t.getDoubleValue(); };
                        g->updateText();
                        g->tooltipFromValue = k->altTextFromValue;
                        g->refreshTooltip();
                    }
                    if (k->audition)
                    {
                        auto movedG = std::make_shared<bool> (false);
                        g->onValueChange = [g, s, movedG, aud = k->audition,
                                            prev = std::move (g->onValueChange)]
                        {
                            if (prev) prev();
                            if (g->isMouseButtonDown (true) || g->isMouseOver (true))
                            {
                                *movedG = true;
                                aud ((int) s->getValue(), true);
                            }
                        };
                        g->onDragStart = [s, movedG, aud = k->audition]
                        {
                            *movedG = false;
                            aud ((int) s->getValue(), true);
                        };
                        g->onDragEnd = [movedG, aud = k->audition] { if (*movedG) aud (0, false); };
                    }
                    if (k->toggleParamId.isNotEmpty())
                        if (auto* v = apvts.getRawParameterValue (k->toggleParamId))
                            condKnobs.push_back ({ g, nullptr, [v] { return v->load() > 0.5f; }, true });
                    addChildComponent (*g);   // hidden until the row toggle flips (applyAltRow)
                    if (auto* tb = cells.back().toggle)
                        tb->toFront (false);   // the corner switch overlaps the cell — keep it clickable above g
                    altKnobs.push_back ({ s, g });
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
                if (c->itemValues)
                    comboValues.push_back ({ c->paramId, c->itemValues });
                if (c->indexIsValue)
                {
                    // Item INDEX == param value. Bypass ComboBoxParameterAttachment (its value is
                    // index/(numItems-1), which mismaps a variable item count against a fixed range —
                    // MOD MATRIX PARAM). Sync combo→param by index; param→combo via refreshCombo.
                    if (auto* raw = apvts.getRawParameterValue (c->paramId))
                        box->setSelectedItemIndex (comboPositionFor (c->paramId, (int) raw->load()),
                                                   juce::dontSendNotification);
                    juce::ComboBox* boxPtr = box;
                    const juce::String pid = c->paramId;
                    const auto userHook = c->onUserSelect;   // user-gesture-only (see descriptor)
                    box->onChange = [this, boxPtr, pid, userHook]
                    {
                        const int pos = juce::jmax (0, boxPtr->getSelectedItemIndex());
                        // With a value list the position is only where the item sits; the VALUE is
                        // what the parameter has always meant.
                        const auto vals = valuesFor (pid);
                        const int value = juce::isPositiveAndBelow (pos, vals.size()) ? vals[pos] : pos;
                        if (auto* pp = apvts.getParameter (pid))
                            pp->setValueNotifyingHost (pp->convertTo0to1 ((float) value));
                        if (userHook)
                            userHook (value);
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

                // Mode-dependent combo (MOD MATRIX QUANT outside FREQ routings): same polling
                // path as the mode-dependent knobs — disabled + dimmed, layout untouched.
                if (c->activeWhen)
                    condKnobs.push_back ({ box, cells.back().caption, c->activeWhen });
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
                    cells.push_back ({ disp->component, nullptr, juce::jmax (1, disp->slots),
                                       nullptr, /*display*/ true });
                }
            }
        }

        // Apply the fold's starting state (16.2) once the cells exist.
        if (desc.collapseTitle.isNotEmpty())
            applyCollapsed();
    }

    // Display fold (16.2): hide/show every Display cell and re-flow the body. resized() is
    // called explicitly because the rack may hand this frame the SAME bounds (today the
    // neighbouring modules keep the row tall), and JUCE skips resized() when nothing moved.
    void ModuleFrame::applyCollapsed()
    {
        for (auto& cell : cells)
            if (cell.display && cell.widget != nullptr)
                cell.widget->setVisible (! collapsedState);
        resized();
        repaint();
    }

    // --- Step pages (16.3) --------------------------------------------------------------------

    bool ModuleFrame::idIsPaged (const juce::String& id) const
    {
        if (desc.paging.pageCount <= 1 || desc.paging.stepsPerPage <= 0)
            return false;
        for (const auto& pre : desc.paging.pagedPrefixes)
            if (id.length() > pre.length() && id.startsWith (pre)
                && id.substring (pre.length()).containsOnly ("0123456789"))
                return true;
        return false;
    }

    juce::String ModuleFrame::pagedId (const juce::String& id) const
    {
        if (builtPage <= 0)
            return id;
        int i = id.length();
        while (i > 0 && juce::CharacterFunctions::isDigit (id[i - 1])) --i;
        const int n = id.substring (i).getIntValue();
        return id.substring (0, i) + juce::String (n + builtPage * desc.paging.stepsPerPage);
    }

    void ModuleFrame::rebuildPagedBody()
    {
        // Tear down ONLY what buildBody created — header widgets and their attachments stay.
        // Attachments go FIRST (their destructors talk to the widgets they watch), widgets after.
        sliderAtt.clear();
        comboAtt.clear();
        buttonAtt.erase (buttonAtt.begin() + (long) headerButtonAtts, buttonAtt.end());
        cells.clear();               // Display components are editor-owned: buildBody re-adds them
        markedKnobs.clear();
        ringKnobs.clear();
        xformKnobs.clear();
        condKnobs.clear();
        altKnobs.clear();
        dynCombos.clear();
        indexValueCombos.clear();
        comboValues.clear();
        actionButtons.clear();
        markerCells.clear();
        ownedWidgets.clear();        // deletes the body widgets (Components detach themselves)
        ownedCaptions.clear();

        buildBody();

        // Mirror the constructor's after-build passes for the fresh widgets.
        for (auto* w : ownedWidgets)
            if (w != nullptr) { w->setWantsKeyboardFocus (false); w->setMouseClickGrabsKeyboardFocus (false); }
        applyAltRow();       // a page flip keeps the GATE view if it was up
        updateCondKnobs();
        resized();
        repaint();
    }

    void ModuleFrame::updatePageButtons()
    {
        const int  shown   = shownPage();
        const int  playing = desc.paging.playingPage ? desc.paging.playingPage() : -1;
        const bool follow  = desc.paging.getFollow ? desc.paging.getFollow() : false;

        // Read-out: "2/4", with the playing page marked by a dot — "•2/4" when the pattern plays
        // the shown page, "2/4 •1" when it plays elsewhere. Text, not colour, so it stays
        // readable for red-green colour vision.
        if (pageLabel != nullptr)
        {
            const auto dot = juce::String::fromUTF8 ("\xe2\x80\xa2");
            auto text = juce::String (shown + 1) + "/" + juce::String (desc.paging.pageCount);
            if (playing == shown)                 text = dot + text;
            else if (playing >= 0)                text = text + " " + dot + juce::String (playing + 1);
            if (text != lastPageText)
            {
                lastPageText = text;
                pageLabel->setText (text, juce::dontSendNotification);
            }
        }
        if (pagePrevBtn != nullptr) pagePrevBtn->setEnabled (shown > 0);
        if (pageNextBtn != nullptr) pageNextBtn->setEnabled (shown < desc.paging.pageCount - 1);
        const char fs = follow ? (char) 1 : (char) 0;
        if (fs != lastFollowState)
        {
            lastFollowState = fs;
            if (followBtn != nullptr)
                followBtn->setToggleState (follow, juce::dontSendNotification);
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

        // Step pages (16.3): the body lists one page — reset the SAME params on every other page
        // too (same scope as above: the knob's main param; corner/alt params keep today's reset
        // semantics on every page alike).
        if (desc.paging.pageCount > 1 && desc.paging.stepsPerPage > 0)
            for (const auto& el : desc.body)
                if (auto* k = std::get_if<Knob> (&el))
                    if (idIsPaged (k->paramId))
                    {
                        int i = k->paramId.length();
                        while (i > 0 && juce::CharacterFunctions::isDigit (k->paramId[i - 1])) --i;
                        const auto base = k->paramId.substring (0, i);
                        const int  n    = k->paramId.substring (i).getIntValue();
                        for (int p = 1; p < desc.paging.pageCount; ++p)
                            resetId (base + juce::String (n + p * desc.paging.stepsPerPage));
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
        // Row toggle (15.7): only modules that declare one give up header width for it, so
        // every other module's header geometry is untouched.
        if (altRowBtn != nullptr)
            altRowBtn->setBounds (header.removeFromRight (46).reduced (2, 1));
        // Header actions (15.8), left of the row toggle. Reverse order, so the declaration
        // order reads left-to-right on screen (removeFromRight fills right-to-left).
        for (int i = actionBtns.size(); --i >= 0;)
            actionBtns[i]->setBounds (header.removeFromRight (72).reduced (2, 1));
        // Step pages (16.3), left of the header actions: … < 2/4 > FOLLOW [LOAD MIDI] …
        if (followBtn != nullptr)
            followBtn->setBounds (header.removeFromRight (62).reduced (2, 1));
        if (pageNextBtn != nullptr)
            pageNextBtn->setBounds (header.removeFromRight (24).reduced (2, 1));
        if (pageLabel != nullptr)   // 72: "16/16 •12" (16 pages since 2026-09-02) needs the room
            pageLabel->setBounds (header.removeFromRight (72).reduced (0, 1));
        if (pagePrevBtn != nullptr)
            pagePrevBtn->setBounds (header.removeFromRight (24).reduced (2, 1));
        // Display fold latch (16.2), same slot family as the row toggle.
        if (collapseBtn != nullptr)
            collapseBtn->setBounds (header.removeFromRight (52).reduced (2, 1));
        header.removeFromLeft (11);   // room for the status-LED dot painted at the header's left edge
        titleLabel.setBounds (header);

        // --- body slot grid (the single body-layout site) ---
        auto body = r.reduced (5, 4);
        // The FOLDED size when the display is away (16.2) — the fold changes the flow: fewer
        // content rows, and the Display cells drop out of the slot accounting below entirely,
        // so the remaining knobs spread exactly as if the module had been born this size.
        const auto spec = sizeClassSpec (effectiveSizeClass());

        // The body fills the module width from its CONTENT, decoupled from the grid column
        // span AND from the knob size (PROTOTYPE): lay the content slots (combo=2, display=N,
        // else 1) across `units` rows. nCols is derived so the cells fill the full width with
        // no trailing empty cells — the cause of the old "module doesn't use its space" look.
        const int units = juce::jmax (1, spec.units);
        auto folded = [this] (const Cell& cell) { return collapsedState && cell.display; };   // 16.2
        int rawTotal = 0;
        for (auto& cell : cells)
            if (! folded (cell))
                rawTotal += juce::jmax (1, cell.slots);
        rawTotal = juce::jmax (1, rawTotal);
        const int nCols = juce::jmax (1, (rawTotal + units - 1) / units);   // ceil(rawTotal / units)

        // Row count from the cells we actually place (sum of their clamped spans), NOT
        // from bodySlots(desc.body): a skipped null-Display would inflate the count and
        // leave a phantom gap. (deferred 1.2 review item)
        int total = 0;
        for (auto& cell : cells)
            if (! folded (cell))
                total += juce::jlimit (1, nCols, cell.slots);
        total = juce::jmax (1, total);
        const int nRows = (total + nCols - 1) / nCols;
        const int cellW = body.getWidth()  / nCols;
        const int cellH = body.getHeight() / juce::jmax (1, nRows);

        // Cells are a whole number of pixels wide, so nCols of them rarely fill the body exactly —
        // in MOD MATRIX 32 × 54 px leave 16 px, which piled up at the RIGHT edge as a dead strip
        // ("Trauerrand", maintainer 2026-08-11). A module that repeats a GROUP of controls
        // (desc.slotActivity, a routing slot) can spend that remainder instead of hoarding it: the
        // leftover is split into the gaps BETWEEN the groups, which both empties the right edge and
        // separates the slots. paint() fills these gaps in the dim colour so the grouping reads as
        // grouping rather than as a layout accident. Modules without groups are untouched.
        groupGaps.clear();
        int groupSpan = 0;
        if (const int gs = desc.slotActivity.groupSize; gs > 0)
            for (int i = 0; i < gs && i < (int) cells.size(); ++i)
                groupSpan += juce::jlimit (1, nCols, cells[(size_t) i].slots);
        const int groupsPerRow = (groupSpan > 0) ? nCols / groupSpan : 0;
        const int leftover     = body.getWidth() - nCols * cellW;
        const int groupGap     = (groupsPerRow > 1 && leftover > 0) ? leftover / (groupsPerRow - 1) : 0;
        for (int gi = 1; gi < groupsPerRow && groupGap > 0; ++gi)
            groupGaps.push_back ({ body.getX() + gi * groupSpan * cellW + (gi - 1) * groupGap,
                                   body.getY(), groupGap, body.getHeight() });

        int col = 0, row = 0;
        for (auto& cell : cells)
        {
            if (folded (cell))
                continue;   // 16.2: a folded Display claims no slot — the knobs close the gap
            const int span = juce::jlimit (1, nCols, cell.slots);
            if (col + span > nCols) { col = 0; ++row; }

            // Never lay a (spanning) cell out below the body's bottom edge: bound the
            // row to the grid we sized for. (deferred 1.2 review item)
            const int placeRow = juce::jmin (row, nRows - 1);
            const int gapShift = (groupGap > 0 && groupSpan > 0) ? (col / groupSpan) * groupGap : 0;
            juce::Rectangle<int> cellR (body.getX() + col * cellW + gapShift,
                                        body.getY() + placeRow * cellH,
                                        cellW * span, cellH);

            if (cell.caption != nullptr)   // knob/combo/toggle: NAME caption on top, widget below
            {
                auto cr = cellR.reduced (2);
                const int capH = 13;   // fits the uniform 13pt caption font
                auto* knob = dynamic_cast<SynthySlider*> (cell.widget);
                const bool isKnob   = knob != nullptr;
                const bool isButton = dynamic_cast<juce::Button*> (cell.widget) != nullptr;

                // ONE knob size for the whole rack (KnobSize::Standard), exactly as kComboW is one
                // width for every combo — the cell no longer decides how big a knob is, it only
                // decides whether the standard FITS. It is capped by
                //   · height: what is left after caption + value box,
                //   · width : the cell minus the 4 px per side drawRotarySlider reduces by,
                // and never falls below Minimum. A module whose cell cannot host the standard is
                // telling us its size class is wrong; that is a layout fix, not a knob fix. This is
                // why the module HEIGHTS below are derived from the standard (a knob row is
                // 13 caption + 40 + 8 + 14 value + 4 = 79 px) instead of the reverse.
                const int kValueH = 14, kKnobPad = 8;
                const int knobD = isKnob
                    ? juce::jlimit (KnobSize::Minimum, KnobSize::Standard,
                                    juce::jmin (cr.getHeight() - capH - kKnobPad - kValueH,
                                                cr.getWidth() - kKnobPad))
                    : KnobSize::Standard;
                // A knob's widget takes the whole cell below its caption: the value box then always
                // sits on the cell's bottom edge and the rotary — which the LookAndFeel caps at
                // knobD — is centred in what is left. Sizing the widget to the diameter instead
                // would let a WIDTH-limited knob (a narrow cell in SAMPLER or STEP SEQ) float in the
                // middle of its cell with air under the value box. A combo is just the short box.
                const int wH = isKnob ? juce::jmax (knobD + kKnobPad + kValueH, cr.getHeight() - capH)
                                      : kComboH;

                if (isKnob)
                {
                    // Knob block (NAME + rotary + value box) centred vertically in the cell,
                    // CENTRED horizontally; the slider draws the rotary in its top square and the
                    // value box in the bottom 14 px. The LookAndFeel caps the rotary at the
                    // slider's own knob diameter, so it is set here, per cell, not once per module.
                    knob->setKnobDiameter (knobD);
                    const int blockH = capH + wH;
                    const int top = juce::jmax (cr.getY(), cr.getCentreY() - blockH / 2);
                    cell.caption->setBounds (cr.getX(), top, cr.getWidth(), capH);
                    const int sw = juce::jmin (cr.getWidth(), knobD + kKnobPad);
                    cell.widget->setBounds (cr.getCentreX() - sw / 2, top + capH, sw, wH);
                    // The optional switch rides in the cell's top-right corner, on the caption's
                    // line: it belongs to this knob, so it must not claim a cell of its own.
                    // The bounds reach 8 px further left and 7 px further down than the drawn
                    // box (StepSwitch anchors its glyph top-right, so nothing moves visually):
                    // pure hit area, into the cell corner's empty air — the box itself was too
                    // small to aim at (maintainer 2026-08-26).
                    if (cell.toggle != nullptr)
                        cell.toggle->setBounds (cr.getRight() - 24, top - 1, 24, capH + 9);
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

        // Row toggle (15.7): the alternate slider rides EXACTLY on its main knob's bounds —
        // the row flip is a visibility swap, never a re-layout.
        for (auto& ak : altKnobs)
        {
            ak.alt->setKnobDiameter (ak.main->getKnobDiameter());
            ak.alt->setBounds (ak.main->getBounds());
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

        // Separators between repeated control groups (see resized/groupGaps): the pixels the cell
        // grid could not use, spent between MOD MATRIX's routing slots instead of left over at the
        // right edge. Same dim tone the inactive slots use, so it reads as "nothing here" rather
        // than as a fifth column.
        g.setColour (juce::Colour (0xff15181d).withAlpha (0.45f));
        for (const auto& gap : groupGaps)
            g.fillRect (gap);
    }

    void ModuleFrame::paintOverChildren (juce::Graphics& g)
    {
        // Pattern-length marker (16.2, maintainer: "eine rote Trennlinie hinter das letzte
        // aktive Element"): a red line on the right edge of the LEN-th step knob, so where the
        // figure wraps is visible at a glance — the knob-row counterpart of PERC's dimmed
        // beyond-LEN cells. Red is a POSITION here, not a colour code, so it stays readable
        // for red-green colour vision too.
        if (! markerCells.empty() && markerLen != nullptr)
        {
            // Page-aware since 16.3: the line lands on the page that CONTAINS the boundary; on
            // any other page it is simply absent (the figure runs on past both edges).
            int len = (int) markerLen->load();
            if (desc.paging.pageCount > 1 && desc.paging.stepsPerPage > 0)
                len -= builtPage * desc.paging.stepsPerPage;
            else
                len = juce::jlimit (1, (int) markerCells.size(), len);
            if (len >= 1 && len <= (int) markerCells.size())
            {
                const auto& cell = cells[(size_t) markerCells[(size_t) len - 1]];
                if (cell.widget != nullptr)
                {
                    auto b = cell.widget->getBounds();
                    if (cell.caption != nullptr) b = b.getUnion (cell.caption->getBounds());
                    g.setColour (juce::Colour (0xffd64541));
                    g.fillRect (b.getRight() - 1, b.getY(), 3, b.getHeight());
                }
            }
        }

        // Write cursor (Story 15.4): a ring around the ONE step the keyboard will write next. Drawn
        // in the module's identity colour, brightened — the rack's other marks are a green dot
        // (active) and a grey ring (off), so this cannot be mistaken for either. Painted before the
        // dim overlay so a disabled module's cursor fades with everything else.
        for (const auto& m : markedKnobs)
        {
            if (m.on != 1 || m.widget == nullptr) continue;
            auto b = m.widget->getBounds();
            if (m.caption != nullptr) b = b.getUnion (m.caption->getBounds());
            if (m.ring)
            {
                g.setColour (typeColour (desc.type).brighter (0.7f));
                g.drawRoundedRectangle (b.expanded (2).toFloat(), 4.0f, 2.0f);
            }
            else
            {
                // Playhead: the same lit dot the MOD MATRIX marks a live slot with. It rides the
                // caption's line, BETWEEN the step number and its on/off box (maintainer 2026-08-11:
                // at the cell's left edge it sat in front of the knob and read as belonging to the
                // neighbour). Without a switch to anchor to it hugs the caption's right edge.
                const float d  = 6.0f;
                const auto& cell = cells[juce::jmin (m.cellIndex, cells.size() - 1)];
                const auto  line = m.caption != nullptr ? m.caption->getBounds() : b;
                // Anchor to the DRAWN box, not the bounds — the switch's hit area is wider than
                // its glyph, and the dot must hug what the eye sees.
                const float right = cell.toggle != nullptr
                                        ? (float) cell.toggle->getRight() - (float) StepSwitch::kGlyphW - 2.0f
                                        : (float) line.getRight();
                g.setColour (juce::Colour (0xff7bd88f));
                g.fillEllipse (right - d, (float) line.getCentreY() - d * 0.5f, d, d);
            }
        }

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
        // Step pages (16.3): the shown page is editor-owned view state — poll it, and when it
        // moved (a page button, FOLLOW's auto-flip, the write cursor crossing a boundary),
        // rebuild the body against the new page's params. A paging module without paged cells
        // (PERC — its grid is a custom Display that windows itself) only repaints.
        if (desc.paging.pageCount > 1)
        {
            if (shownPage() != builtPage)
            {
                if (hasPagedCells) rebuildPagedBody();   // sets builtPage via buildBody
                else               { builtPage = shownPage(); repaint(); }
            }
            updatePageButtons();
        }

        // Pattern-length marker (16.2): poll LEN and repaint only on a real change.
        if (markerLen != nullptr)
        {
            const int now = (int) markerLen->load();
            if (now != lastMarkerLen) { lastMarkerLen = now; repaint(); }
        }

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
                const int want = comboPositionFor (iv.first, (int) raw->load());
                if (want >= 0 && want != iv.second->getSelectedItemIndex() && want < iv.second->getNumItems())
                    iv.second->setSelectedItemIndex (want, juce::dontSendNotification);
            }
        }

        // Highlight predicates (STEP SEQ's write cursor): repaint only when a ring appears or goes.
        for (auto& m : markedKnobs)
        {
            const char now = (m.predicate && m.predicate()) ? (char) 1 : (char) 0;
            if (m.on != now) { m.on = now; repaint(); }
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

    void ModuleFrame::applyAltRow()
    {
        // The row flip is a pure visibility swap on identical bounds (see resized): the cell —
        // and with it the corner switch, the write ring and the playhead — stays where it is.
        for (auto& ak : altKnobs)
        {
            ak.main->setVisible (! altRowActive);
            ak.alt ->setVisible (  altRowActive);
        }
    }

    void ModuleFrame::refreshNamedReadouts()
    {
        // Only sliders that carry a textFromValue read-out; everyone else keeps JUCE's own text.
        for (auto& cell : cells)
            if (auto* s = dynamic_cast<SynthySlider*> (cell.widget))
                if (s->textFromValueFunction)
                {
                    s->updateText();
                    s->refreshTooltip();   // full text on hover, same reference as the box
                }
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
            if (ck.widget != nullptr)
            {
                if (! ck.dimOnly)          // a dim-only control (a rest's pitch knob) keeps the mouse
                    ck.widget->setEnabled (on);
                ck.widget->setAlpha (on ? 1.0f : 0.35f);
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

    juce::Array<int> ModuleFrame::valuesFor (const juce::String& paramId) const
    {
        for (const auto& cv : comboValues)
            if (cv.first == paramId && cv.second)
                return cv.second();
        return {};
    }

    int ModuleFrame::comboPositionFor (const juce::String& paramId, int value) const
    {
        const auto vals = valuesFor (paramId);
        return vals.isEmpty() ? value : vals.indexOf (value);
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
            // Item ids are position+1, and with a value list the position has to be looked up.
            if (auto* raw = apvts.getRawParameterValue (paramId))
                dc.box->setSelectedId (comboPositionFor (paramId, (int) raw->load()) + 1,
                                       juce::dontSendNotification);
        }
    }

    void ModuleFrame::clickFirstAction()
    {
        if (! actionButtons.empty() && actionButtons.front() != nullptr)
            actionButtons.front()->triggerClick();   // press animation + fires onClick
    }
}
