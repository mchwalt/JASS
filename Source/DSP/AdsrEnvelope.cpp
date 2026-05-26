#include "AdsrEnvelope.h"

void AdsrEnvelope::gateOn()
{
    stage = Stage::Attack;
}

void AdsrEnvelope::gateOff()
{
    if (stage == Stage::Idle) return;
    releaseStartLevel = level;
    stage = Stage::Release;
}

float AdsrEnvelope::process()
{
    double increment;

    switch (stage)
    {
        case Stage::Attack:
            increment = 1.0 / (std::max(attack, 0.001) * sampleRate);
            level += increment;
            if (level >= 1.0) { level = 1.0; stage = Stage::Decay; }
            break;

        case Stage::Decay:
            increment = (1.0 - sustain) / (std::max(decay, 0.001) * sampleRate);
            level -= increment;
            if (level <= sustain) { level = sustain; stage = Stage::Sustain; }
            break;

        case Stage::Sustain:
            level = sustain;
            break;

        case Stage::Release:
            increment = releaseStartLevel / (std::max(release, 0.001) * sampleRate);
            level -= increment;
            if (level <= 0.0) { level = 0.0; stage = Stage::Idle; }
            break;

        case Stage::Idle:
            level = 0.0;
            break;
    }

    return static_cast<float>(level);
}

void AdsrEnvelope::reset()
{
    level = 0.0;
    stage = Stage::Idle;
}
