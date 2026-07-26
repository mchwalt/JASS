#pragma once
#include <cmath>
#include "ModTargets.h"   // LFOTarget + the modulation-target vocabulary (single source of truth)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum class LFOWaveform { Sine, Triangle, Square, Sawtooth };
// LFOTarget + the full target vocabulary live in ModTargets.h (single source of truth) — add a
// target there and the enum, persist strings, DEST labels and enable-map all update together.

// Number of LFOs, indexed like the oscillators (lfoOn(i), lfoRate(i) …). Grow HERE:
// bumping this + appending a ModSource + the source-index map is all a new LFO needs.
inline constexpr int kNumLFOs = 4;

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
        if (phase < 0.0)  phase += 1.0;   // wrap for a negative rate too (mirrors the oscillator)

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
