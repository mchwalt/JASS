#pragma once
#include <JuceHeader.h>
#include <array>
#include "../DSP/Oscillator.h"
#include "../DSP/AdsrEnvelope.h"
#include "../DSP/PitchEnvelope.h"
#include "../DSP/BiquadFilter.h"
#include "../DSP/Effects.h"
#include "../DSP/LFO.h"
#include "../DSP/NoiseGenerator.h"
#include "../DSP/KarplusStrong.h"
#include "../DSP/WavetableOscillator.h"
#include "../DSP/SamplePlayer.h"   // Story 12.1: SAMPLER generator
#include "../DSP/ModMatrix.h"
#include "../DSP/ChannelStrip.h"   // Epic 10: ChannelStrip, kMaxOutChannels, OutputMode, positionToGains
#include "../DSP/BinauralPanner.h" // Epic 10 (10.3): parametric binaural per-generator renderer
#include "../DSP/HrtfPanner.h"     // Epic 10 (10.3): HRIR "Kunstkopf" per-generator renderer
#include "SynthSound.h"

// Poly-glide (portamento): the processor computes, per block, the transpose ratio each
// newly-started note should glide FROM (pitch-sorted assignment against the previous
// chord). Voices read it in startNote via a shared pointer. startRatio[note] < 0 => that
// note starts instantly (no predecessor / glide off).
struct GlideInfo
{
    bool   enabled = false;
    double timeSec = 0.0;
    std::array<float, 128> startRatio;   // transpose ratio to glide from; < 0 = instant
    GlideInfo() { startRatio.fill(-1.0f); }
};

