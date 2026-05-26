#include "BiquadFilter.h"

float BiquadFilter::process(float input)
{
    if (type == FilterType::Off)
        return input;

    if (type != lastType || std::abs(cutoff - lastCutoff) > 0.01 || std::abs(resonance - lastQ) > 0.001)
        calculateCoefficients();

    double output = a0 * input + a1 * x1 + a2 * x2 - b1 * y1 - b2 * y2;

    x2 = x1; x1 = input;
    y2 = y1; y1 = output;

    if (std::isnan(output) || std::isinf(output))
    {
        output = 0;
        y1 = y2 = x1 = x2 = 0;
    }

    return static_cast<float>(output);
}

void BiquadFilter::calculateCoefficients()
{
    lastType = type;
    lastCutoff = cutoff;
    lastQ = resonance;

    double w0 = 2.0 * M_PI * std::clamp(cutoff, 20.0, sampleRate * 0.45) / sampleRate;
    double alpha = std::sin(w0) / (2.0 * std::max(resonance, 0.1));
    double cosW0 = std::cos(w0);

    double normA0;

    switch (type)
    {
        case FilterType::Lowpass:
            a0 = (1.0 - cosW0) / 2.0;
            a1 = 1.0 - cosW0;
            a2 = (1.0 - cosW0) / 2.0;
            b1 = -2.0 * cosW0;
            b2 = 1.0 - alpha;
            normA0 = 1.0 + alpha;
            break;

        case FilterType::Highpass:
            a0 = (1.0 + cosW0) / 2.0;
            a1 = -(1.0 + cosW0);
            a2 = (1.0 + cosW0) / 2.0;
            b1 = -2.0 * cosW0;
            b2 = 1.0 - alpha;
            normA0 = 1.0 + alpha;
            break;

        default: return;
    }

    a0 /= normA0; a1 /= normA0; a2 /= normA0;
    b1 /= normA0; b2 /= normA0;
}

void BiquadFilter::reset()
{
    x1 = x2 = y1 = y2 = 0;
}
