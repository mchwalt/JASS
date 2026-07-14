#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

// Master-bus compressor. Unlike the per-voice effects, this runs ONCE on the
// summed output buffer in processBlock (a compressor must see the whole mix to
// "glue" it — per-voice compression would gate each note independently and give
// no bus-glue). Feed-forward, stereo-linked peak detection (the louder of L/R
// drives the gain reduction so the stereo image stays centred). The gain-
// reduction envelope is smoothed with separate attack/release time constants.
// Placed before the pseudo-stereo width stage, so it compresses the mono mix.
class Compressor
{
public:
    bool   enabled   = false;
    double threshold = -18.0;  // dB, level above which reduction starts
    double ratio     = 2.0;    // 1 (none) .. 20 (limiting)
    double attackMs  = 10.0;   // gain-reduction attack
    double releaseMs = 120.0;  // gain-reduction release
    double makeupDb  = 0.0;    // output make-up gain

    void prepare(double sampleRate)
    {
        sr = sampleRate < 1.0 ? 44100.0 : sampleRate;
        envDb = 0.0;
    }

    void reset() { envDb = 0.0; }

    // Operates in place on the (mono-content) stereo buffer.
    void process(juce::AudioBuffer<float>& buffer)
    {
        if (! enabled || buffer.getNumChannels() < 1)
            return;

        const int numCh = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();
        const double slope = 1.0 - 1.0 / std::max(1.0, ratio);   // 0 (ratio 1) .. ~1 (limiting)
        const double aCoeff = std::exp(-1.0 / (std::max(0.05, attackMs)  * 0.001 * sr));
        const double rCoeff = std::exp(-1.0 / (std::max(1.0,  releaseMs) * 0.001 * sr));
        const double makeup = std::pow(10.0, makeupDb / 20.0);

        float* ch0 = buffer.getWritePointer(0);
        float* ch1 = numCh > 1 ? buffer.getWritePointer(1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            // Stereo-linked peak detector.
            const double peak = ch1 != nullptr
                                    ? std::max(std::abs((double) ch0[i]), std::abs((double) ch1[i]))
                                    : std::abs((double) ch0[i]);
            const double inDb = 20.0 * std::log10(peak + 1.0e-9);
            const double overDb = inDb - threshold;
            const double targetGr = overDb > 0.0 ? overDb * slope : 0.0;   // desired reduction (dB, ≥0)

            // Smooth the gain reduction: fast attack when it rises, slow release when it falls.
            const double coeff = targetGr > envDb ? aCoeff : rCoeff;
            envDb = coeff * envDb + (1.0 - coeff) * targetGr;

            const float gain = (float) (makeup * std::pow(10.0, -envDb / 20.0));
            ch0[i] *= gain;
            if (ch1 != nullptr) ch1[i] *= gain;
        }
    }

private:
    double sr = 44100.0;
    double envDb = 0.0;   // smoothed gain reduction in dB (≥ 0)
};
