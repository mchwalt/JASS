#pragma once
#include <algorithm>
#include <cmath>

// Minimal one-shot pitch envelope: on trigger it jumps to 1.0 and decays
// exponentially toward 0. The voice multiplies the oscillator pitch by
// 2^((amountSemitones/12) * value), so a positive amount sweeps DOWN to the
// base pitch (kicks, zaps) and a negative amount sweeps UP (lasers, risers).
// Not an ADSR — deliberately just an attack-less decay, the classic pitch-env shape.
class PitchEnvelope
{
public:
    void setSampleRate(double sr) { sampleRate = sr < 1.0 ? 44100.0 : sr; }
    void setDecay(double seconds) { decaySec = seconds; }   // time to fall ~to zero

    void trigger() { value = 1.0; }
    void reset()   { value = 0.0; }

    // Advance one sample and return the current 1→0 envelope value.
    double process()
    {
        // Exponential decay: reach ~0.001 (−60 dB) after decaySec.
        const double t = std::max(0.0005, decaySec);
        const double coeff = std::exp(-6.9077553 / (t * sampleRate)); // ln(1000) ≈ 6.9078
        value *= coeff;
        return value;
    }

private:
    double value = 0.0;
    double decaySec = 0.3;
    double sampleRate = 44100.0;
};
