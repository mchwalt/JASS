#pragma once
#include <JuceHeader.h>   // juce::jlimit
#include <array>
#include <cmath>
#include "BiquadFilter.h"
#include "Effects.h"

// ── Spatialization (Epic 10) ─────────────────────────────────────────────────────────────────
// The per-voice signal path is channel-count agnostic. Each generator is mono; a PAN places it into
// an N-channel per-voice bus, and the post-generator effect chain is instanced PER channel
// (ChannelStrip) so each panned channel runs its own effects. In Mono / Pseudo-Stereo mode only
// channel 0 runs (mono, byte-identical to before). A later surround phase bumps kMaxOutChannels and
// adds positionToGains() branches — additive, no rewrite. The mono FX classes stay unchanged.

// Max output channels the per-voice mix fans out to. 2 for stereo pan (Story 10.1).
inline constexpr int kMaxOutChannels = 2;

// Output Mode (append-only; Surround/Binaural appended after StereoPan later). MUST match the
// "outputMode" choice order in StereoSpecs.h.
enum class OutputMode { Mono = 0, PseudoStereo = 1, StereoPan = 2, Binaural = 3, Kunstkopf = 4 };

// Generators that carry an independent PAN (OSC 1/2/3, SUB, NOISE, KARPLUS, WAVETABLE).
inline constexpr int kNumPanGenerators = 7;
enum PanGen { PanOsc1 = 0, PanOsc2, PanOsc3, PanSub, PanNoise, PanKarplus, PanWavetable };

// The post-generator effect chain, one set per output channel.
struct ChannelStrip
{
    WavefolderEffect wavefolder;
    BiquadFilter     filter;
    FormantFilter    formant;
    DistortionEffect distortion;
    BitcrusherEffect bitcrusher;
    PhaserEffect     phaser;
    ChorusEffect     chorus;
    DelayEffect      delay;
    ReverbEffect     reverb;
};

// Equal-power pan of a mono source at position pan ∈ [-1,+1] into `nCh` channels.
//  nCh <= 1 → mono, gain 1.0 (byte-identical to the pre-spatialization sum).
//  nCh == 2 → equal-power L/R: center = cos/sin at 45° = 0.707 each (−3 dB, acoustically matched).
//  nCh  > 2 → Phase-B surround extension point (currently degrades to stereo).
inline void positionToGains (float pan, int nCh, float* gains) noexcept
{
    if (nCh <= 1) { gains[0] = 1.0f; return; }
    const float p = juce::jlimit (-1.0f, 1.0f, pan);
    const float angle = (p * 0.5f + 0.5f) * 1.57079633f;   // 0 .. π/2
    gains[0] = std::cos (angle);   // L
    gains[1] = std::sin (angle);   // R
}
