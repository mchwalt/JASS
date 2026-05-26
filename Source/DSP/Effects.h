#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

enum class DistortionType { Off, SoftClip, HardClip, Foldback };

class DistortionEffect
{
public:
    DistortionType type = DistortionType::Off;
    double drive = 0.5;   // 0..1 (mapped to gain 1x..20x)
    double mix = 1.0;     // dry/wet 0..1

    float process(float input);
};

class DelayEffect
{
public:
    bool enabled = false;
    double time = 0.3;
    double feedback = 0.4;
    double mix = 0.3;

    void prepare(double sampleRate);
    float process(float input);
    void reset();

private:
    std::vector<float> buffer;
    int writePos = 0;
    double sr = 44100.0;
};

class ChorusEffect
{
public:
    bool enabled = false;
    double rate = 1.5;
    double depth = 0.005;
    double mix = 0.5;

    void prepare(double sampleRate);
    float process(float input);
    void reset();

private:
    std::vector<float> buffer;
    int writePos = 0;
    double lfoPhase = 0.0;
    double sr = 44100.0;
};

class ReverbEffect
{
public:
    bool enabled = false;
    double roomSize = 0.7;
    double damping = 0.5;
    double mix = 0.3;

    void prepare(double sampleRate);
    float process(float input);
    void reset();

private:
    struct CombFilter
    {
        std::vector<float> buffer;
        int pos = 0;
        float filterStore = 0.0f;

        void init(int size) { buffer.assign(size, 0.0f); pos = 0; filterStore = 0.0f; }
        float process(float input, float fb, float damp);
    };

    struct AllpassFilter
    {
        std::vector<float> buffer;
        int pos = 0;

        void init(int size) { buffer.assign(size, 0.0f); pos = 0; }
        float process(float input);
    };

    CombFilter combs[4];
    AllpassFilter allpasses[2];
    bool initialized = false;
};
