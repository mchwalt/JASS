#pragma once
#include <JuceHeader.h>
#include "MarkdownRenderer.h"

// A small, movable, read-only help panel (Story 6.1). Lives as a child of the editor so it
// floats above the rack and inherits the look; it is NOT a CallOutBox (that dismisses on
// outside-click and can't be dragged). Opened by a module's header info icon; closed ONLY by
// its "✕" button or the ESC key. A single instance is reused for every module.
class HelpPanel : public juce::Component
{
public:
    std::function<void()> onClose;   // editor hides/repositions; called by "✕" and ESC

    HelpPanel()
    {
        closeBtn.setButtonText (juce::String::fromUTF8 ("\xE2\x9C\x95"));   // ✕
        closeBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff3a3f48));
        closeBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffd0d6df));
        closeBtn.onClick = [this] { if (onClose) onClose(); };
        closeBtn.setWantsKeyboardFocus (false);   // never steal focus from the playable keyboard
        addAndMakeVisible (closeBtn);

        // The panel must NOT take keyboard focus: opening/moving help would otherwise stop the
        // computer keys from playing the on-screen keyboard. ESC is handled by the editor.
        setWantsKeyboardFocus (false);

        // Keep the panel within its parent (the editor) while dragging, always leaving the
        // title strip grabbable — otherwise it can be dragged under the keyboard/off-window
        // and become impossible to pull back up.
        constrainer.setMinimumOnscreenAmounts (kTitleH, kTitleH, kTitleH, kTitleH);
    }

    // Set the content and size the panel to fit the wrapped body. Call on open AND on a
    // language switch while open. `body` is Markdown (compact subset) — it is rendered ONCE into
    // an AttributedString that both this measure pass and paint() reuse, so the measured height
    // can never disagree with what is drawn (no clipped last line).
    void setContent (const juce::String& title, const juce::String& body)
    {
        titleText = title;
        bodyAttr  = md::render (body);

        juce::TextLayout tl;
        tl.createLayout (bodyAttr, (float) (kWidth - 2 * kPad));

        const int bodyH   = (int) std::ceil (tl.getHeight()) + 4;   // +4 safety against rounding
        const int totalH  = kTitleH + kPad + bodyH + kPad;
        setSize (kWidth, juce::jmax (totalH, kTitleH + 40));
        repaint();
    }

    void resized() override
    {
        closeBtn.setBounds (getWidth() - kTitleH + 3, 3, kTitleH - 6, kTitleH - 6);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff21252c));
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (juce::Colour (0xff3d8f88));
        g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.2f);

        // title strip (the drag handle)
        auto title = getLocalBounds().removeFromTop (kTitleH);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRect (title);
        g.setColour (juce::Colour (0xffe8edf3));
        g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
        g.drawText (titleText, title.reduced (kPad, 0).withTrimmedRight (kTitleH),
                    juce::Justification::centredLeft, true);

        // body — the same AttributedString that was measured in setContent()
        auto body = getLocalBounds().withTrimmedTop (kTitleH).reduced (kPad, kPad);
        bodyAttr.draw (g, body.toFloat());
    }

    // Drag from the title strip only (matches "drag by its title bar").
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.y < kTitleH)
            dragger.startDraggingComponent (this, e);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.getMouseDownY() < kTitleH)
            dragger.dragComponent (this, e, &constrainer);
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::escapeKey)
        {
            if (onClose) onClose();
            return true;
        }
        return false;
    }

    static constexpr int kWidth   = 340;
    static constexpr int kTitleH  = 26;
    static constexpr int kPad     = 12;

private:
    juce::String titleText;
    juce::AttributedString bodyAttr;   // rendered Markdown; measured in setContent, drawn in paint
    juce::TextButton closeBtn;
    juce::ComponentDragger dragger;
    juce::ComponentBoundsConstrainer constrainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HelpPanel)
};
