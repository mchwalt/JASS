#pragma once
#include <JuceHeader.h>
#include "../DSP/WaveformCapture.h"

class WaveformDisplay : public juce::Component, private juce::Timer
{
public:
    WaveformDisplay(WaveformCapture& capture, juce::Colour color = juce::Colour(0xff40c0ff))
        : captureRef(capture), strokeColour(color)
    {
        zoomBox.addItemList({"1 ms", "2 ms", "5 ms", "10 ms", "25 ms", "50 ms"}, 1);
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

        float plotW = w - leftMargin - rightMargin;
        float plotH = h - topMargin - bottomMargin;
        float midY = topMargin + plotH / 2.0f;

        // Background
        g.setColour(juce::Colour(0xff151528));
        g.fillRoundedRectangle(bounds, 8.0f);

        // Y-axis
        float yLevels[] = { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f };
        auto labelFont = juce::FontOptions(9.0f);
        for (auto level : yLevels)
        {
            float y = midY - level * (plotH / 2.0f) * 0.9f;
            g.setColour(level == 0.0f ? juce::Colour(0x40ffffff) : juce::Colour(0x28ffffff));
            g.drawHorizontalLine(static_cast<int>(y), leftMargin, w - rightMargin);

            g.setColour(juce::Colour(0xff888899));
            g.setFont(labelFont);
            juce::String label = (level >= 0 ? "+" : "") + juce::String(level, 1);
            g.drawText(label, 0, static_cast<int>(y - 6), static_cast<int>(leftMargin - 4), 12,
                       juce::Justification::centredRight);
        }

        // X-axis: integer ms ticks
        double tickIntervalMs = timeRangeMs <= 5 ? 1 : timeRangeMs <= 15 ? 2 : timeRangeMs <= 30 ? 5 : 10;
        int tickCount = static_cast<int>(timeRangeMs / tickIntervalMs);

        for (int i = 0; i <= tickCount; ++i)
        {
            double timeMs = i * tickIntervalMs;
            float frac = static_cast<float>(timeMs / timeRangeMs);
            float x = leftMargin + frac * plotW;

            g.setColour(juce::Colour(0x28ffffff));
            g.drawVerticalLine(static_cast<int>(x), topMargin, topMargin + plotH);

            g.setColour(juce::Colour(0xff888899));
            g.setFont(labelFont);
            g.drawText(juce::String(static_cast<int>(timeMs)),
                       static_cast<int>(x - 12), static_cast<int>(topMargin + plotH + 1), 24, 14,
                       juce::Justification::centred);
        }

        g.setColour(juce::Colour(0xff888899));
        g.setFont(labelFont);
        g.drawText("ms", static_cast<int>(w - rightMargin - 16),
                   static_cast<int>(topMargin + plotH + 1), 16, 14,
                   juce::Justification::centredRight);

        // Waveform
        auto& samples = captureRef.getSnapshot();
        int displaySamples = static_cast<int>(timeRangeMs / 1000.0 * captureRef.getSampleRate());
        int samplesToShow = std::min(displaySamples, static_cast<int>(samples.size()));

        if (isOn() && samplesToShow > 1)
        {
            juce::Path path;
            float xStep = plotW / static_cast<float>(samplesToShow - 1);

            path.startNewSubPath(leftMargin, midY - samples[0] * (plotH / 2.0f) * 0.9f);

            for (int i = 1; i < samplesToShow; ++i)
            {
                float x = leftMargin + static_cast<float>(i) * xStep;
                float y = midY - samples[i] * (plotH / 2.0f) * 0.9f;
                path.lineTo(x, y);
            }

            g.setColour(strokeColour);
            g.strokePath(path, juce::PathStrokeType(1.5f));
        }

        // Border
        g.setColour(juce::Colour(0xff333355));
        g.drawRoundedRectangle(leftMargin, topMargin, plotW, plotH, 0.0f, 1.0f);

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
        double values[] = { 1.0, 2.0, 5.0, 10.0, 25.0, 50.0 };
        int idx = zoomBox.getSelectedId() - 1;
        if (idx >= 0 && idx < (int) (sizeof(values) / sizeof(values[0])))
            timeRangeMs = values[idx];
    }

    WaveformCapture& captureRef;
    juce::Colour strokeColour;
    juce::ComboBox zoomBox;
    double timeRangeMs = 10.0;
    bool showTitle = true;   // false in the rack (module header carries the title)
    std::atomic<float>* enableSrc = nullptr;   // optional module-enable source (scopeOn)
    bool wasOn = true;
};
