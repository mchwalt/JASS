#pragma once
#include <cmath>
#include <atomic>
#include <cstdint>
#include "ModTargets.h"   // LFOTarget + the modulation-target vocabulary (single source of truth)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Append-only: the WAVE combo (LfoSpecs.h) and the legacy kLfoWave list (PresetIO.h) map by index.
enum class LFOWaveform { Sine, Triangle, Square, Sawtooth, SampleHold, OneShot };
// LFOTarget + the full target vocabulary live in ModTargets.h (single source of truth) — add a
// target there and the enum, persist strings, DEST labels and enable-map all update together.

// Number of LFOs, indexed like the oscillators (lfoOn(i), lfoRate(i) …). Grow HERE:
// bumping this + appending a ModSource + the source-index map is all a new LFO needs.
inline constexpr int kNumLFOs = 4;

class LFO
{
public:
    LFO()
    {
        // Per-instance RNG seed (construction happens on the message thread, not in the render
        // path): note-on resets every voice LFO, so identical seeds would make all voices step
        // the same "random" S&H sequence in lockstep.
        static std::atomic<std::uint64_t> instanceCounter { 0 };
        rngState = 0x853c49e6748fea9bULL + instanceCounter.fetch_add (1) * 0x9E3779B97F4A7C15ULL;
        if (rngState == 0) rngState = 0x853c49e6748fea9bULL;   // xorshift must never hold 0
        heldValue = drawRandom();
    }

    void setRate(double r) { rate = r; }
    void setDepth(double d) { depth = d; }
    void setWaveform(LFOWaveform w) { waveform = w; }
    void setTarget(LFOTarget t) { target = t; }
    void setSampleRate(double sr) { sampleRate = sr; }

    double getRate() const { return rate; }
    double getDepth() const { return depth; }
    LFOWaveform getWaveform() const { return waveform; }
    LFOTarget getTarget() const { return target; }

    // Returns bipolar value -1..+1
    float process()
    {
        if (target == LFOTarget::Off)
            return 0.0f;

        double value = 0.0;

        switch (waveform)
        {
            case LFOWaveform::Sine:
                value = std::sin(2.0 * M_PI * phase);
                break;
            case LFOWaveform::Triangle:
                value = phase < 0.5
                    ? 4.0 * phase - 1.0
                    : 3.0 - 4.0 * phase;
                break;
            case LFOWaveform::Square:
                value = phase < 0.5 ? 1.0 : -1.0;
                break;
            case LFOWaveform::Sawtooth:
                value = 2.0 * phase - 1.0;
                break;
            case LFOWaveform::SampleHold:
                value = heldValue;   // stepped: holds until the phase wraps (below)
                break;
            case LFOWaveform::OneShot:
                value = 1.0 - phase;   // unipolar single sweep 1 -> 0, envelope-like per note
                break;
        }

        if (waveform == LFOWaveform::OneShot)
        {
            // One cycle per note, then hold at 0: clamp instead of wrap. reset() (note-on)
            // rearms the sweep — RATE is therefore the sweep speed, not a repeat rate.
            phase += rate / sampleRate;
            if (phase > 1.0) phase = 1.0;
            if (phase < 0.0) phase = 0.0;
        }
        else
        {
            phase += rate / sampleRate;
            const bool wrapped = phase >= 1.0 || phase < 0.0;
            if (phase >= 1.0) phase -= 1.0;
            if (phase < 0.0)  phase += 1.0;   // wrap for a negative rate too (mirrors the oscillator)
            if (wrapped && waveform == LFOWaveform::SampleHold)
                heldValue = drawRandom();     // a wrap is a step boundary: draw the next level
        }

        return static_cast<float>(value * depth);
    }

    // Note-on retrigger: restart the cycle; S&H holds a fresh level immediately. The RNG stream
    // continues across notes (no reseed), so consecutive notes step differently.
    void reset() { phase = 0.0; heldValue = drawRandom(); }

private:
    // Fast xorshift PRNG -> [-1, 1) — same inline pattern as NoiseGenerator/KarplusStrong
    // (deliberately duplicated there too; extracting a shared RNG header is not worth the churn).
    double drawRandom()
    {
        rngState ^= rngState << 13;
        rngState ^= rngState >> 7;
        rngState ^= rngState << 17;
        double u = (rngState >> 11) * (1.0 / 9007199254740992.0); // 2^53
        return u * 2.0 - 1.0;
    }

    double phase = 0.0;
    double rate = 2.0;
    double depth = 0.5;
    double sampleRate = 44100.0;
    double heldValue = 0.0;                    // S&H: level held between phase wraps
    std::uint64_t rngState = 0x853c49e6748fea9bULL;
    LFOWaveform waveform = LFOWaveform::Sine;
    LFOTarget target = LFOTarget::Off;
};
