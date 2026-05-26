#include "Oscillator.h"

double Oscillator::generateSample(double ph) const
{
    switch (waveform)
    {
        case WaveformType::Sine:
            return std::sin(2.0 * M_PI * ph);
        case WaveformType::Sawtooth:
            return 2.0 * ph - 1.0;
        case WaveformType::Square:
            return ph < 0.5 ? 1.0 : -1.0;
        case WaveformType::Triangle:
            return 4.0 * std::abs(ph - 0.5) - 1.0;
    }
    return 0.0;
}

float Oscillator::nextSample(double fmOffset)
{
    if (!enabled || amplitude < 0.001)
        return 0.0f;

    double sum = 0.0;

    for (int i = 0; i < unisonCount; ++i)
    {
        // Symmetric detune: voice 0 = center, others spread ± evenly
        double detuneOffset = 0.0;
        if (unisonCount > 1)
        {
            // Map voice index to range -1..+1
            double spread = (2.0 * i / (unisonCount - 1)) - 1.0;
            detuneOffset = spread * detuneCents;
        }

        double freq = frequency * std::pow(2.0, detuneOffset / 1200.0);
        sum += generateSample(phases[i]);

        phases[i] += (freq + fmOffset) / sampleRate;
        if (phases[i] >= 1.0)
            phases[i] -= 1.0;
        if (phases[i] < 0.0)
            phases[i] += 1.0;
    }

    sum /= unisonCount;
    return static_cast<float>(sum * amplitude);
}
