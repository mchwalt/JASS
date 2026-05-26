#pragma once
#include <cmath>
#include <algorithm>

enum class FilterType { Off, Lowpass, Highpass };

class BiquadFilter
{
public:
    void setType(FilterType t) { type = t; }
    void setCutoff(double c) { cutoff = c; }
    void setResonance(double q) { resonance = q; }
    void setSampleRate(double sr) { sampleRate = sr; }

    FilterType getType() const { return type; }
    double getCutoff() const { return cutoff; }
    double getResonance() const { return resonance; }

    float process(float input);
    void reset();

private:
    void calculateCoefficients();

    FilterType type = FilterType::Off;
    double cutoff = 5000.0;
    double resonance = 0.707;
    double sampleRate = 44100.0;

    double a0 = 0, a1 = 0, a2 = 0, b1 = 0, b2 = 0;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;

    FilterType lastType = FilterType::Off;
    double lastCutoff = 0, lastQ = 0;
};
