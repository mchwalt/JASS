#pragma once
#include <JuceHeader.h>
#include "../DSP/WaveformCapture.h"

// Final-output oscilloscope (Story 10.6): shows exactly what leaves the synth. STEREO content is
// drawn as TWO SEPARATE PLOTS side by side (L left/blue, R right/orange — a red-green-colorblind-
// safe pair; user layout decision 2026-07-29); effectively-mono content collapses to one
// full-width plot. Both plots share the y scale (labels drawn once, far left) and the time base.
class WaveformDisplay : public juce::Component, private juce::Timer
{
public:
    WaveformDisplay(WaveformCapture& capture, juce::Colour color = juce::Colour(0xff40c0ff))
        : captureRef(capture), strokeColour(color)
    {
        zoomBox.addItemList({"1 ms", "2 ms", "5 ms", "10 ms", "25 ms", "50 ms", "100 ms"}, 1);
        zoomBox.setSelectedId(4); // 10ms default
        zoomBox.onChange = [this]() { updateTimeRange(); };
        addAndMakeVisible(zoomBox);

        startTimerHz(30);
    }

    ~WaveformDisplay() override { stopTimer(); }

    void setTimeRangeMs(double ms) { timeRangeMs = ms; }

    // Reset the time-base selector to its factory default (10 ms). Setting the id sends a
    // notification, so onChange -> updateTimeRange() applies it. Used by the module's ↺.
    void resetTimeRange() { zoomBox.setSelectedId(4); }

    void resized() override
    {
        auto area = getLocalBounds();
        zoomBox.setBounds(area.getRight() - 80, 2, 72, 18);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        float w = bounds.getWidth();
        float h = bounds.getHeight();

        constexpr float leftMargin = 38.0f;
        constexpr float bottomMargin = 16.0f;
        constexpr float topMargin = 22.0f; // room for zoom selector
        constexpr float rightMargin = 6.0f;
        constexpr float plotGap = 14.0f;   // gap between the L and R plots in stereo

        float availW = w - leftMargin - rightMargin;
        float plotH  = h - topMargin - bottomMargin;

        // Background
        g.setColour(juce::Colour(0xff151528));
        g.fillRoundedRectangle(bounds, 8.0f);

        const bool on = isOn();
        const bool stereo = on && captureRef.isStereoContent();

        // Y-axis labels — drawn once, far left (both plots share the scale)
        float yLevels[] = { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f };
        auto labelFont = juce::FontOptions(9.0f);
        float midY = topMargin + plotH / 2.0f;
        for (auto level : yLevels)
        {
            float y = midY - level * (plotH / 2.0f) * 0.9f;
            g.setColour(juce::Colour(0xff888899));
            g.setFont(labelFont);
            juce::String label = (level >= 0 ? "+" : "") + juce::String(level, 1);
            g.drawText(label, 0, static_cast<int>(y - 6), static_cast<int>(leftMargin - 4), 12,
                       juce::Justification::centredRight);
        }

        // Layout: side-by-side is the preferred stereo view; if the module is ever too narrow for
        // two readable plots, fall back to BOTH traces overlaid in one plot (emergency only —
        // user decision 2026-07-29). With the current W12H2 module this never triggers.
        const float halfW = (availW - plotGap) / 2.0f;
        const bool sideBySide = stereo && halfW >= 160.0f;

        // One sub-plot: grid, time ticks, border, channel trace(s), channel tag.
        // ch2 >= 0 => second channel overlaid in the fallback colour.
        auto drawPlot = [&] (juce::Rectangle<float> r, int ch, juce::Colour col, const char* tag, int ch2 = -1)
        {
            const float pMidY = r.getCentreY();

            // Horizontal grid
            for (auto level : yLevels)
            {
                float y = pMidY - level * (r.getHeight() / 2.0f) * 0.9f;
                g.setColour(level == 0.0f ? juce::Colour(0x40ffffff) : juce::Colour(0x28ffffff));
                g.drawHorizontalLine(static_cast<int>(y), r.getX(), r.getRight());
            }

            // X-axis: integer ms ticks (coarser side-by-side — half the width per plot)
            double tickIntervalMs = timeRangeMs <= 5 ? 1 : timeRangeMs <= 15 ? 2 : timeRangeMs <= 30 ? 5 : timeRangeMs <= 60 ? 10 : 20;
            if (sideBySide) tickIntervalMs *= 2;
            int tickCount = static_cast<int>(timeRangeMs / tickIntervalMs);

            for (int i = 0; i <= tickCount; ++i)
            {
                double timeMs = i * tickIntervalMs;
                float frac = static_cast<float>(timeMs / timeRangeMs);
                float x = r.getX() + frac * r.getWidth();

                g.setColour(juce::Colour(0x28ffffff));
                g.drawVerticalLine(static_cast<int>(x), r.getY(), r.getBottom());

                g.setColour(juce::Colour(0xff888899));
                g.setFont(labelFont);
                g.drawText(juce::String(static_cast<int>(timeMs)),
                           static_cast<int>(x - 12), static_cast<int>(r.getBottom() + 1), 24, 14,
                           juce::Justification::centred);
            }

            // Trace(s)
            auto strokeChannel = [&] (int channel, juce::Colour c)
            {
                auto& samples = captureRef.getSnapshot(channel);
                int displaySamples = static_cast<int>(timeRangeMs / 1000.0 * captureRef.getSampleRate());
                int samplesToShow = std::min(displaySamples, static_cast<int>(samples.size()));
                if (samplesToShow < 2) return;

                juce::Path path;
                float xStep = r.getWidth() / static_cast<float>(samplesToShow - 1);
                path.startNewSubPath(r.getX(), pMidY - samples[0] * (r.getHeight() / 2.0f) * 0.9f);
                for (int i = 1; i < samplesToShow; ++i)
                    path.lineTo(r.getX() + static_cast<float>(i) * xStep,
                                pMidY - samples[i] * (r.getHeight() / 2.0f) * 0.9f);
                g.setColour(c);
                g.strokePath(path, juce::PathStrokeType(1.5f));
            };
            if (on)
            {
                if (ch2 >= 0) strokeChannel(ch2, kRightColour);   // overlay fallback: R below L
                strokeChannel(ch, col);
            }

            // Border + channel tag
            g.setColour(juce::Colour(0xff333355));
            g.drawRoundedRectangle(r, 0.0f, 1.0f);
            if (tag != nullptr)
            {
                g.setColour(col);
                g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
                g.drawText(tag, static_cast<int>(r.getX() + 5), static_cast<int>(r.getY() + 3), 16, 14,
                           juce::Justification::centredLeft);
                if (ch2 >= 0)
                {
                    g.setColour(kRightColour);
                    g.drawText("R", static_cast<int>(r.getX() + 22), static_cast<int>(r.getY() + 3), 16, 14,
                               juce::Justification::centredLeft);
                }
            }
        };

        if (sideBySide)
        {
            drawPlot({ leftMargin,                    topMargin, halfW, plotH }, 0, strokeColour, "L");
            drawPlot({ leftMargin + halfW + plotGap,  topMargin, halfW, plotH }, 1, kRightColour, "R");
        }
        else if (stereo)   // emergency overlay: both traces, two colours, one diagram
            drawPlot({ leftMargin, topMargin, availW, plotH }, 0, strokeColour, "L", 1);
        else
            drawPlot({ leftMargin, topMargin, availW, plotH }, 0, strokeColour, nullptr);

        // Time-unit tag (once, far right)
        g.setColour(juce::Colour(0xff888899));
        g.setFont(labelFont);
        g.drawText("ms", static_cast<int>(w - rightMargin - 16),
                   static_cast<int>(topMargin + plotH + 1), 16, 14,
                   juce::Justification::centredRight);

        // Title (suppressed in the rack — the module header already shows it)
        if (showTitle)
        {
            g.setColour(juce::Colour(0xff555555));
            g.setFont(juce::FontOptions(10.0f));
            g.drawText("OSCILLOSCOPE", static_cast<int>(leftMargin + 4), 4, 100, 14,
                       juce::Justification::centredLeft);
        }
    }

