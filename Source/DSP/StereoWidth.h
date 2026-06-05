#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

// Pseudo-stereo / stereo-width stage. The whole synth engine is MONO: every
// voice writes the same sample to both channels, so by the time the signal
// reaches here L == R and a plain mid-side width control would do nothing
// (the side signal is zero). Instead we synthesise a decorrelated side signal
// from the mono source using the classic complementary-comb (Lauridsen)
// trick:
//
//     delayed = mono delayed by TIME ms
//     L = mono + g * delayed
//     R = mono - g * delayed
//
// The g*delayed term is added to one channel and subtracted from the other, so
// L + R = 2*mono exactly -> the effect is perfectly MONO-COMPATIBLE (it fully
// cancels when summed to mono, no comb-filter colouration on a mono system).
// WIDTH maps to g (0 = untouched mono, higher = wider); TIME sets the comb
// spacing: short (~1-5 ms) = shimmering width, longer (~8-15 ms) = light
// Haas-style room. Runs once on the summed buffer in processBlock, NOT per voice.
class StereoWidth
{
public:
    bool   enabled = false;
    double width   = 0.5;   // 0..1  -> side gain g (0 = mono)
    double timeMs  = 12.0;  // 1..30 ms comb/Haas delay

    void prepare(double sampleRate)
    {
        sr = sampleRate;
        // Size the delay line for the maximum supported TIME (+ slack).
        const int maxSamples = (int) std::ceil(sr * kMaxTimeMs / 1000.0) + 2;
        delayLine.assign((size_t) maxSamples, 0.0f);
        writePos = 0;
    }

    void reset()
    {
        std::fill(delayLine.begin(), delayLine.end(), 0.0f);
        writePos = 0;
    }

    // Operates in place on a (mono-content) stereo buffer.
    void process(juce::AudioBuffer<float>& buffer)
    {
        if (! enabled || buffer.getNumChannels() < 2 || delayLine.empty())
            return;

        const int n = (int) delayLine.size();
        const int delaySamples = std::clamp((int) std::round(sr * timeMs / 1000.0), 1, n - 1);
        const float g = (float) std::clamp(width, 0.0, 1.0) * 0.9f; // cap so |L|,|R| stay sane

        float* left  = buffer.getWritePointer(0);
        float* right = buffer.getWritePointer(1);
        const int numSamples = buffer.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
        {
            // Mono source (L == R coming in); average to be safe.
            const float mono = 0.5f * (left[i] + right[i]);

            int readPos = writePos - delaySamples;
            if (readPos < 0) readPos += n;
            const float delayed = delayLine[(size_t) readPos];

            left[i]  = mono + g * delayed;
            right[i] = mono - g * delayed;

            delayLine[(size_t) writePos] = mono;
            if (++writePos >= n) writePos = 0;
        }
    }

private:
    static constexpr double kMaxTimeMs = 15.0;
    std::vector<float> delayLine;
    int writePos = 0;
    double sr = 44100.0;
};
