#pragma once
#include <JuceHeader.h>
#include "SynthVoice.h"
#include "SynthSound.h"
#include "Parameters.h"

// One patch's sound engine (Story 21.4 = S0 of AD-16): everything per-patch that used to live in
// SynthyProcessor — the juce::Synthesiser with its 8 voices, poly/mono glide, the GLOBAL / display
// LFOs (20.1), the SAMPLER loop clock (12.1), the block-level LFO rates and delay time, and the
// per-voice parameter push. The processor keeps the instrument: MIDI pre-processing (drone, ARP,
// STEP SEQ, BASS routing, host anchors), PERC, BASS, CHAOS, compressor, master, stereo stages.
//
// S0 has ONE engine (layer A, today's parameter ids). AD-16 S1 adds a second with prefix "b".
// The processor calls the block methods in the order the code ran when it lived in processBlock;
// the cut was verified bit-exact with tools/rig/nulltest.py.
class PatchEngine
{
public:
    // What the instrument hands the patch for one block.
    struct BlockContext
    {
        int    numSamples   = 0;
        double sampleRate   = 44100.0;
        double syncBpm      = 120.0;       // resolved tempo (host, MASTER knob, MASTER · TEMPO mod)
        float  chaosX       = 0.0f;        // the one global attractor's snapshot (0 when CHAOS is off)
        float  chaosY       = 0.0f;
        float  accentDepth  = 0.0f;        // STEP SEQ ACCENT (15.2)
        bool   hosted       = false;
        bool   hostPlaying  = true;
        juce::Optional<double> hostPpq;
    };

    PatchEngine (juce::AudioProcessorValueTreeState& apvts, const juce::String& idPrefix);

    void prepare (double sampleRate, int samplesPerBlock);

    const Parameters::PatchParams& params() const noexcept { return p; }

    // ── block, in call order ─────────────────────────────────────────────────────────────────
    void killAllVoices()                     { synth.allNotesOff (0, false); }
    // LFO rates, delay time, SAMPLER loop clock, and the parameter push into the voices.
    void beginBlock (const BlockContext& ctx);
    // STEP SEQ legato/slide: move the voice holding `note` without retriggering it.
    void slideVoice (int note, double semitones, double seconds);
    void setPluckEnabled (bool enabled);      // the drone must not pluck the Karplus string
    void pluck();                             // PLUCK button / spacebar
    // Poly-glide sources + mono-glide note-off rebuild, on the FINAL midi buffer (after the ARP).
    void processGlide (juce::MidiBuffer& midi, int droneChannel);
    void render (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi)
    {
        synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());
    }
    // Advance the display / GLOBAL LFOs (20.1) — after the render, as before.
    void endBlock (const BlockContext& ctx);

    // ── editor / instrument reads ───────────────────────────────────────────────────────────
    float getLfoDisplayValue (int i) const   { return lfoDisplayValues[i].load(); }
    void  requestGlobalLfoRearm() noexcept   { rearmGlobalLfos.store (true); }

private:
    juce::AudioProcessorValueTreeState& apvts;
    Parameters::PatchParams p;

    // Construction order matters for the null test: every LFO draws its random seed from a
    // process-wide counter (LFO.h), and these display LFOs were built before the voices' LFOs
    // when they lived in the processor. Keep uiLfos above the voices being created (constructor).
    LFO uiLfos[kNumLFOs];      // display-only LFOs mirroring each patch LFO (for the rings)
    std::atomic<float> lfoDisplayValues[kNumLFOs] {};
    // GLOBAL LFOs (Story 20.1): uiLfos double as the ONE instance of a global LFO — what the rings
    // show is what every voice hears. Rising edge of (on && GLOBAL) rearms it, so a global One-Shot
    // starts when the LFO is switched on; a preset or DAW-state load rearms as well.
    bool lfoGlobalWasOn[kNumLFOs] {};            // audio thread only
    std::atomic<bool> rearmGlobalLfos { false };

    juce::Synthesiser synth;
    // Flat list of the synth's voices, built once in the constructor and handed to every voice so a
    // note-on can reach its siblings (Story 12.7 choke groups). Non-owning — the synthesiser owns
    // them — and never mutated afterwards, so reading it on the audio thread needs no lock.
    std::vector<SynthVoice*> voiceRoster;

    double samplerMasterFrac = 0.0;   // Story 12.1: shared SAMPLER loop phase (see beginBlock)

    // Block-level values resolved in beginBlock, read again in endBlock.
    double lfoRateHz[kNumLFOs] {};
    bool   lfoGlobalActive[kNumLFOs] {};   // 20.1: on && GLOBAL => the engine's instance is the LFO

    // Poly-glide (portamento): each newly-started note gets a predecessor pitch to glide FROM
    // (pitch-sorted against the previous chord), shared with all voices via glideInfo. Held-note
    // tracking excludes the auto-play drone.
    GlideInfo glideInfo;
    std::vector<int> glideHeld;        // notes currently held down
    std::vector<int> glideLastChord;   // last non-empty held set (glide source after a gap)
    std::vector<int> glideNewNotes, glideOffNotes;   // per-block scratch (no RT realloc)
    juce::MidiBuffer glideRebuiltScratch;            // mono-glide rebuild (no per-block alloc)
    int monoSounding = -1;             // Mono glide: the note currently sounding (last-note priority)

    JUCE_DECLARE_NON_COPYABLE (PatchEngine)
};
