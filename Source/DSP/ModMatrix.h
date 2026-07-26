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
// oscIndex selects a single oscillator (0..2) for the per-oscillator targets
// (FREQ/AMP/DETUNE); -1 = global / not osc-scoped ("Alle OSC" and every non-OSC target).
// Plain ints/floats only — filled per block, read per sample on the audio thread (RT-safe).
struct ModSlot { int source = 0; int target = 0; int oscIndex = -1; float amount = 0.0f; };

namespace ModMatrixConfig
{
    inline constexpr int kNumSlots   = 8;   // fixed routing slots (append-only; grow here)
    inline constexpr int kNumSources = 6;   // ModSource count (LFO1, Envelope, Velocity, LFO2, LFO3, LFO4)
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
                const double v = (double) sl.amount * (double) sourceVals[(size_t) src];

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
