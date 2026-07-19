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
    bypassGate.reset(sampleRate, 0.010);   // 10 ms anti-click gate for the ADSR-bypass path
    pitchEnv.setSampleRate(sampleRate);
    filter.setSampleRate(sampleRate);
    formant.prepare(sampleRate);
    for (auto& l : lfos) l.setSampleRate(sampleRate);
    karplus.setSampleRate(sampleRate);
    wavetable.setSampleRate(sampleRate);
    phaser.prepare(sampleRate);
    delay.prepare(sampleRate);
    chorus.prepare(sampleRate);
    reverb.prepare(sampleRate);
}

void SynthVoice::startNote(int midiNoteNumber, float velocity,
                           juce::SynthesiserSound*, int)
{
    noteVelocity = velocity;   // Velocity mod source (Story 8.1): 0..1, constant across the note.

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
    for (auto& l : lfos) l.reset();
    noise.reset();
    wavetable.reset();
    if (pluckEnabled)
        pluckKarplus();   // pluck at the transposed pitch (skipped for the drone)

    pitchEnv.trigger();   // (re)start the one-shot pitch sweep at note-on
    envelope.gateOn();
    bypassGate.setCurrentAndTargetValue(0.0f);
    bypassGate.setTargetValue(1.0f);   // fast fade-in (used only when the ADSR module is off)
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
    {
        envelope.gateOff();
        bypassGate.setTargetValue(0.0f);   // fast fade-out (ADSR-bypass path); frees the voice in ~10 ms
    }
    else
    {
        envelope.reset();
        bypassGate.setCurrentAndTargetValue(0.0f);
        clearCurrentNote();
        noteOn = false;
    }
}

void SynthVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                                  int startSample, int numSamples)
{
    // Voice is inactive when the note is off AND its amplitude source has fully decayed: the ADSR
    // (release => Idle) when the envelope module is on, else the fast bypass gate (reached 0).
    if (!noteOn && (adsrOn ? envelope.getStage() == AdsrEnvelope::Stage::Idle
                           : bypassGate.getCurrentValue() <= 0.0f))
        return;

    // Capture the knob frequencies; everything plays transposed by the note.
    for (int i = 0; i < 3; ++i)
        baseFrequencies[i] = oscillators[i].getFrequency();
    double baseWtFreq = wavetable.getFrequency();
    baseCutoff = filter.getCutoff();
    // Base values for the additional LFO targets (captured once; the LFO modulates around them).
    const double baseReso  = filter.getResonance();
    const double basePos   = wavetable.getPosition();
    const double baseVowel = formant.vowel;
    const double baseFold  = wavefolder.drive;

    // Sub-oscillator octave multiplier (constant across the block).
    const double subMul = std::pow(2.0, subOctave);

    // --- Modulation matrix (Story 8.1) ---------------------------------------
    // Which targets receive ANY routing this block: the LFO's own (implicit) target plus
    // every active slot's target. Only these get their per-sample offset applied, so an
    // untouched target is left exactly as applyToVoice set it — byte-identical to the old
    // single-target behaviour when nothing (or only the legacy LFO) is routed.
    // ModSource index for each LFO (append-only order): LFO 1 -> ModSource::LFO1, LFO 2 -> LFO2, …
    // LFOs no longer have a built-in TARGET (removed 2026-07-19) — they route ONLY via matrix slots.
    static constexpr int kLfoSourceIdx[kNumLFOs] = { (int) ModSource::LFO1, (int) ModSource::LFO2,
                                                     (int) ModSource::LFO3, (int) ModSource::LFO4 };

    std::array<bool, ModMatrixConfig::kNumTargets> tActive {};
    if (modMatrixOn)
        for (const auto& s : modSlots)
            if (s.target > 0 && s.target < ModMatrixConfig::kNumTargets && s.amount != 0.0f)
                tActive[(size_t) s.target] = true;
    std::array<double, ModMatrixConfig::kNumTargets> modOffset {};   // per-sample summed offsets

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Poly-glide: the pitch ratio glides toward the target (== target once finished /
        // when glide is off). Oscillator frequencies are (re)applied every sample so both
        // the glide and the frequency modulation take effect.
        const double ratio = glideRatio.getNextValue();

        // Modulation sources this sample. lfo.process() advances the shared LFO ONCE (its
        // value feeds both the implicit LFO routing and any slot whose source is LFO 1).
        // envelope.process() advances the ADSR ONCE here and is REUSED for the amplitude
        // gain below — it must NOT be advanced again this sample. Velocity is constant.
        float lfoVals[kNumLFOs];
        for (int i = 0; i < kNumLFOs; ++i) lfoVals[i] = lfos[i].process();
        const float envValue = envelope.process();
        const float gateG    = bypassGate.getNextValue();   // advanced every sample (ADSR-bypass gain + voice-free)
        // Envelope as a mod SOURCE is gated by the ENVELOPE module enable, so a source is
        // only active when its module is on — the same rule as the LFOs (silent when off).
        // (envValue itself still drives the amplitude gain below regardless, per Story 2.4.)
        const float envSource = adsrOn ? envValue : 0.0f;
        // Fill by ModSource index: Envelope + Velocity fixed, each LFO into its mapped source.
        std::array<float, ModMatrixConfig::kNumSources> srcVals {};
        srcVals[(size_t) ModSource::Envelope] = envSource;
        srcVals[(size_t) ModSource::Velocity] = noteVelocity;
        for (int i = 0; i < kNumLFOs; ++i) srcVals[(size_t) kLfoSourceIdx[i]] = lfoVals[i];

        // Sum the explicit matrix slots into the per-target offset (LFOs are just sources now).
        modMatrixAccumulate(modSlots, modMatrixOn, srcVals, modOffset);

        // Pitch: 2^offset octaves (offset 0 => ×1 = unchanged). Always applied — freqFactor
        // multiplies the per-sample setFrequency that glide/pitch-env already require.
        const double freqFactor = std::pow(2.0, modOffset[(size_t) LFOTarget::Frequency]);
        // Pitch envelope: one-shot 1→0 sweep, applied as a pitch multiplier to all
        // pitched generators (osc/wavetable/sub; the plucked Karplus string is excluded).
        const double pitchEnvMul = pitchEnvOn
                                       ? std::pow(2.0, (pitchEnvAmount / 12.0) * pitchEnv.process())
                                       : 1.0;
        // freqFactor (matrix/LFO "Pitch" target) modulates EVERY pitched generator, exactly like
        // the pitch envelope above — otherwise a Pitch routing is inaudible on wavetable-/sub-heavy
        // patches (e.g. whuwhu, whose loudest voice is the wavetable). osc + wavetable + sub all get it.
        for (int i = 0; i < 3; ++i)
            oscillators[i].setFrequency(baseFrequencies[i] * ratio * freqFactor * pitchEnvMul);
        wavetable.setFrequency(baseWtFreq * ratio * freqFactor * pitchEnvMul);
        subOsc.setFrequency(baseFrequencies[0] * ratio * subMul * freqFactor * pitchEnvMul);

        // Apply each ACTIVE target's summed offset ONCE, reusing today's exact curve+clamp.
        if (tActive[(size_t) LFOTarget::FilterCutoff])
            filter.setCutoff(std::clamp(baseCutoff * std::pow(2.0, modOffset[(size_t) LFOTarget::FilterCutoff] * 3.0), 20.0, 20000.0));
        if (tActive[(size_t) LFOTarget::WavetablePosition])
            wavetable.setPosition(std::clamp(basePos + modOffset[(size_t) LFOTarget::WavetablePosition] * 0.5, 0.0, 1.0));
        if (tActive[(size_t) LFOTarget::FormantVowel])
            formant.vowel = std::clamp(baseVowel + modOffset[(size_t) LFOTarget::FormantVowel] * 0.5, 0.0, 1.0);
        if (tActive[(size_t) LFOTarget::FilterResonance])
            filter.setResonance(std::clamp(baseReso * std::pow(2.0, modOffset[(size_t) LFOTarget::FilterResonance] * 1.5), 0.1, 10.0));
        if (tActive[(size_t) LFOTarget::WavefolderDrive])
            wavefolder.drive = std::clamp(baseFold + modOffset[(size_t) LFOTarget::WavefolderDrive] * 0.5, 0.0, 1.0);

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

        // Apply amplitude modulation (tremolo). Same (1+v)*0.5 map as before; only when
        // Amplitude is an active target, so an unrouted patch keeps full gain (no ×0.5).
        if (tActive[(size_t) LFOTarget::Amplitude])
            mixedSample *= (1.0f + (float) modOffset[(size_t) LFOTarget::Amplitude]) * 0.5f;

        // Filter (main biquad), then the vowel/formant filter.
        mixedSample = filter.process(mixedSample);
        mixedSample = formant.process(mixedSample);

        // Envelope gain. The ADSR was already advanced ONCE at the top of the loop (envValue,
        // also a mod source); reuse it here — do not advance twice. When disabled (Story 2.4)
        // bypass it with the fast gate (NOT constant 1.0 — that made a released note hang at full
        // volume for the whole release time before cutting; the gate fades in/out in ~10 ms and
        // frees the voice promptly). The envelope still advances, so toggling mid-note doesn't glitch.
        mixedSample *= (adsrOn ? envValue : gateG);

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

        // Free the voice when its amplitude source has fully decayed: ADSR reaching Idle (envelope
        // on), else the fast bypass gate reaching 0 (envelope off) — so a released note stops in
        // ~10 ms instead of hanging for the envelope's release time at full gain.
        const bool voiceIdle = adsrOn ? (envelope.getStage() == AdsrEnvelope::Stage::Idle)
                                      : (gateG <= 0.0f && ! bypassGate.isSmoothing());
        if (voiceIdle)
        {
            clearCurrentNote();
            noteOn = false;
            break;
        }
    }

    // Restore the knob values (applyToVoice resets them next block anyway)
    for (int i = 0; i < 3; ++i)
        oscillators[i].setFrequency(baseFrequencies[i]);
    wavetable.setFrequency(baseWtFreq);
    wavetable.setPosition(basePos);
    filter.setCutoff(baseCutoff);
    filter.setResonance(baseReso);
    formant.vowel = baseVowel;
    wavefolder.drive = baseFold;
}
