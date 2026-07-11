#pragma once
#include <JuceHeader.h>

namespace rack
{
    // A tiny header icon button that VECTOR-DRAWS its glyph (Story 6.1). Font glyphs like
    // "ⓘ"/"↺" render as tofu/"?" in the shipped font and shrink to the button height; drawing
    // the icon ourselves guarantees it renders, stays crisp, and fills the button regardless of
    // the (small) header height. Used for the module header's info + reset icons.
    class IconButton : public juce::Button
    {
    public:
        enum class Kind { Info, Reset };

        explicit IconButton (Kind k) : juce::Button ({}), kind (k) {}

        void setTint (juce::Colour c) { tint = c; }

        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            const float d  = juce::jmin (r.getWidth(), r.getHeight());
            const float cx = r.getCentreX(), cy = r.getCentreY();
            const float rad = d * 0.48f;   // fill the click box almost completely

            juce::Colour c = tint;
            if (down)      c = c.brighter (0.35f);
            else if (over) c = c.brighter (0.18f);

            // subtle circular background so the icon reads against the header
            g.setColour (c.withAlpha (0.22f));
            g.fillEllipse (cx - rad - 1.0f, cy - rad - 1.0f, (rad + 1.0f) * 2.0f, (rad + 1.0f) * 2.0f);

            g.setColour (c);
            const float stroke = juce::jmax (1.3f, d * 0.11f);

            if (kind == Kind::Info)
            {
                g.drawEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f, stroke);
                // "i": dot + stem, drawn as shapes (no font dependency)
                const float dot = juce::jmax (1.6f, d * 0.13f);
                g.fillEllipse (cx - dot * 0.5f, cy - rad * 0.62f, dot, dot);
                const float stemW = juce::jmax (1.6f, d * 0.13f);
                const float stemTop = cy - rad * 0.18f;
                const float stemBot = cy + rad * 0.55f;
                g.fillRoundedRectangle (cx - stemW * 0.5f, stemTop, stemW, stemBot - stemTop, stemW * 0.4f);
            }
            else // Reset: a circular arrow (refresh)
            {
                juce::Path arc;
                // ~300° arc, gap at the top-right; angles clockwise from 12 o'clock.
                arc.addCentredArc (cx, cy, rad, rad, 0.0f,
                                   juce::MathConstants<float>::pi * 0.35f,
                                   juce::MathConstants<float>::pi * 1.95f, true);
                g.strokePath (arc, juce::PathStrokeType (stroke, juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));
                // arrowhead at the arc's start (top-right), pointing along the sweep
                const float a = juce::MathConstants<float>::pi * 0.35f;
                const float ex = cx + rad * std::sin (a);
                const float ey = cy - rad * std::cos (a);
                const float h  = juce::jmax (2.5f, d * 0.22f);
                juce::Path head;
                head.addTriangle (ex, ey - h * 0.6f, ex, ey + h * 0.6f, ex + h, ey);
                head.applyTransform (juce::AffineTransform::rotation (a + juce::MathConstants<float>::pi * 0.5f, ex, ey));
                g.fillPath (head);
            }
        }

    private:
        Kind kind;
        juce::Colour tint { juce::Colours::white };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IconButton)
    };
}
