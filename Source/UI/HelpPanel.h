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

        // Long help texts (MOD MATRIX, KEYBOARD) scroll rather than run past the window edge.
        view.setViewedComponent (&bodyComp, /*deleteWhenRemoved*/ false);
        view.setScrollBarsShown (/*vertical*/ true, /*horizontal*/ false);
        view.setScrollBarThickness (kScrollW);
        view.setWantsKeyboardFocus (false);       // same reason as the panel itself
        addAndMakeVisible (view);

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
    // `width` lets the editor widen the panel for a long text (fewer wrapped lines = shorter
    // panel), `maxHeight` caps it at what the window can show — a text longer than that SCROLLS
    // inside the panel instead of being cut off at the bottom. Returns true when everything fits
    // without scrolling, so the caller can try a wider panel first.
    bool setContent (const juce::String& title, const juce::String& body,
                     int width = kWidth, int maxHeight = 1 << 24)
    {
        titleText = title;

        const int innerW = juce::jmax (80, width - 2 * kPad - kScrollW);
        bodyComp.attr = md::render (body);

        juce::TextLayout tl;
        tl.createLayout (bodyComp.attr, (float) innerW);
        const int bodyH = (int) std::ceil (tl.getHeight()) + 4;   // +4 safety against rounding

        const int wantH = kTitleH + kPad + bodyH + kPad;
        const int panelH = juce::jmax (kTitleH + 40, juce::jmin (wantH, maxHeight));
        setSize (width, panelH);

        // The scrolled component keeps its FULL text height; the viewport shows a window on it.
        bodyComp.setSize (innerW, bodyH);
        view.setViewPosition (0, 0);   // a fresh text always starts at the top
        repaint();
        return wantH <= maxHeight;
    }

    void resized() override
    {
        closeBtn.setBounds (getWidth() - kTitleH + 3, 3, kTitleH - 6, kTitleH - 6);
        view.setBounds (getLocalBounds().withTrimmedTop (kTitleH).reduced (kPad, kPad));
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

        // The body is drawn by the scrolled child (bodyComp) inside `view`.
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
    static constexpr int kScrollW = 9;    // scrollbar lane, always reserved so the text wrap
                                          // does not change when the bar appears

private:
    // The scrolled text itself: it is as tall as the rendered Markdown needs, and the viewport
    // shows as much of it as the panel has room for.
    struct Body : juce::Component
    {
        juce::AttributedString attr;   // rendered Markdown; measured in setContent, drawn here
        Body() { setInterceptsMouseClicks (false, false); }   // wheel scrolls, clicks pass through
        void paint (juce::Graphics& g) override { attr.draw (g, getLocalBounds().toFloat()); }
    };

    juce::String titleText;
    juce::Viewport view;
    Body bodyComp;
    juce::TextButton closeBtn;
    juce::ComponentDragger dragger;
    juce::ComponentBoundsConstrainer constrainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HelpPanel)
};