    void setShowTitle(bool b) { showTitle = b; repaint(); }

    // Optional enable source (the module's APVTS enable, e.g. scopeOn). When off, the
    // display freezes + blanks its trace (so the module enabler truly switches it off,
    // not just dims). Polled read-only, like EnvelopeDisplay reads its params.
    void setEnableSource(std::atomic<float>* p) { enableSrc = p; }

private:
    bool isOn() const { return enableSrc == nullptr || enableSrc->load() >= 0.5f; }

    void timerCallback() override
    {
        const bool on = isOn();
        if (! on)
        {
            if (wasOn) { wasOn = false; repaint(); }   // blank once when switched off
            return;                                    // frozen while off — no capture/repaint
        }
        wasOn = true;
        captureRef.updateSnapshot();
        repaint();
    }

    void updateTimeRange()
    {
        double values[] = { 1.0, 2.0, 5.0, 10.0, 25.0, 50.0, 100.0 };
        int idx = zoomBox.getSelectedId() - 1;
        if (idx >= 0 && idx < (int) (sizeof(values) / sizeof(values[0])))
            timeRangeMs = values[idx];
    }

    WaveformCapture& captureRef;
    juce::Colour strokeColour;                                        // L / mono trace
    static constexpr juce::uint32 kRightColourArgb = 0xfffb923c;      // R trace: orange (colorblind-safe vs blue)
    const juce::Colour kRightColour { kRightColourArgb };
    juce::ComboBox zoomBox;
    double timeRangeMs = 10.0;
    bool showTitle = true;   // false in the rack (module header carries the title)
    std::atomic<float>* enableSrc = nullptr;   // optional module-enable source (scopeOn)
    bool wasOn = true;
};
