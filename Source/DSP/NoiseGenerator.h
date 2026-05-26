#pragma once
#include <array>
#include <cstdint>

enum class NoiseType { Off, White, Pink };

// White + Pink noise. Pink uses the Voss-McCartney algorithm.
// Ported from the C# Synthy NoiseGenerator.
class NoiseGenerator
{
public:
    void setType(NoiseType t) { type = t; }
    void setAmplitude(double amp) { amplitude = amp; }

    float nextSample()
    {
        if (type == NoiseType::Off || amplitude < 0.001)
            return 0.0f;

        double sample = (type == NoiseType::Pink) ? pinkNoise() : whiteNoise();
        return static_cast<float>(sample * amplitude);
    }

    void reset()
    {
        pinkRows.fill(0.0);
        pinkIndex = 0;
        pinkRunningSum = 0.0;
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

    NoiseType type = NoiseType::Off;
    double amplitude = 0.3;

    std::array<double, 16> pinkRows{};
    unsigned int pinkIndex = 0;
    double pinkRunningSum = 0.0;
    std::uint64_t rngState = 0x853c49e6748fea9bULL;
};
