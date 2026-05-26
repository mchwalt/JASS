#pragma once
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum class LFOWaveform { Sine, Triangle, Square, Sawtooth };
enum class LFOTarget { Off, Frequency, Amplitude, FilterCutoff };

class LFO
{
public:
    void setRate(double r) { rate = r; }
    void setDepth(double d) { depth = d; }
    void setWaveform(LFOWaveform w) { waveform = w; }
    void setTarget(LFOTarget t) { target = t; }
    void setSampleRate(double sr) { sampleRate = sr; }

    double getRate() const { return rate; }
    double getDepth() const { return depth; }
    LFOWaveform getWaveform() const { return waveform; }
    LFOTarget getTarget() const { return target; }

    // Returns bipolar value -1..+1
    float process()
    {
        if (target == LFOTarget::Off)
            return 0.0f;

        double value = 0.0;

        switch (waveform)
        {
            case LFOWaveform::Sine:
                value = std::sin(2.0 * M_PI * phase);
                break;
            case LFOWaveform::Triangle:
                value = phase < 0.5
                    ? 4.0 * phase - 1.0
                    : 3.0 - 4.0 * phase;
                break;
            case LFOWaveform::Square:
                value = phase < 0.5 ? 1.0 : -1.0;
                break;
            case LFOWaveform::Sawtooth:
                value = 2.0 * phase - 1.0;
                break;
        }

        phase += rate / sampleRate;
        if (phase >= 1.0) phase -= 1.0;

        return static_cast<float>(value * depth);
    }

    void reset() { phase = 0.0; }

private:
    double phase = 0.0;
    double rate = 2.0;
    double depth = 0.5;
    double sampleRate = 44100.0;
    LFOWaveform waveform = LFOWaveform::Sine;
    LFOTarget target = LFOTarget::Off;
};