class SynthVoice : public juce::SynthesiserVoice
{
public:
    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}
    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void renderNextBlock(juce::AudioBuffer<float>&, int startSample, int numSamples) override;

    Oscillator* getOscillators() { return oscillators; }
    AdsrEnvelope& getEnvelope() { return envelope; }
    BiquadFilter& getFilter() { return filter; }
    DistortionEffect& getDistortion() { return distortion; }
    WavefolderEffect& getWavefolder() { return wavefolder; }
    BitcrusherEffect& getBitcrusher() { return bitcrusher; }
    PhaserEffect& getPhaser() { return phaser; }
    DelayEffect& getDelay() { return delay; }
    ChorusEffect& getChorus() { return chorus; }
    ReverbEffect& getReverb() { return reverb; }
    FormantFilter& getFormant() { return formant; }
    LFO* getLFOs() { return lfos; }
    NoiseGenerator& getNoise() { return noise; }
    KarplusStrong& getKarplus() { return karplus; }
    MixMode& getMixMode() { return mixMode; }
    WavetableOscillator& getWavetable() { return wavetable; }
    SamplePlayer& getSampler() { return sampler; }   // Story 12.1

    // ── Story 12.7: choke groups act ACROSS voices ───────────────────────────────────────────
    // A voice knows only its own note (that is what keeps it RT-safe and simple, Story 11.1), so a
    // closed hi-hat cannot reach the open one without help. The narrowest help that works: the
    // processor hands every voice the same flat roster of its siblings — no synthesiser, no casts,
    // no ownership. Set once after the voices are built; nullptr means "no siblings", which is
    // simply the pre-12.7 behaviour.
    void setVoicePeers (const std::vector<SynthVoice*>* peers) noexcept { voicePeers = peers; }
    // Silence this voice's sampler if it is sounding a zone that belongs to `group`. Called by a
    // SIBLING from its startNote, so it runs on the audio thread: no allocation, no lock.
    void chokeSamplerInGroup (int group) noexcept
    {
        if (const auto* z = sampler.currentZone(); z != nullptr && group != 0 && z->group == group)
            sampler.chokeOff();
    }
    Oscillator& getSubOsc() { return subOsc; }
    int& getSubOctaveRef() { return subOctave; }
    bool& getAdsrOnRef() { return adsrOn; }
    bool& getMixModeOnRef() { return mixModeOn; }
    int& getMixSrcARef() { return mixSrcA; }   // Epic 5: RingMod/FM operands
    int& getMixSrcBRef() { return mixSrcB; }
    PitchEnvelope& getPitchEnv() { return pitchEnv; }
    double& getPitchEnvAmountRef() { return pitchEnvAmount; }
    bool& getPitchEnvOnRef() { return pitchEnvOn; }
    // Modulation matrix (Story 8.1): the processor fills these per block from the params.
    ModSlot* getModSlots() { return modSlots; }
    bool& getModMatrixOnRef() { return modMatrixOn; }
    // CHAOS (LFO expansion): block-rate snapshot of the ONE global Lorenz attractor — the
    // processor advances it and hands every voice the same X/Y pair (correlated by design).
    // Not reset on note-on: the source free-runs. Block-constant per sample is fine for a
    // slow chaotic source (~94 Hz update at 512-sample blocks).
    void setChaosValues (float x, float y) noexcept { chaosSrc[0] = x; chaosSrc[1] = y; }

    // ACCENT depth (15.2), pushed per block by the processor from STEP SEQ's knob: how much a
    // note's velocity above/below the sequencer's plain 100 moves the note's gain and cutoff.
    // 0 = velocity changes nothing — the pre-15.2 behaviour and the sound of every old preset.
    void setAccentDepth (float d) noexcept { accentDepth = d; }

    // Spatialization (Epic 10): the processor writes the output mode + the per-generator pans each
    // block (applyToVoice); the voice turns them into per-channel gains at the top of renderNextBlock.
    int& getOutputModeRef() { return outputMode; }
    float* getGeneratorPan() { return generatorPan; }   // [kNumPanGenerators], indexed by PanGen
    std::array<ChannelStrip, kMaxOutChannels>& getStrips() { return strips; }   // applyToVoice configures all

    // Re-pluck the Karplus string at the voice's current (transposed) pitch.
    void pluckKarplus();

    // When false, startNote does NOT pluck the string (used so the auto-play
    // drone doesn't auto-pluck STRING — it's played via the keyboard).
    void setPluckEnabled(bool b) { pluckEnabled = b; }

    // Poly-glide: shared read-only info filled by the processor each block (see GlideInfo).
    void setGlideInfo(const GlideInfo* g) { glideInfo = g; }

    // 15.7 (STEP SEQ TIE/SLIDE): retune the sounding note WITHOUT retriggering — the 303's
    // slide. The pitch target moves by `semitones`; seconds > 0 glides there through the same
    // smoothed ratio the GLIDE module uses, ~0 steps instantly (a tie taking over a new pitch).
    // The envelope is untouched: no new attack, which is the whole point.
    void slideTo(double semitones, double seconds)
    {
        transposeRatio *= std::pow(2.0, semitones / 12.0);
        if (seconds > 0.001)
        {
            const double now = glideRatio.getCurrentValue();
            glideRatio.reset(currentSampleRate, seconds);
            glideRatio.setCurrentAndTargetValue(now);
            glideRatio.setTargetValue(transposeRatio);
        }
        else
        {
            glideRatio.reset(currentSampleRate, 0.0);
            glideRatio.setCurrentAndTargetValue(transposeRatio);
        }
    }

