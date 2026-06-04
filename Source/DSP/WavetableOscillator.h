#pragma once
#include "WavetableBank.h"
#include <array>
#include <algorithm>
#include <cmath>

// Wavetable oscillator: plays a WavetableBank, morphing through frames via
// the Position parameter. Supports unison/detune like the main oscillator.
// Ported from the C# Synthy WavetableOscillator.
class WavetableOscillator
{
public:
    static constexpr int maxUnison = 7;

    void setEnabled(bool on) { enabled = on; }
    void setFrequency(double f) { frequency = f; }
    void setAmplitude(double a) { amplitude = a; }
    void setPosition(double p) { position = p; }
    void setSampleRate(double sr) { sampleRate = sr; }
    void setUnisonCount(int c) { unisonCount = std::clamp(c, 1, maxUnison); }
    void setDetuneAmount(double cents) { detuneCents = cents; }
    void setBank(const WavetableBank* b) { bank = b; }
    double getFrequency() const { return frequency; }

    void reset() { phases.fill(0.0); }

    float nextSample()
    {
        if (!enabled || amplitude < 0.001 || bank == nullptr)
            return 0.0f;

        double sum = 0.0;
        for (int i = 0; i < unisonCount; ++i)
        {
            double detuneOffset = 0.0;
            if (unisonCount > 1)
            {
                double spread = (2.0 * i / (unisonCount - 1)) - 1.0;
                detuneOffset = spread * detuneCents;
            }
            double freq = frequency * std::pow(2.0, detuneOffset / 1200.0);

            sum += bank->getSample(phases[(size_t) i], position);

            phases[(size_t) i] += freq / sampleRate;
            if (phases[(size_t) i] >= 1.0)
                phases[(size_t) i] -= 1.0;
        }

        sum /= unisonCount;
        return static_cast<float>(sum * amplitude);
    }

private:
    std::array<double, maxUnison> phases{};
    const WavetableBank* bank = nullptr;
    bool enabled = false;
    double frequency = 261.63;
    double amplitude = 0.5;
    double position = 0.0;
    double sampleRate = 44100.0;
    double detuneCents = 25.0;
    int unisonCount = 1;
};
