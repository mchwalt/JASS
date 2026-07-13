#include "SynthVoice.h"

bool SynthVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<SynthSound*>(sound) != nullptr;
}

void SynthVoice::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    glideRatio.reset(sampleRate, 0.0);
    glideRatio.setCurrentAndTargetValue(1.0);
    for (auto& osc : oscillators)
        osc.setSampleRate(sampleRate);
    subOsc.setSampleRate(sampleRate);
    envelope.setSampleRate(sampleRate);
    filter.setSampleRate(sampleRate);
    formant.prepare(sampleRate);
    lfo.setSampleRate(sampleRate);
    karplus.setSampleRate(sampleRate);
    wavetable.setSampleRate(sampleRate);
    phaser.prepare(sampleRate);
    delay.prepare(sampleRate);
    chorus.prepare(sampleRate);
    reverb.prepare(sampleRate);
}

void SynthVoice::startNote(int midiNoteNumber, float /*velocity*/,
                           juce::SynthesiserSound*, int)
{
    // All generators transpose relative to C4 (note 60); the FREQ knobs define
    // the sound AT C4. Note 60 (the auto-play drone) gives ratio 1.0 = no shift.
    transposeRatio = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber)
                   / juce::MidiMessage::getMidiNoteInHertz(60);

    // Poly-glide: start this note's pitch at the assigned predecessor ratio and glide to
    // its own ratio. startRatio < 0 (no predecessor / glide off) => start on pitch instantly.
    float glideFrom = -1.0f;
    if (glideInfo != nullptr && glideInfo->enabled && midiNoteNumber >= 0 && midiNoteNumber < 128)
        glideFrom = glideInfo->startRatio[(size_t) midiNoteNumber];
    if (glideFrom > 0.0f && glideInfo->timeSec > 0.001)
    {
        glideRatio.reset(currentSampleRate, glideInfo->timeSec);
        glideRatio.setCurrentAndTargetValue((double) glideFrom);
        glideRatio.setTargetValue(transposeRatio);
    }
    else
    {
        glideRatio.reset(currentSampleRate, 0.0);
        glideRatio.setCurrentAndTargetValue(transposeRatio);
    }

    for (auto& osc : oscillators)
        osc.reset();
    subOsc.reset();
    lfo.reset();
    noise.reset();
    wavetable.reset();
    if (pluckEnabled)
        pluckKarplus();   // pluck at the transposed pitch (skipped for the drone)

    envelope.gateOn();
    noteOn = true;
}

void SynthVoice::pluckKarplus()
{
    // The string's pitch is fixed at pluck time by its delay-line length, so
    // apply the transposition here (then restore the knob value to avoid
    // compounding on the next pluck).
    double knob = karplus.getFrequency();
    karplus.setFrequency(knob * transposeRatio);
    karplus.pluck();
    karplus.setFrequency(knob);
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

    // Capture the knob frequencies; everything plays transposed by the note.
    for (int i = 0; i < 3; ++i)
        baseFrequencies[i] = oscillators[i].getFrequency();
    double baseWtFreq = wavetable.getFrequency();
    baseCutoff = filter.getCutoff();

    // Sub-oscillator octave multiplier (constant across the block).
    const double subMul = std::pow(2.0, subOctave);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Poly-glide: the pitch ratio glides toward the target (== target once finished /
        // when glide is off). Oscillator frequencies are (re)applied every sample so both
        // the glide and the LFO frequency modulation take effect.
        const double ratio = glideRatio.getNextValue();

        // LFO modulation
        float lfoValue = lfo.process();
        auto lfoTarget = lfo.getTarget();

        const double freqFactor = (lfoTarget == LFOTarget::Frequency)
                                      ? std::pow(2.0, lfoValue)   // ±1 octave at full depth
                                      : 1.0;
        for (int i = 0; i < 3; ++i)
            oscillators[i].setFrequency(baseFrequencies[i] * ratio * freqFactor);
        wavetable.setFrequency(baseWtFreq * ratio);
        subOsc.setFrequency(baseFrequencies[0] * ratio * subMul);

        if (lfoTarget == LFOTarget::FilterCutoff)
        {
            // Modulate cutoff logarithmically
            double factor = std::pow(2.0, lfoValue * 3.0); // ±3 octaves
            filter.setCutoff(std::clamp(baseCutoff * factor, 20.0, 20000.0));
        }

        // Mix oscillators according to the mix mode. When Mix-Mode is disabled (Story 2.4)
        // the OSCs are summed plainly, regardless of the selected mode.
        // Epic 5: A/B are user-selectable operands (0..2 => OSC 1/2/3); the third OSC
        // (o = 3-a-b) is summed plainly. Defaults A=0,B=1,o=2 == the prior fixed OSC1<->OSC2.
        // EVERY oscillator is advanced exactly once per sample in every branch (no phase drift);
        // a==b falls back to plain additive so no oscillator is advanced twice.
        const int a = juce::jlimit(0, 2, mixSrcA);
        const int b = juce::jlimit(0, 2, mixSrcB);
        float mixedSample = 0.0f;
        if (mixModeOn && a != b && mixMode == MixMode::FM)
        {
            // FM: OSC A modulates OSC B's frequency; the third OSC is additive.
            const int o = 3 - a - b;
            float modulator = oscillators[a].nextSample();
            double fmOffset = modulator * oscillators[a].getFrequency() * 2.0;
            float carrier = oscillators[b].nextSample(fmOffset);
            float other = oscillators[o].nextSample();
            mixedSample = carrier + other;
        }
        else if (mixModeOn && a != b && mixMode == MixMode::RingMod)
        {
            // Ring: OSC A × OSC B, + the third OSC.
            const int o = 3 - a - b;
            float sa = oscillators[a].nextSample();
            float sb = oscillators[b].nextSample();
            float other = oscillators[o].nextSample();
            mixedSample = sa * sb * 2.0f + other;
        }
        else
        {
            for (auto& osc : oscillators)
                mixedSample += osc.nextSample();
        }

        // Add noise + Karplus string + wavetable + sub-oscillator
        mixedSample += noise.nextSample();
        mixedSample += karplus.nextSample();
        mixedSample += wavetable.nextSample();
        mixedSample += subOsc.nextSample();

        // Wavefolding (West-Coast timbre): fold the raw signal BEFORE the filter
        // so the filter can tame the harsh upper harmonics it generates.
        mixedSample = wavefolder.process(mixedSample);

        // Apply amplitude LFO (tremolo)
        if (lfoTarget == LFOTarget::Amplitude)
            mixedSample *= (1.0f + lfoValue) * 0.5f; // map -1..+1 to 0..1

        // Filter (main biquad), then the vowel/formant filter.
        mixedSample = filter.process(mixedSample);
        mixedSample = formant.process(mixedSample);

        // Envelope. Always advance the ADSR state (so toggling mid-note doesn't glitch);
        // when disabled (Story 2.4) bypass it with constant gain 1.0.
        const float envGain = envelope.process();
        mixedSample *= (adsrOn ? envGain : 1.0f);

        // Effects
        mixedSample = distortion.process(mixedSample);
        mixedSample = bitcrusher.process(mixedSample);
        mixedSample = phaser.process(mixedSample);
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

    // Restore the knob frequencies (applyToVoice resets them next block anyway)
    for (int i = 0; i < 3; ++i)
        oscillators[i].setFrequency(baseFrequencies[i]);
    wavetable.setFrequency(baseWtFreq);
    filter.setCutoff(baseCutoff);
}
