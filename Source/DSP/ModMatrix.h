#pragma once
#include "LFO.h"          // targets ARE LFOTarget values (0 = Off .. 7); reused as the vocabulary
#include <array>
#include <algorithm>
#include <cmath>          // quantizeSemis: floor/round/abs

// ── Modulation Matrix (Story 8.1 / Epic 8) ──────────────────────────────────
// Decouples modulation SOURCES from TARGETS. Old modulation was hard-wired: one LFO
// drove exactly one target and REPLACED the value. Here a routing is {source, target,
// amount} and multiple routings STACK on the same target — their offsets SUM before a
// single per-target apply in SynthVoice. This is what makes macros / per-voice random /
// an evolution module / extra LFOs cheap follow-ons: each is just "another source".
//
// Both enums are APPEND-ONLY: their integer indices are persisted in the .synthy preset
// and in DAW state, so new entries go at the END and existing ones never move.

enum class ModSource { LFO1, Envelope, Velocity, LFO2, LFO3, LFO4, ChaosX, ChaosY };   // append-only (Macros, Voice-Random … later)

// One routing slot as read per block from the APVTS params into each voice.
// target is an LFOTarget index (0 = Off = slot inactive); source is a ModSource index.
// oscIndex selects a single oscillator (0..2) for the per-oscillator targets
// (FREQ/AMP/DETUNE); -1 = global / not osc-scoped ("Alle OSC" and every non-OSC target).
// quant is the per-slot QUANT choice (0 = Off, then Chrom/Major/Minor/Penta) — only pitch
// (Frequency) routings read it. Plain ints/floats only — filled per block, read per sample
// on the audio thread (RT-safe).
struct ModSlot { int source = 0; int target = 0; int oscIndex = -1; float amount = 0.0f; int quant = 0; };

namespace ModMatrixConfig
{
    inline constexpr int kNumSlots   = 8;   // fixed routing slots (append-only; grow here)
    inline constexpr int kNumSources = 8;   // ModSource count (LFO1, Envelope, Velocity, LFO2..4, ChaosX, ChaosY)
    inline constexpr int kNumTargets = ModTargets::kCount;   // single source: ModTargets.h (incl. Off at 0)
}

// Sum every ACTIVE explicit slot's contribution into offsetOut[target] (indexed by LFOTarget:
// 0 = Off/unused). `sourceVals` is indexed by ModSource. Each LFO keeps its own built-in TARGET
// as an IMPLICIT routing (amount 1) — those are added by the caller (SynthVoice) AFTER this
// call, so the default patch stays byte-identical (AC5). Pure combine: no allocation, no state,
// no application of curves (SynthVoice owns the 2^x / clamp apply where the base values live).
// Per-oscillator modulation offsets (FREQ/AMP/DETUNE) for OSC 1..3, filled alongside the global
// offsets. A slot whose oscIndex is 0..2 lands here (only that oscillator moves); "Alle OSC" and
// every non-OSC target keep going into offsetOut. Pitch/detune are harmless at 0 (2^0 / +0), so
// only the caller-computed amplitude "active" flags decide whether the AMP curve is applied.
struct OscModOffsets
{
    double pitch[3]    { 0.0, 0.0, 0.0 };   // additive octave offset (2^x applied in the voice)
    double amp[3]      { 0.0, 0.0, 0.0 };   // amplitude mod value ((1+v)*0.5 applied in the voice)
    double detune[3]   { 0.0, 0.0, 0.0 };   // detune offset (scaled + clamped in the voice)
    double feedback[3] { 0.0, 0.0, 0.0 };   // self-FM feedback offset (scaled + clamped in the voice)
    double voices[3]   { 0.0, 0.0, 0.0 };   // unison voice-count offset (scaled + clamped in the voice)
    double pan[3]      { 0.0, 0.0, 0.0 };   // stereo-pan offset (Epic 10; applied per OSC in Stereo-Pan mode)
    void clear() noexcept { for (int i = 0; i < 3; ++i) { pitch[i] = amp[i] = detune[i] = feedback[i] = voices[i] = pan[i] = 0.0; } }
};

