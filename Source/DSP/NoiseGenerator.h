#pragma once
#include <array>
#include <cstdint>

// Append-only: Brown/Blue added after Pink (persisted by name, choice index maps to enum +1).
enum class NoiseType { Off, White, Pink, Brown, Blue };

// White + Pink + Brown + Blue noise.
//  - Pink  (-3 dB/oct): Voss-McCartney.
//  - Brown (-6 dB/oct): leaky-integrated white → deep, rumbly.
//  - Blue  (+3 dB/oct): differentiated pink → bright, airy (the spectral mirror of pink).
// Ported/extended from the C# Synthy NoiseGenerator.
class NoiseGenerator
{
public:
    void setType(NoiseType t) { type = t; }
    void setAmplitude(double amp) { amplitude = amp; }

    float nextSample()
    {
        if (type == NoiseType::Off || amplitude < 0.001)
            return 0.0f;

        double sample;
        switch (type)
        {
            case NoiseType::Pink:  sample = pinkNoise();  break;
            case NoiseType::Brown: sample = brownNoise(); break;
            case NoiseType::Blue:  sample = blueNoise();  break;
            default:               sample = whiteNoise(); break;
        }
        return static_cast<float>(sample * amplitude);
    }

    void reset()
    {
        pinkRows.fill(0.0);
        pinkIndex = 0;
        pinkRunningSum = 0.0;
        brownState = 0.0;
        bluePrevPink = 0.0;
    }

private:
    // Fast xorshift PRNG -> [-1, 1)
    double whiteNoise()
    {
        rngState ^= rngState << 13;
        rngState ^= rngState >> 7;
        rngState ^= rngState << 17;
        // Map to [0,1) then to [-1,1)
        double u = (rngState >> 11) * (1.0 / 9007199254740992.0); // 2^53
        return u * 2.0 - 1.0;
    }

    double pinkNoise()
    {
        // Voss-McCartney: update one random row per sample based on changed bits.
        unsigned int lastIndex = pinkIndex;
        pinkIndex++;

        unsigned int diff = lastIndex ^ pinkIndex;
        for (int row = 0; row < (int) pinkRows.size(); ++row)
        {
            if ((diff & (1u << row)) != 0)
            {
                pinkRunningSum -= pinkRows[(size_t) row];
                pinkRows[(size_t) row] = whiteNoise();
                pinkRunningSum += pinkRows[(size_t) row];
                break;
            }
        }

        double white = whiteNoise();
        return (pinkRunningSum + white) / (pinkRows.size() + 1);
    }

    // Brown: leaky integrator of white (−6 dB/oct). The /1.02 leak bleeds off DC so it
    // never wanders to the rails; ×3.5 restores a comparable output level.
    double brownNoise()
    {
        brownState = (brownState + 0.02 * whiteNoise()) / 1.02;
        return brownState * 3.5;
    }

    // Blue: first difference of pink (+3 dB/oct → bright, the mirror image of pink).
    double blueNoise()
    {
        double p = pinkNoise();
        double out = p - bluePrevPink;
        bluePrevPink = p;
        return out * 2.0;   // gain compensation for the difference
    }

    NoiseType type = NoiseType::Off;
    double amplitude = 0.3;

    std::array<double, 16> pinkRows{};
    unsigned int pinkIndex = 0;
    double pinkRunningSum = 0.0;
    double brownState = 0.0;
    double bluePrevPink = 0.0;
    std::uint64_t rngState = 0x853c49e6748fea9bULL;
};
