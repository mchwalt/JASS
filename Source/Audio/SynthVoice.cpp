#include "SynthVoice.h"

bool SynthVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<SynthSound*>(sound) != nullptr;
}

void SynthVoice::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    for (auto& osc : oscillators)
        osc.setSampleRate(sampleRate);
    envelope.setSampleRate(sampleRate);
    filter.setSampleRate(sampleRate);
    lfo.setSampleRate(sampleRate);
    karplus.setSampleRate(sampleRate);
    wavetable.setSampleRate(sampleRate);
    delay.prepare(sampleRate);
    chorus.prepare(sampleRate);
    reverb.prepare(sampleRate);
}

void SynthVoice::startNote(int midiNoteNumber, float velocity,
                           juce::SynthesiserSound*, int)
{
    double freq = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    oscillators[0].setFrequency(freq);
    oscillators[0].setAmplitude(static_cast<double>(velocity));
    oscillators[0].reset();
    lfo.reset();
    noise.reset();
    karplus.pluck();
    wavetable.reset();

    envelope.gateOn();
    noteOn = true;
}

void SynthVoice::stopNote(float /*velocity*/, bool allowTailOff)
{
    if (allowTailOff)
        envelope.gateOff();
    else
    {
        envelope.reset();
        clearCurrentNote();
        noteOn = false;
    }
}

void SynthVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                                  int startSample, int numSamples)
{
    if (!noteOn && envelope.getStage() == AdsrEnvelope::Stage::Idle)
        return;

    // Store base values before LFO modulation
    for (int i = 0; i < 3; ++i)
        baseFrequencies[i] = oscillators[i].getFrequency();
    baseCutoff = filter.getCutoff();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // LFO modulation
        float lfoValue = lfo.process();
        auto lfoTarget = lfo.getTarget();

        if (lfoTarget == LFOTarget::Frequency)
        {
            // Modulate frequency by semitones (±12 semitones at full depth)
            double factor = std::pow(2.0, lfoValue * 12.0 / 12.0);
            for (int i = 0; i < 3; ++i)
                oscillators[i].setFrequency(baseFrequencies[i] * factor);
        }
        else if (lfoTarget == LFOTarget::FilterCutoff)
        {
            // Modulate cutoff logarithmically
            double factor = std::pow(2.0, lfoValue * 3.0); // ±3 octaves
            filter.setCutoff(std::clamp(baseCutoff * factor, 20.0, 10000.0));
        }

        // Mix oscillators according to the mix mode
        float mixedSample = 0.0f;
        if (mixMode == MixMode::FM)
        {
            // FM: OSC1 modulates OSC2's frequency, OSC3 additive
            float modulator = oscillators[0].nextSample();
            double fmOffset = modulator * oscillators[0].getFrequency() * 2.0;
            float carrier = oscillators[1].nextSample(fmOffset);
            float s2 = oscillators[2].nextSample();
            mixedSample = carrier + s2;
        }
        else if (mixMode == MixMode::RingMod)
        {
            // Ring: OSC1 × OSC2 + OSC3
            float s0 = oscillators[0].nextSample();
            float s1 = oscillators[1].nextSample();
            float s2 = oscillators[2].nextSample();
            mixedSample = s0 * s1 * 2.0f + s2;
        }
        else
        {
            for (auto& osc : oscillators)
                mixedSample += osc.nextSample();
        }

        // Add noise generator + Karplus-Strong string + wavetable oscillator
        mixedSample += noise.nextSample();
        mixedSample += karplus.nextSample();
        mixedSample += wavetable.nextSample();

        // Apply amplitude LFO (tremolo)
        if (lfoTarget == LFOTarget::Amplitude)
            mixedSample *= (1.0f + lfoValue) * 0.5f; // map -1..+1 to 0..1

        // Filter
        mixedSample = filter.process(mixedSample);

        // Envelope
        mixedSample *= envelope.process();

        // Effects
        mixedSample = distortion.process(mixedSample);
        mixedSample = chorus.process(mixedSample);
        mixedSample = delay.process(mixedSample);
        mixedSample = reverb.process(mixedSample);

        mixedSample = std::clamp(mixedSample, -1.0f, 1.0f);

        for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
            outputBuffer.addSample(channel, startSample + sample, mixedSample);

        if (envelope.getStage() == AdsrEnvelope::Stage::Idle)
        {
            clearCurrentNote();
            noteOn = false;
            break;
        }
    }

    // Restore base values
    for (int i = 0; i < 3; ++i)
        oscillators[i].setFrequency(baseFrequencies[i]);
    filter.setCutoff(baseCutoff);
}
