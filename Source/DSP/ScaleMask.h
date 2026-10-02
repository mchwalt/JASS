#pragma once
#include <cmath>

// ─── QUANT scale masks ──────────────────────────────────────────────────────────────────────────
// One table for everybody who snaps to a scale: the MOD MATRIX (quantizeSemis in ModMatrix.h,
// unchanged behaviour) and GRAIN (Story 17.1), which draws whole scale *degrees* so that every
// note of the cloud is equally likely — rounding a continuous value onto Penta/Minor would weight
// the degrees next to a 3-semitone gap about twice as often as their neighbours.
//
// quant: 0 = Off (caller decides), 1 = Chromatic, 2 = Major, 3 = Minor, 4 = Pentatonic (minor).
// Everything here is pure, allocation-free and JUCE-free (the scratch harness compiles it alone).
namespace ScaleMask
{
    inline constexpr int kChromatic[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
    inline constexpr int kMajor[]     = { 0, 2, 4, 5, 7, 9, 11 };
    inline constexpr int kMinor[]     = { 0, 2, 3, 5, 7, 8, 10 };
    inline constexpr int kPenta[]     = { 0, 3, 5, 7, 10 };

    struct Scale { const int* deg; int n; };

    inline constexpr Scale scale (int quant) noexcept
    {
        switch (quant)
        {
            case 2:  return { kMajor, 7 };
            case 3:  return { kMinor, 7 };
            case 4:  return { kPenta, 5 };
            default: return { kChromatic, 12 };   // 1 and anything unknown: every semitone
        }
    }

    // A "step" counts scale degrees across octaves: step 0 = the root, step n = the root one octave
    // up, step -1 = the top degree of the octave below. Semitones of a step:
    inline double stepToSemis (int step, int quant) noexcept
    {
        const Scale s = scale (quant);
        const int oct = (step >= 0) ? step / s.n : -((-step + s.n - 1) / s.n);   // floor division
        const int i   = step - oct * s.n;
        return 12.0 * oct + (double) s.deg[i];
    }

    // The step whose semitone value is nearest to `semis`. Ties go DOWN (strict `<` keeps the lower
    // degree) — the same rule as quantizeSemis in ModMatrix.h, whose octave-wrapped root competes
    // too so 11.6 → 12 rather than 11. Keep the two in step until the deferred merge.
    inline int nearestStep (double semis, int quant) noexcept
    {
        const Scale s = scale (quant);
        const double oct = std::floor (semis / 12.0);
        const double r   = semis - oct * 12.0;   // 0..12
        int best = 0; double bestDist = std::abs (r - (double) s.deg[0]);
        for (int i = 1; i < s.n; ++i)
            if (const double d = std::abs (r - (double) s.deg[i]); d < bestDist) { best = i; bestDist = d; }
        if (std::abs (r - 12.0) < bestDist) best = s.n;   // wrapped root of the next octave
        return (int) oct * s.n + best;
    }

    // How many steps up / down from `centerStep` stay within `spreadSemis` of the centre. The two
    // directions differ on uneven scales (Penta: 3 semitones up from the root, 2 down), so a
    // symmetric draw must ask both. Bounded: a 24-semitone spread is at most two octaves.
    inline int stepsUpWithin (int centerStep, double spreadSemis, int quant) noexcept
    {
        const double c = stepToSemis (centerStep, quant);
        int k = 0;
        while (k < 48 && stepToSemis (centerStep + k + 1, quant) - c <= spreadSemis + 1e-9)
            ++k;
        return k;
    }

    inline int stepsDownWithin (int centerStep, double spreadSemis, int quant) noexcept
    {
        const double c = stepToSemis (centerStep, quant);
        int k = 0;
        while (k < 48 && c - stepToSemis (centerStep - k - 1, quant) <= spreadSemis + 1e-9)
            ++k;
        return k;
    }
}
