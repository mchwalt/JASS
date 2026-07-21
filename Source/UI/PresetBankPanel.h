#pragma once
#include <JuceHeader.h>
#include <array>
#include <functional>

// The 12-button preset quick-access grid (F1..F12) that fills the PRESETS module body
// (MASTER BUS). Self-contained custom Component (like WaveformDisplay): the editor owns it,
// injects it as a Display, and wires the two callbacks. It only holds the *display* copy of the
// slot assignments — the editor persists them (PresetIO::savePresetBanks).
//
// Interaction (see also SynthyEditor's F-key polling for the physical-key equivalents):
//   - single click on a FILLED slot  -> onLoadSlot(i)   (load that preset now)
//   - single click on an EMPTY slot   -> nothing
//   - double click on ANY slot        -> onAssignSlot(i) (open the assign dialog)
//     (a double click on a filled slot loads once via the first mouseDown, then opens the
//      dialog — harmless and consistent with the keyboard "tap loads, hold also assigns".)
//   - hover a filled slot whose name is too long -> the full name scrolls (marquee).
class PresetBankPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int kNumSlots = 12;

    PresetBankPanel() { setInterceptsMouseClicks(true, true); setWantsKeyboardFocus(false); }
    ~PresetBankPanel() override { stopTimer(); }

    // Editor wiring.
    std::function<void(int)> onLoadSlot;     // fire the assigned preset (index 0..11)
    std::function<void(int)> onAssignSlot;   // open the assign dialog for a slot

    void setAllAssignments(const std::array<juce::String, kNumSlots>& a) { assigned = a; repaint(); }
    void setAssignment(int i, const juce::String& name)
    {
        if (i >= 0 && i < kNumSlots) { assigned[(size_t) i] = name; repaint(); }
    }
    juce::String getAssignment(int i) const
    {
        return (i >= 0 && i < kNumSlots) ? assigned[(size_t) i] : juce::String();
    }
    bool isAssigned(int i) const { return getAssignment(i).isNotEmpty(); }

    void paint(juce::Graphics& g) override
    {
        for (int i = 0; i < kNumSlots; ++i)
        {
            auto b = slotBounds(i);
            const bool filled = isAssigned(i);
            const bool hot    = (i == hoverSlot);

            g.setColour(juce::Colour(filled ? 0xff2b3446u : 0xff202634u)
                            .brighter(hot ? 0.18f : 0.0f));
            g.fillRoundedRectangle(b, 5.0f);
            g.setColour(filled ? juce::Colour(0xff4aa3ff) : juce::Colour(0xff3a4152));
            g.drawRoundedRectangle(b, 5.0f, filled ? 1.4f : 1.0f);

            // "F1".. label (top-left) + an "assigned" dot (top-right).
            g.setColour(juce::Colour(0xffaab3c0));
            g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
            g.drawText("F" + juce::String(i + 1),
                       b.reduced(6.0f).removeFromTop(14.0f).toNearestInt(),
                       juce::Justification::topLeft, false);
            if (filled)
            {
                g.setColour(juce::Colour(0xff4aa3ff));
                g.fillEllipse(b.getRight() - 12.0f, b.getY() + 7.0f, 5.0f, 5.0f);
            }

            // Name area (lower part of the button).
            auto nameArea = b.reduced(6.0f);
            nameArea.removeFromTop(15.0f);
            const auto ni = nameArea.toNearestInt();

            if (! filled)
            {
                g.setColour(juce::Colour(0xff5a6474));
                g.setFont(juce::FontOptions(13.0f));
                g.drawText(juce::CharPointer_UTF8("\xe2\x80\x94"), ni, juce::Justification::centred, false); // em dash
                continue;
            }

            const auto& name = assigned[(size_t) i];
            juce::Font nameFont(juce::FontOptions(13.0f));
            g.setFont(nameFont);
            g.setColour(juce::Colour(0xffe6ecf5));
            const float textW = stringWidth(nameFont, name);

            if (hot && textW > (float) ni.getWidth())
            {
                // Marquee: scroll the full name; clip to the name area, draw twice for a seamless wrap.
                juce::Graphics::ScopedSaveState ss(g);
                g.reduceClipRegion(ni);
                const float span = textW + 28.0f;   // trailing gap before the copy repeats
                const float startX = (float) ni.getX() - std::fmod(marqueeOffset, span);
                for (int k = 0; k < 2; ++k)
                    g.drawText(name,
                               juce::Rectangle<int>((int) (startX + (float) k * span), ni.getY(),
                                                    (int) textW + 2, ni.getHeight()),
                               juce::Justification::centredLeft, false);
            }
            else
            {
                g.drawText(name, ni, juce::Justification::centredLeft, false); // static, JUCE truncates
            }
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        // Left click only. JUCE delivers mouseDown for BOTH clicks of a double-click, so gate on
        // getNumberOfClicks()==1: a double-click loads once (first click) then assigns via
        // mouseDoubleClick — matching the keyboard path (one load, not two).
        if (! e.mods.isLeftButtonDown() || e.getNumberOfClicks() != 1) return;
        const int i = slotAt(e.getPosition());
        if (i >= 0 && isAssigned(i) && onLoadSlot)
            onLoadSlot(i);
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        if (! e.mods.isLeftButtonDown()) return;
        const int i = slotAt(e.getPosition());
        if (i >= 0 && onAssignSlot)
            onAssignSlot(i);
    }

    void mouseMove(const juce::MouseEvent& e) override { setHover(slotAt(e.getPosition())); }
    void mouseEnter(const juce::MouseEvent& e) override { setHover(slotAt(e.getPosition())); }
    void mouseExit(const juce::MouseEvent&)     override { setHover(-1); }

private:
    void timerCallback() override
    {
        marqueeOffset += 0.7f;   // px per tick (~30 Hz) -> gentle scroll
        repaint();
    }

    void setHover(int i)
    {
        if (i == hoverSlot) return;
        hoverSlot = i;
        marqueeOffset = 0.0f;
        if (hoverSlot >= 0) startTimerHz(30);   // only animate while hovering
        else                stopTimer();
        repaint();
    }

    juce::Rectangle<float> slotBounds(int i) const
    {
        const int cols = 6, rows = 2;
        auto area = getLocalBounds().toFloat();
        const float gap = 4.0f;
        const float cw = (area.getWidth()  - gap * (float) (cols + 1)) / (float) cols;
        const float ch = (area.getHeight() - gap * (float) (rows + 1)) / (float) rows;
        const int c = i % cols, r = i / cols;
        return { gap + (float) c * (cw + gap), gap + (float) r * (ch + gap), cw, ch };
    }

    int slotAt(juce::Point<int> p) const
    {
        for (int i = 0; i < kNumSlots; ++i)
            if (slotBounds(i).contains(p.toFloat()))
                return i;
        return -1;
    }

    static float stringWidth(const juce::Font& f, const juce::String& s)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText(f, s, 0.0f, 0.0f);
        return ga.getBoundingBox(0, -1, true).getWidth();
    }

    std::array<juce::String, kNumSlots> assigned;   // display copy; editor owns persistence
    int   hoverSlot = -1;
    float marqueeOffset = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBankPanel)
};
