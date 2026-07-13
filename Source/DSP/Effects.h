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

// West-Coast / Buchla-style sine wavefolder. Unlike the Foldback distortion
// (which reflects a triangle off ±1), this drives the signal through a smooth
// sine so it folds back on itself repeatedly, producing rich, evolving
// harmonics. Stateless (no DC offset: the transfer function passes through 0).
class WavefolderEffect
{
public:
    bool enabled = false;
    double drive = 0.3;     // 0..1 (mapped to fold gain 1x..8x)
    double symmetry = 0.0;  // -1..1 asymmetry → adds even harmonics
    double mix = 1.0;       // dry/wet 0..1

    float process(float input);
};

// Lo-Fi bitcrusher: reduces bit depth (quantisation → gritty/digital) and the
// effective sample rate (sample & hold → aliasing/crunch). Stateful.
class BitcrusherEffect
{
public:
    bool enabled = false;
    double bits = 8.0;    // 1..16 bit depth
    double rate = 1.0;    // sample-rate reduction factor (1 = none, hold N samples)
    double mix = 1.0;     // dry/wet 0..1

    float process(float input);

private:
    float held = 0.0f;
    int counter = 0;
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

// Phaser (LFO-swept all-pass chain → moving notches) and Flanger (LFO-swept short
// comb → jet/whoosh) in one switchable module. Both use a shared internal LFO and a
// feedback path. Stateful.
enum class PhaserType { Phaser, Flanger };

class PhaserEffect
{
public:
    bool enabled = false;
    PhaserType type = PhaserType::Phaser;
    double rate = 0.5;      // LFO speed in Hz
    double depth = 0.7;     // sweep depth 0..1
    double feedback = 0.5;  // resonance / intensity 0..0.95
    double mix = 0.5;       // dry/wet 0..1 (0.5 = classic phaser notch depth)

    void prepare(double sampleRate);
    float process(float input);
    void reset();

private:
    double sr = 44100.0;
    double lfoPhase = 0.0;

    // Phaser: chain of first-order all-pass sections, swept together.
    static constexpr int kStages = 4;
    float apState[kStages] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float lastPhaser = 0.0f;

    // Flanger: short modulated delay line (1..7 ms) with feedback.
    std::vector<float> buffer;
    int writePos = 0;
    float lastFlanger = 0.0f;
};

// Formant / vowel filter: three parallel band-pass "formants" whose centre
// frequencies morph across the vowels A-E-I-O-U → a "talking" timbre. VOWEL sweeps
// the morph, RESO sharpens the formants (Q). Coefficients are recomputed only when
// VOWEL/RESO change (per block via the dirty check), so process() stays cheap. Stateful.
class FormantFilter
{
public:
    bool enabled = false;
    double vowel = 0.0;       // 0..1 morph across A(0) E I O U(1)
    double resonance = 0.5;   // 0..1 → band-pass Q
    double mix = 1.0;         // dry/wet 0..1

    void prepare(double sampleRate);
    float process(float input);
    void reset();

private:
    struct BandPass
    {
        double b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;   // normalised RBJ band-pass coeffs
        double z1 = 0, z2 = 0;
        void set(double freq, double q, double sr);
        float process(float x);
        void reset() { z1 = z2 = 0; }
    };

    void updateCoeffs();

    double sr = 44100.0;
    BandPass bands[3];
    double lastVowel = -1.0, lastReso = -1.0;   // dirty markers (force first compute)
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
