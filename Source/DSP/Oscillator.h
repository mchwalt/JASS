#pragma once
#include <cmath>
#include <array>
#include <algorithm>

enum class WaveformType { Sine, Sawtooth, Square, Triangle };
enum class MixMode { RingMod, FM };   // "Additive" (no coupling) = CROSS MOD module disabled (mixModeOn=false)

class Oscillator
{
public:
    static constexpr int maxUnison = 7;

    void setEnabled(bool on) { enabled = on; }
    void setFrequency(double freq) { frequency = freq; }
    void setAmplitude(double amp) { amplitude = amp; }
    void setWaveform(WaveformType type) { waveform = type; }
    void setSampleRate(double sr) { sampleRate = sr; }
    void setUnisonCount(int count) { unisonCount = std::clamp(count, 1, maxUnison); }
    void setDetuneAmount(double cents) { detuneCents = cents; }
    // Self-FM (feedback): the oscillator's own previous output offsets its phase.
    // 0 = pure waveform; rising values morph a sine toward a saw and, near the top,
    // into inharmonic/chaotic timbres. See Oscillator::nextSample.
    void setFeedback(double fb) { feedbackAmount = std::clamp(fb, 0.0, 1.0); }
    void reset() { phases.fill(0.0); lastRaw = 0.0; }

    bool isEnabled() const { return enabled; }
    double getFrequency() const { return frequency; }
    double getAmplitude() const { return amplitude; }
    WaveformType getWaveform() const { return waveform; }

    float nextSample(double fmOffset = 0.0);

private:
    double generateSample(double ph) const;

    std::array<double, maxUnison> phases{};
    bool enabled = false;
    double frequency = 440.0;
    double amplitude = 0.5;
    double sampleRate = 44100.0;
    double detuneCents = 25.0;
    int unisonCount = 1;
    WaveformType waveform = WaveformType::Sine;
    double feedbackAmount = 0.0;   // Self-FM depth (0..1); 0 = off
    double lastRaw = 0.0;          // previous pre-gain output, fed back into the phase
};
