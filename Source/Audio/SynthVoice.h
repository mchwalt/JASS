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
    DelayEffect& getDelay() { return delay; }
    ChorusEffect& getChorus() { return chorus; }
    ReverbEffect& getReverb() { return reverb; }
    LFO& getLFO() { return lfo; }
    NoiseGenerator& getNoise() { return noise; }
    KarplusStrong& getKarplus() { return karplus; }
    MixMode& getMixMode() { return mixMode; }
    WavetableOscillator& getWavetable() { return wavetable; }

private:
    Oscillator oscillators[3];
    AdsrEnvelope envelope;
    BiquadFilter filter;
    DistortionEffect distortion;
    WavefolderEffect wavefolder;
    DelayEffect delay;
    ChorusEffect chorus;
    ReverbEffect reverb;
    LFO lfo;
    NoiseGenerator noise;
    KarplusStrong karplus;
    WavetableOscillator wavetable;

    MixMode mixMode = MixMode::Additive;

    // Store base values for LFO modulation
    double baseFrequencies[3] = {};
    double baseCutoff = 5000.0;

    double currentSampleRate = 44100.0;
    bool noteOn = false;
};
