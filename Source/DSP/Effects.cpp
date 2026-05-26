#include "Effects.h"

// --- Distortion ---

static double distortionFoldback(double x)
{
    // Fold signal back when it exceeds ±1
    while (x > 1.0 || x < -1.0)
    {
        if (x > 1.0) x = 2.0 - x;
        if (x < -1.0) x = -2.0 - x;
    }
    return x;
}

float DistortionEffect::process(float input)
{
    if (type == DistortionType::Off) return input;

    double gain = 1.0 + drive * 19.0; // 1x to 20x
    double driven = input * gain;

    double distorted;
    switch (type)
    {
        case DistortionType::SoftClip: distorted = std::tanh(driven); break;
        case DistortionType::HardClip: distorted = std::clamp(driven, -1.0, 1.0); break;
        case DistortionType::Foldback: distorted = distortionFoldback(driven); break;
        default:                       distorted = driven; break;
    }

    double output = input * (1.0 - mix) + distorted * mix;
    return static_cast<float>(std::clamp(output, -1.0, 1.0));
}

// --- Delay ---

void DelayEffect::prepare(double sampleRate)
{
    sr = sampleRate;
    buffer.assign(static_cast<int>(2.0 * sr), 0.0f);
    writePos = 0;
}

float DelayEffect::process(float input)
{
    if (!enabled || buffer.empty()) return input;

    int bufSize = static_cast<int>(buffer.size());
    int delaySamples = std::clamp(static_cast<int>(time * sr), 1, bufSize - 1);
    int readPos = (writePos - delaySamples + bufSize) % bufSize;

    float delayed = buffer[readPos];
    buffer[writePos] = input + delayed * static_cast<float>(feedback);
    writePos = (writePos + 1) % bufSize;

    return input * (1.0f - static_cast<float>(mix)) + delayed * static_cast<float>(mix);
}

void DelayEffect::reset()
{
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    writePos = 0;
}

// --- Chorus ---

void ChorusEffect::prepare(double sampleRate)
{
    sr = sampleRate;
    buffer.assign(static_cast<int>(0.05 * sr), 0.0f);
    writePos = 0;
    lfoPhase = 0.0;
}

float ChorusEffect::process(float input)
{
    if (!enabled || buffer.empty()) return input;

    int bufSize = static_cast<int>(buffer.size());
    buffer[writePos] = input;

    double lfo = std::sin(2.0 * M_PI * lfoPhase);
    lfoPhase += rate / sr;
    if (lfoPhase >= 1.0) lfoPhase -= 1.0;

    double delaySec = depth * (1.0 + lfo) * 0.5 + 0.001;
    double delaySamples = delaySec * sr;

    int readPos1 = static_cast<int>(writePos - delaySamples + bufSize * 2) % bufSize;
    int readPos2 = (readPos1 + 1) % bufSize;
    double frac = delaySamples - static_cast<int>(delaySamples);

    float delayed = static_cast<float>(buffer[readPos1] * (1.0 - frac) + buffer[readPos2] * frac);
    writePos = (writePos + 1) % bufSize;

    return input * (1.0f - static_cast<float>(mix)) + delayed * static_cast<float>(mix);
}

void ChorusEffect::reset()
{
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    writePos = 0;
    lfoPhase = 0.0;
}

// --- Reverb ---

float ReverbEffect::CombFilter::process(float input, float fb, float damp)
{
    float output = buffer[pos];
    filterStore = output * (1.0f - damp) + filterStore * damp;
    buffer[pos] = input + filterStore * fb;
    pos = (pos + 1) % static_cast<int>(buffer.size());
    return output;
}

float ReverbEffect::AllpassFilter::process(float input)
{
    float delayed = buffer[pos];
    float output = -input + delayed;
    buffer[pos] = input + delayed * 0.5f;
    pos = (pos + 1) % static_cast<int>(buffer.size());
    return output;
}

void ReverbEffect::prepare(double sampleRate)
{
    double scale = sampleRate / 44100.0;
    combs[0].init(static_cast<int>(1116 * scale));
    combs[1].init(static_cast<int>(1188 * scale));
    combs[2].init(static_cast<int>(1277 * scale));
    combs[3].init(static_cast<int>(1356 * scale));
    allpasses[0].init(static_cast<int>(556 * scale));
    allpasses[1].init(static_cast<int>(441 * scale));
    initialized = true;
}

float ReverbEffect::process(float input)
{
    if (!enabled || !initialized) return input;

    float fb = static_cast<float>(roomSize * 0.9 + 0.1);
    float damp = static_cast<float>(damping);

    float combOut = 0.0f;
    for (auto& comb : combs)
        combOut += comb.process(input, fb, damp);
    combOut *= 0.25f;

    float output = combOut;
    for (auto& ap : allpasses)
        output = ap.process(output);

    return input * (1.0f - static_cast<float>(mix)) + output * static_cast<float>(mix);
}

void ReverbEffect::reset()
{
    initialized = false;
}
