#pragma once
#include <algorithm>

// 4-point Hermite (Catmull-Rom) read at a fractional position, edge-clamped. The SAMPLER's
// interpolator since Story 12.1 (measured 38.3 dB vs 28.5 dB linear); lifted out of SamplePlayer
// so GRAIN (Story 17.1) reads the same material with the same maths. JUCE-free on purpose — the
// scratch harness compiles it alone.
//
// Edge-clamped means: it never reads outside [0, n-1], and a position past the end returns the
// last sample held — safe, but a *held* sample is DC, so a caller that windows short reads (GRAIN)
// must keep its read span inside the buffer for the sound's sake, not for safety.
inline float hermiteRead (const float* d, int n, double p) noexcept
{
    if (d == nullptr || n <= 0) return 0.0f;   // shared header: keep the "never outside" promise for every caller
    p = std::clamp (p, 0.0, (double) (n - 1));
    const int i = (int) p;
    const float f = (float) (p - i);
    const float xm1 = d[std::max (i - 1, 0)];
    const float x0  = d[i];
    const float x1  = d[std::min (i + 1, n - 1)];
    const float x2  = d[std::min (i + 2, n - 1)];
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * f + c2) * f + c1) * f + x0;
}
