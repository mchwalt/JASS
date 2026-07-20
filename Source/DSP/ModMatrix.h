#pragma once
#include "LFO.h"          // targets ARE LFOTarget values (0 = Off .. 7); reused as the vocabulary
#include <array>
#include <algorithm>

// ── Modulation Matrix (Story 8.1 / Epic 8) ──────────────────────────────────
// Decouples modulation SOURCES from TARGETS. Old modulation was hard-wired: one LFO
// drove exactly one target and REPLACED the value. Here a routing is {source, target,
// amount} and multiple routings STACK on the same target — their offsets SUM before a
// single per-target apply in SynthVoice. This is what makes macros / per-voice random /
// an evolution module / extra LFOs cheap follow-ons: each is just "another source".
//
// Both enums are APPEND-ONLY: their integer indices are persisted in the .synthy preset
// and in DAW state, so new entries go at the END and existing ones never move.

enum class ModSource { LFO1, Envelope, Velocity, LFO2, LFO3, LFO4 };   // append-only (Macros, Voice-Random … later)

// One routing slot as read per block from the APVTS params into each voice.
// target is an LFOTarget index (0 = Off = slot inactive); source is a ModSource index.
// Plain ints/floats only — filled per block, read per sample on the audio thread (RT-safe).
struct ModSlot { int source = 0; int target = 0; float amount = 0.0f; };

namespace ModMatrixConfig
{
    inline constexpr int kNumSlots   = 6;   // fixed routing slots (append-only; grow here)
    inline constexpr int kNumSources = 6;   // ModSource count (LFO1, Envelope, Velocity, LFO2, LFO3, LFO4)
    inline constexpr int kNumTargets = 16;  // == LFOTarget count, including Off at index 0
}

// Sum every ACTIVE explicit slot's contribution into offsetOut[target] (indexed by LFOTarget:
// 0 = Off/unused). `sourceVals` is indexed by ModSource. Each LFO keeps its own built-in TARGET
// as an IMPLICIT routing (amount 1) — those are added by the caller (SynthVoice) AFTER this
// call, so the default patch stays byte-identical (AC5). Pure combine: no allocation, no state,
// no application of curves (SynthVoice owns the 2^x / clamp apply where the base values live).
inline void modMatrixAccumulate (const ModSlot* slots, bool matrixOn,
                                 const std::array<float, ModMatrixConfig::kNumSources>& sourceVals,
                                 std::array<double, ModMatrixConfig::kNumTargets>& offsetOut) noexcept
{
    offsetOut.fill (0.0);

    if (matrixOn)
        for (int s = 0; s < ModMatrixConfig::kNumSlots; ++s)
        {
            const ModSlot& sl = slots[s];
            if (sl.target > 0 && sl.target < ModMatrixConfig::kNumTargets && sl.amount != 0.0f)
            {
                const int src = std::clamp (sl.source, 0, ModMatrixConfig::kNumSources - 1);
                offsetOut[(size_t) sl.target] += (double) sl.amount * (double) sourceVals[(size_t) src];
            }
        }
}
