#pragma once
#include <vector>
#include <cmath>
#include <cstdint>

// Karplus-Strong plucked-string synthesis.
// A burst of noise is fed into a delay line with a low-pass filter in the
// feedback loop. Ported from the C# Synthy KarplusStrong.
class KarplusStrong
{
public:
    void setEnabled(bool on) { enabled = on; }
    void setFrequency(double f) { frequency = f; }
    void setAmplitude(double a) { amplitude = a; }
    void setDamping(double d) { damping = d; }   // 0 = bright/long, 1 = muffled/short
    void setStretch(double s) { stretch = s; }   // 0 = normal, 1 = inharmonic/bell-like
    void setSampleRate(double sr) { sampleRate = sr; }
    double getFrequency() const { return frequency; }

    // Trigger (pluck) the string — fills the delay buffer with noise.
    void pluck()
    {
        if (!enabled || frequency < 20.0)
            return;

        bufferLength = std::max(2, (int) (sampleRate / frequency));
        buffer.assign((size_t) bufferLength, 0.0f);
        writePos = 0;
        prevSample = 0.0f;

        for (int i = 0; i < bufferLength; ++i)
            buffer[(size_t) i] = whiteNoise();

        ampEnv = 1.0f;
        isActive = true;
    }

    float nextSample()
    {
        if (!enabled || !isActive || buffer.empty())
            return 0.0f;

        float current = buffer[(size_t) writePos];

        int nextPos = (writePos + 1) % bufferLength;
        float next = buffer[(size_t) nextPos];

        float dampFactor = 1.0f - (float) damping * 0.03f;
        float averaged = (current + next) * 0.5f * dampFactor;

        // Stretch: occasionally keep sample unchanged (bell/drum tones)
        if (stretch > 0.01)
        {
            float stretchProb = (float) stretch * 0.5f;
            if (whiteUnipolar() < stretchProb)
                averaged = current * dampFactor;
        }

        // One-pole low-pass for extra damping control
        float filterAmount = (float) damping * 0.6f;
        averaged = prevSample * filterAmount + averaged * (1.0f - filterAmount);
        prevSample = averaged;

        buffer[(size_t) writePos] = averaged;
        writePos = nextPos;

        // Deactivate only once the string has TRULY decayed. Testing the
        // instantaneous sample was buggy: the waveform crosses zero constantly,
        // so two tiny consecutive samples (a zero crossing) could kill a string
        // that was still ringing -> occasional abrupt cut-offs. Use a slowly
        // decaying peak follower instead.
        float a = std::abs(current);
        ampEnv = (a > ampEnv) ? a : ampEnv * 0.9995f;
        if (ampEnv < 0.0002f)
            isActive = false;

        return current * (float) amplitude;
    }

    void reset()
    {
        buffer.clear();
        writePos = 0;
        isActive = false;
        prevSample = 0.0f;
        ampEnv = 0.0f;
    }

private:
    double whiteUnipolar()
    {
        rngState ^= rngState << 13;
        rngState ^= rngState >> 7;
        rngState ^= rngState << 17;
        return (rngState >> 11) * (1.0 / 9007199254740992.0);
    }
    float whiteNoise() { return (float) (whiteUnipolar() * 2.0 - 1.0); }

    std::vector<float> buffer;
    int writePos = 0;
    int bufferLength = 0;
    bool isActive = false;
    float prevSample = 0.0f;
    float ampEnv = 0.0f;   // peak follower for end-of-string detection

    bool enabled = false;
    double frequency = 261.63;
    double amplitude = 0.5;
    double damping = 0.5;
    double stretch = 0.0;
    double sampleRate = 44100.0;
    std::uint64_t rngState = 0x2545f4914f6cdd1dULL;
};
