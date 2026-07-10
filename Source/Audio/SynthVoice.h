#pragma once
#include <JuceHeader.h>
#include "../DSP/Oscillator.h"
#include "../DSP/AdsrEnvelope.h"
#include "../DSP/BiquadFilter.h"
#include "../DSP/Effects.h"
#include "../DSP/LFO.h"
#include "../DSP/NoiseGenerator.h"
#include "../DSP/KarplusStrong.h"
#include "../DSP/WavetableOscillator.h"
#include "SynthSound.h"

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
    DelayEffect& getDelay() { return delay; }
    ChorusEffect& getChorus() { return chorus; }
    ReverbEffect& getReverb() { return reverb; }
    LFO& getLFO() { return lfo; }
    NoiseGenerator& getNoise() { return noise; }
    KarplusStrong& getKarplus() { return karplus; }
    MixMode& getMixMode() { return mixMode; }
    WavetableOscillator& getWavetable() { return wavetable; }
    Oscillator& getSubOsc() { return subOsc; }
    int& getSubOctaveRef() { return subOctave; }
    bool& getAdsrOnRef() { return adsrOn; }
    bool& getMixModeOnRef() { return mixModeOn; }
    int& getMixSrcARef() { return mixSrcA; }   // Epic 5: RingMod/FM operands
    int& getMixSrcBRef() { return mixSrcB; }

    // Re-pluck the Karplus string at the voice's current (transposed) pitch.
    void pluckKarplus();

    // When false, startNote does NOT pluck the string (used so the auto-play
    // drone doesn't auto-pluck STRING — it's played via the keyboard).
    void setPluckEnabled(bool b) { pluckEnabled = b; }

private:
    Oscillator oscillators[3];
    Oscillator subOsc;            // sub-oscillator: tracks OSC1 pitch, octave(s) down
    AdsrEnvelope envelope;
    BiquadFilter filter;
    DistortionEffect distortion;
    WavefolderEffect wavefolder;
    BitcrusherEffect bitcrusher;
    DelayEffect delay;
    ChorusEffect chorus;
    ReverbEffect reverb;
    LFO lfo;
    NoiseGenerator noise;
    KarplusStrong karplus;
    WavetableOscillator wavetable;

    MixMode mixMode = MixMode::Additive;
    bool adsrOn = true;      // false => envelope bypassed (constant gain) — Story 2.4
    bool mixModeOn = true;   // false => oscillators summed additively regardless of mixMode
    int mixSrcA = 0;         // Epic 5: RingMod/FM operand A (0..2 => OSC 1/2/3); default OSC1
    int mixSrcB = 1;         // operand B; default OSC2 (== prior fixed OSC1<->OSC2 coupling)

    // Store base values for LFO modulation
    double baseFrequencies[3] = {};
    double baseCutoff = 5000.0;

    // Pitch transposition: played note relative to C4 (note 60). 1.0 = no shift.
    double transposeRatio = 1.0;
    int subOctave = -1;           // sub-oscillator octave offset (-1 or -2)

    double currentSampleRate = 44100.0;
    bool noteOn = false;
    bool pluckEnabled = true;   // false → startNote skips the Karplus pluck (drone)
};
