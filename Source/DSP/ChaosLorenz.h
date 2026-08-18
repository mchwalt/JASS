#pragma once
#include <cmath>
#include <algorithm>

// ── Lorenz attractor as a modulation source (CHAOS module) ──────────────────
// Deterministic chaos instead of randomness: three coupled ODEs whose orbit never
// repeats but never degenerates into noise. ONE global instance lives in the
// processor (all voices move together — correlated, musical, cheap); voices get a
// block-rate snapshot of x/y through applyToVoice. Free-running by design: it is
// never reset — not on note-on, not on preset load — so the orbit keeps evolving.
//
// x and y are exposed as TWO matrix sources (Chaos X / Chaos Y): they orbit the
// same attractor, so they drift together but not alike — routing X→cutoff and
// Y→wavetable position moves as one intentional gesture, which two independent
// random sources cannot do.
class ChaosLorenz
{
public:
    void setSampleRate (double sr) { sampleRate = sr > 0.0 ? sr : 44100.0; }

    // RATE knob: orbit revolutions feel right around rate ≈ 1; the knob scales the
    // integration speed linearly.
    void setRate (double r) { rate = std::clamp (r, 0.0, 20.0); }

    // Advance the orbit by one audio block. Forward Euler diverges with large steps,
    // so the block is integrated in sub-steps with dt clamped to a stable region;
    // an isfinite guard re-seeds the orbit as belt and braces (a NaN here would
    // otherwise pour silence/garbage into every routed target).
    void advance (int numSamples) noexcept
    {
        const double span = rate * kTimeScale * (double) numSamples / sampleRate;
        const int    steps = std::max (1, (int) std::ceil (span / kMaxDt));
        const double dt    = span / (double) steps;

        for (int i = 0; i < steps; ++i)
        {
            const double dx = kSigma * (y - x);
            const double dy = x * (kRho - z) - y;
            const double dz = x * y - kBeta * z;
            x += dx * dt; y += dy * dt; z += dz * dt;
        }

        if (! (std::isfinite (x) && std::isfinite (y) && std::isfinite (z)))
            reseed();
    }

    // Normalized outputs ~[-1, 1] (the attractor wings span roughly ±20 / ±27).
    float outX() const noexcept { return (float) std::clamp (x / 20.0, -1.0, 1.0); }
    float outY() const noexcept { return (float) std::clamp (y / 27.0, -1.0, 1.0); }

private:
    void reseed() noexcept { x = 0.1; y = 0.0; z = 0.0; }

    // Classic Lorenz parameters — the values the attractor is famous for.
    static constexpr double kSigma = 10.0, kRho = 28.0, kBeta = 8.0 / 3.0;
    static constexpr double kMaxDt = 0.005;   // Euler stability ceiling per sub-step
    static constexpr double kTimeScale = 8.0; // rate 1.0 → audibly wandering, not static

    double x = 0.1, y = 0.0, z = 0.0;         // deterministic seed, never re-randomized
    double rate = 1.0;
    double sampleRate = 44100.0;
};