// QUANT scale masks: snap a semitone value to the nearest degree of the chosen scale.
// quant: 1 = Chromatic (nearest semitone), 2 = Major, 3 = Minor, 4 = Pentatonic (minor).
// Pure and allocation-free; negative values decompose correctly via floor. The octave-wrapped
// first degree competes too, so 11.6 semitones snaps to 12, not down to 11.
inline double quantizeSemis (double semis, int quant) noexcept
{
    if (quant == 1)
        return std::round (semis);
    static constexpr int kMajor[] = { 0, 2, 4, 5, 7, 9, 11 };
    static constexpr int kMinor[] = { 0, 2, 3, 5, 7, 8, 10 };
    static constexpr int kPenta[] = { 0, 3, 5, 7, 10 };
    const int* deg = kMajor; int n = 7;
    if (quant == 3)      { deg = kMinor; n = 7; }
    else if (quant == 4) { deg = kPenta; n = 5; }
    const double oct = std::floor (semis / 12.0);
    const double r   = semis - oct * 12.0;   // 0..12
    double best = (double) deg[0], bestDist = std::abs (r - best);
    for (int i = 1; i < n; ++i)
        if (const double d = std::abs (r - (double) deg[i]); d < bestDist) { best = (double) deg[i]; bestDist = d; }
    if (std::abs (r - (12.0 + (double) deg[0])) < bestDist) best = 12.0 + (double) deg[0];
    return oct * 12.0 + best;
}

inline void modMatrixAccumulate (const ModSlot* slots, bool matrixOn,
                                 const std::array<float, ModMatrixConfig::kNumSources>& sourceVals,
                                 std::array<double, ModMatrixConfig::kNumTargets>& offsetOut,
                                 OscModOffsets& oscOut) noexcept
{
    offsetOut.fill (0.0);
    oscOut.clear();

    if (matrixOn)
        for (int s = 0; s < ModMatrixConfig::kNumSlots; ++s)
        {
            const ModSlot& sl = slots[s];
            if (sl.target > 0 && sl.target < ModMatrixConfig::kNumTargets && sl.amount != 0.0f)
            {
                const int src = std::clamp (sl.source, 0, ModMatrixConfig::kNumSources - 1);
                double v = (double) sl.amount * (double) sourceVals[(size_t) src];

                // QUANT: pitch routings snap to scale degrees — a stepped source (S&H, Chaos)
                // becomes a melody instead of continuous detune. FREQ contributions are in
                // OCTAVES (AMT 1 = 1 octave), so semitone space is simply v*12. Snapped HERE,
                // per slot BEFORE the sum: two stacked quantized slots stay honest, and an
                // un-quantized vibrato slot on the same target keeps gliding untouched.
                if (sl.quant > 0 && sl.target == (int) LFOTarget::Frequency)
                    v = quantizeSemis (v * 12.0, sl.quant) / 12.0;

                if (sl.oscIndex >= 0 && sl.oscIndex < 3)   // per-oscillator: only this OSC moves
                {
                    switch ((LFOTarget) sl.target)
                    {
                        case LFOTarget::Frequency:   oscOut.pitch[sl.oscIndex]    += v; break;
                        case LFOTarget::Amplitude:   oscOut.amp[sl.oscIndex]      += v; break;
                        case LFOTarget::OscDetune:   oscOut.detune[sl.oscIndex]   += v; break;
                        case LFOTarget::OscFeedback: oscOut.feedback[sl.oscIndex] += v; break;
                        case LFOTarget::OscVoices:   oscOut.voices[sl.oscIndex]   += v; break;
                        case LFOTarget::OscPan:      oscOut.pan[sl.oscIndex]      += v; break;
                        default:                     offsetOut[(size_t) sl.target] += v; break;   // defensive
                    }
                }
                else
                    offsetOut[(size_t) sl.target] += v;   // global (incl. "Alle OSC")
            }
        }
}