private:
    Oscillator oscillators[3];
    Oscillator subOsc;            // sub-oscillator: tracks OSC1 pitch, octave(s) down
    AdsrEnvelope envelope;
    PitchEnvelope pitchEnv;   // one-shot pitch sweep (kicks/lasers/zaps)
    // Effect chain instanced per output channel (Epic 10). The old individual FX members are now
    // reference ALIASES onto strip 0, so all existing render/applyToVoice code (which uses `filter`,
    // `delay`, … directly) keeps working unchanged and operates on channel 0 — byte-identical while
    // only strip 0 is active (Mono / Pseudo-Stereo). Stereo-Pan iterates strips[0..nCh) directly.
    std::array<ChannelStrip, kMaxOutChannels> strips;
    BiquadFilter&     filter     = strips[0].filter;
    DistortionEffect& distortion = strips[0].distortion;
    WavefolderEffect& wavefolder = strips[0].wavefolder;
    BitcrusherEffect& bitcrusher = strips[0].bitcrusher;
    PhaserEffect&     phaser     = strips[0].phaser;
    DelayEffect&      delay      = strips[0].delay;
    ChorusEffect&     chorus     = strips[0].chorus;
    ReverbEffect&     reverb     = strips[0].reverb;
    FormantFilter&    formant    = strips[0].formant;   // vowel filter, applied right after the main filter
    LFO lfos[kNumLFOs];   // indexed LFOs (each: own module + target + a matrix source)
    NoiseGenerator noise;
    KarplusStrong karplus;
    WavetableOscillator wavetable;
    SamplePlayer sampler;   // Story 12.1: recordings as a generator (stereo via PanSamplerL/R)
    const std::vector<SynthVoice*>* voicePeers = nullptr;   // 12.7 (see setVoicePeers)

    MixMode mixMode = MixMode::RingMod;   // only meaningful when mixModeOn; off => additive
    bool adsrOn = true;      // false => envelope bypassed (constant gain) — Story 2.4
    bool mixModeOn = false;  // false => oscillators summed additively (the "Additive" default)
    int mixSrcA = 0;         // Epic 5: RingMod/FM operand A (0..2 => OSC 1/2/3); default OSC1
    int mixSrcB = 1;         // operand B; default OSC2 (== prior fixed OSC1<->OSC2 coupling)
    double pitchEnvAmount = 0.0;  // semitones at full envelope (bipolar); 0 => no pitch sweep
    bool   pitchEnvOn = false;    // false => pitch envelope bypassed

    // Spatialization (Epic 10). outputMode/generatorPan are filled per block by applyToVoice;
    // panGains is derived once per block at the top of renderNextBlock via positionToGains().
    int   outputMode = (int) OutputMode::PseudoStereo;
    float generatorPan[kNumPanGenerators] = {};                 // -1..1 per generator (PanGen order)
    float panGains[kNumPanGenerators][kMaxOutChannels] = {};    // per-generator per-channel gains
    BinauralPanner binaural[kNumPanGenerators];                 // 10.3: per-generator binaural renderer (Binaural mode)
    HrtfPanner     hrtf[kNumPanGenerators];                     // 10.3: per-generator HRIR renderer (Kunstkopf HRTF mode)

    // Modulation matrix (Story 8.1): N routing slots + master enable, filled per block by
    // the processor (Parameters::applyToVoice). noteVelocity is the Velocity source (0..1),
    // captured once at note-on and constant across the note.
    ModSlot modSlots[ModMatrixConfig::kNumSlots];
    bool    modMatrixOn = true;    // false => explicit slots ignored (implicit LFO routing still applies)
    float   noteVelocity = 1.0f;   // MIDI velocity 0..1 (Velocity mod source)
    float   chaosSrc[2] {};        // ChaosX/Y sources — global attractor snapshot (setChaosValues)
    float   accentDepth = 0.0f;    // 15.2: velocity → gain/cutoff amount (STEP SEQ's ACCENT knob)

    // Store base values for LFO modulation
    double baseFrequencies[3] = {};
    double baseCutoff = 5000.0;

    // Pitch transposition: played note relative to C4 (note 60). 1.0 = no shift.
    // transposeRatio is the TARGET; glideRatio is the smoothed value actually used for
    // pitch (equals the target instantly when glide is off / no predecessor).
    double transposeRatio = 1.0;
    const GlideInfo* glideInfo = nullptr;   // poly-glide info (owned by the processor)
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> glideRatio;
    int subOctave = -1;           // sub-oscillator octave offset (-1 or -2)

    // When the ENVELOPE module is OFF (adsrOn=false) the amplitude isn't ADSR-shaped — but the
    // note must still gate on/off cleanly and the voice must free PROMPTLY on release (not hang for
    // the envelope's release time at full gain). This short smoothed gate does that (anti-click).
    juce::SmoothedValue<float> bypassGate;   // 0..1, ~10 ms ramp; gain + voice-free path when ADSR off

    double currentSampleRate = 44100.0;
    bool noteOn = false;
    bool pluckEnabled = true;   // false → startNote skips the Karplus pluck (drone)
    // 12.4: with the ADSR module OFF, the bypass gate is held open while the SAMPLER plays its
    // own release fade (isRingingOut) — released when the fade is spent (see renderNextBlock).
    bool samplerTailHold = false;
};
