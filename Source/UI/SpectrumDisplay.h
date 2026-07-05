#pragma once
#include <JuceHeader.h>
#include "../DSP/WaveformCapture.h"

class SpectrumDisplay : public juce::Component, private juce::Timer
{
public:
    static constexpr int fftOrder = 10;          // 2^10 = 1024 points
    static constexpr int fftSize = 1 << fftOrder;

    SpectrumDisplay(WaveformCapture& capture, juce::Colour color = juce::Colour(0xffa78bfa))
        : captureRef(capture), strokeColour(color),
          fft(fftOrder),
          window(fftSize, juce::dsp::WindowingFunction<float>::hann)
    {
        smoothedMagnitudes.resize(fftSize / 2, 0.0f);
        startTimerHz(30);
    }

    ~SpectrumDisplay() override { stopTimer(); }

    void resized() override {}

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        float w = bounds.getWidth();
        float h = bounds.getHeight();

        constexpr float leftMargin = 38.0f;
        constexpr float bottomMargin = 16.0f;
        constexpr float topMargin = 22.0f;
        constexpr float rightMargin = 6.0f;

        float plotW = w - leftMargin - rightMargin;
        float plotH = h - topMargin - bottomMargin;

        // Background
        g.setColour(juce::Colour(0xff151528));
        g.fillRoundedRectangle(bounds, 8.0f);

        // Y-axis: dB scale
        auto labelFont = juce::FontOptions(9.0f);
        float dbLevels[] = { 0.0f, -12.0f, -24.0f, -36.0f, -48.0f };
        for (auto db : dbLevels)
        {
            float yNorm = juce::jmap(db, mindB, 0.0f, 1.0f, 0.0f);
            float y = topMargin + yNorm * plotH;

            g.setColour(db == 0.0f ? juce::Colour(0x40ffffff) : juce::Colour(0x28ffffff));
            g.drawHorizontalLine(static_cast<int>(y), leftMargin, w - rightMargin);

            g.setColour(juce::Colour(0xff888899));
            g.setFont(labelFont);
            g.drawText(juce::String(static_cast<int>(db)),
                       0, static_cast<int>(y - 6), static_cast<int>(leftMargin - 4), 12,
                       juce::Justification::centredRight);
        }

        // X-axis: frequency ticks (log scale)
        float freqTicks[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
        for (auto freq : freqTicks)
        {
            if (freq < minFreq || freq > maxFreq) continue;
            float xNorm = freqToX(freq);
            float x = leftMargin + xNorm * plotW;

            g.setColour(juce::Colour(0x28ffffff));
            g.drawVerticalLine(static_cast<int>(x), topMargin, topMargin + plotH);

            g.setColour(juce::Colour(0xff888899));
            g.setFont(labelFont);
            juce::String label = freq >= 1000.0f
                ? juce::String(static_cast<int>(freq / 1000)) + "k"
                : juce::String(static_cast<int>(freq));
            g.drawText(label, static_cast<int>(x - 14),
                       static_cast<int>(topMargin + plotH + 1), 28, 14,
                       juce::Justification::centred);
        }

        g.setColour(juce::Colour(0xff888899));
        g.setFont(labelFont);
        g.drawText("Hz", static_cast<int>(w - rightMargin - 16),
                   static_cast<int>(topMargin + plotH + 1), 16, 14,
                   juce::Justification::centredRight);

        // Spectrum path
        int numBins = fftSize / 2;
        float binWidth = sampleRate / static_cast<float>(fftSize);

        juce::Path path;
        bool pathStarted = false;

        for (int i = 1; i < numBins; ++i)
        {
            float freq = i * binWidth;
            if (freq < minFreq || freq > maxFreq) continue;

            float xNorm = freqToX(freq);
            float x = leftMargin + xNorm * plotW;

            float mag = smoothedMagnitudes[i];
            float db = mag > 0.0f ? 20.0f * std::log10(mag) : mindB;
            db = std::max(db, mindB);

            float yNorm = juce::jmap(db, mindB, 0.0f, 1.0f, 0.0f);
            float y = topMargin + yNorm * plotH;

            if (!pathStarted)
            {
                path.startNewSubPath(x, y);
                pathStarted = true;
            }
            else
            {
                path.lineTo(x, y);
            }
        }

        if (pathStarted)
        {
            g.setColour(strokeColour.withAlpha(0.15f));
            juce::Path filled(path);
            filled.lineTo(leftMargin + plotW, topMargin + plotH);
            filled.lineTo(leftMargin, topMargin + plotH);
            filled.closeSubPath();
            g.fillPath(filled);

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
            g.drawText("SPECTRUM", static_cast<int>(leftMargin + 4), 4, 100, 14,
                       juce::Justification::centredLeft);
        }
    }

    void setSampleRate(double sr) { sampleRate = static_cast<float>(sr); }
    void setShowTitle(bool b) { showTitle = b; repaint(); }

private:
    void timerCallback() override
    {
        // Track the engine's real sample rate (bin→Hz mapping); set from prepareToPlay.
        sampleRate = static_cast<float>(captureRef.getSampleRate());
        captureRef.updateSnapshot();
        computeFFT();
        repaint();
    }

    void computeFFT()
    {
        auto& snapshot = captureRef.getSnapshot();
        int len = static_cast<int>(snapshot.size());

        // Fill FFT buffer (zero-pad if snapshot is shorter than fftSize)
        std::array<float, fftSize * 2> fftData{};
        for (int i = 0; i < std::min(len, fftSize); ++i)
            fftData[i] = snapshot[i];

        window.multiplyWithWindowingTable(fftData.data(), fftSize);
        fft.performFrequencyOnlyForwardTransform(fftData.data());

        // Normalize and smooth
        float invN = 2.0f / fftSize;
        constexpr float smoothing = 0.7f;

        for (int i = 0; i < fftSize / 2; ++i)
        {
            float mag = fftData[i] * invN;
            smoothedMagnitudes[i] = smoothedMagnitudes[i] * smoothing + mag * (1.0f - smoothing);
        }
    }

    float freqToX(float freq) const
    {
        return (std::log2(freq) - std::log2(minFreq))
             / (std::log2(maxFreq) - std::log2(minFreq));
    }

    WaveformCapture& captureRef;
    juce::Colour strokeColour;
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;

    std::vector<float> smoothedMagnitudes;
    float sampleRate = 44100.0f;
    bool showTitle = true;   // false in the rack (module header carries the title)

    static constexpr float minFreq = 30.0f;
    static constexpr float maxFreq = 16000.0f;
    static constexpr float mindB = -48.0f;
};
