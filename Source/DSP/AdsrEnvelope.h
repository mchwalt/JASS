#pragma once
#include <algorithm>

class AdsrEnvelope
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void setAttack(double a) { attack = a; }
    void setDecay(double d) { decay = d; }
    void setSustain(double s) { sustain = s; }
    void setRelease(double r) { release = r; }
    void setSampleRate(double sr) { sampleRate = sr; }

    void gateOn();
    void gateOff();
    float process();
    void reset();

    Stage getStage() const { return stage; }

private:
    double attack = 0.5;
    double decay = 0.3;
    double sustain = 0.7;
    double release = 1.0;
    double sampleRate = 44100.0;
    double level = 0.0;
    double releaseStartLevel = 0.0;
    Stage stage = Stage::Idle;
};
