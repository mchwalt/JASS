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

    // Self-FM: offset the read phase by the previous (pre-gain) output. Using the
    // pre-gain value keeps the feedback character independent of the AMP knob. Scaled
    // to ±0.5 cycles at full depth — enough to reach the sine→saw morph and beyond.
    const double fbOffset = feedbackAmount * lastRaw * 0.5;

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
        // Read at the feedback-modulated phase, wrapped to [0,1) so the non-sine
        // waveforms (which assume that range) stay valid.
        double readPhase = phases[i] + fbOffset;
        readPhase -= std::floor(readPhase);
        sum += generateSample(readPhase);

        phases[i] += (freq + fmOffset) / sampleRate;
        if (phases[i] >= 1.0)
            phases[i] -= 1.0;
        if (phases[i] < 0.0)
            phases[i] += 1.0;
    }

    sum /= unisonCount;
    lastRaw = sum;   // pre-gain average, feeds next sample's self-FM
    return static_cast<float>(sum * amplitude);
}
