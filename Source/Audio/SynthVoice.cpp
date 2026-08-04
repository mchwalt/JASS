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
    for (auto& l : lfos) l.setSampleRate(sampleRate);
    karplus.setSampleRate(sampleRate);
    wavetable.setSampleRate(sampleRate);
    sampler.setSampleRate(sampleRate);   // Story 12.1
    // Prepare EVERY channel strip's effects (not just strip 0) so a later Stereo-Pan channel is ready
    // (its delay/reverb buffers preallocated) — no allocation ever happens on the audio thread.
    for (auto& s : strips)
    {
        s.filter.setSampleRate(sampleRate);
        s.formant.prepare(sampleRate);
        s.phaser.prepare(sampleRate);
        s.delay.prepare(sampleRate);
        s.chorus.prepare(sampleRate);
        s.reverb.prepare(sampleRate);
    }
    for (auto& b : binaural) b.prepare(sampleRate);   // 10.3: size the per-generator ITD delay lines once
    for (auto& h : hrtf)     h.prepare(sampleRate);   // 10.3: init HRIR panners (clear history, center kernel)
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
    for (auto& b : binaural) b.reset();   // 10.3: clear ITD tails so a new note doesn't click
    for (auto& h : hrtf)     h.reset();   // 10.3: clear HRIR history so a new note doesn't click
    if (pluckEnabled)
        pluckKarplus();   // pluck at the transposed pitch (skipped for the drone)

    sampler.trigger(transposeRatio, midiNoteNumber);   // Story 12.1: (re)start the recording at the
                                                       // note's rate; the note picks the zone (12.2)
    pitchEnv.trigger();   // (re)start the one-shot pitch sweep at note-on
    envelope.gateOn();
    samplerTailHold = false;   // 12.4: a retrigger ends any held tail state
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
        sampler.gateOff();   // Story 12.4: start the sampler's own release fade (zone/REL);
                             // the sustain pedal (CC64) delays this call until the pedal lifts
        // 12.4: with the ADSR module OFF the 10 ms bypass gate would cut that fade right off
        // (user report 2026-08-04) — hold the gate open while the sampler rings out; it fades
        // once the ramp is spent (checked per block in renderNextBlock). Other generators in
        // an ADSR-off preset sustain under the tail — the documented trade-off; a sampled
        // instrument is normally the ONLY generator when this matters.
        if (! adsrOn && sampler.isRingingOut())
            samplerTailHold = true;
        else
            bypassGate.setTargetValue(0.0f);   // fast fade-out (ADSR-bypass path); frees the voice in ~10 ms
    }
    else
    {
        envelope.reset();
        sampler.reset();   // Story 12.1: hard stop ends the recording too
        samplerTailHold = false;
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

    // 12.4: ADSR-off sampler tail — the gate was held open in stopNote; once the fade is spent,
    // close it so the voice frees normally. Per-block granularity is fine: the ramp floor is
    // −80 dB, one block of slack is inaudible.
    if (samplerTailHold && ! sampler.isRingingOut())
    {
        samplerTailHold = false;
        bypassGate.setTargetValue(0.0f);
    }

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
    // Base values for the appended per-voice FX/generator targets (captured once; modulated around).
    const double baseDelayTime = delay.time;
    const double baseDelayMix  = delay.mix;
    const double baseReverbMix = reverb.mix;
    const double baseChorusDep = chorus.depth;
    const double baseDistDrive = distortion.drive;
    const double baseCrushMix  = bitcrusher.mix;
    const double baseSubLevel  = subOsc.getAmplitude();
    const std::array<double, 3> baseDetune { oscillators[0].getDetuneAmount(),
                                             oscillators[1].getDetuneAmount(),
                                             oscillators[2].getDetuneAmount() };
    const std::array<double, 3> baseFeedback { oscillators[0].getFeedback(),
                                               oscillators[1].getFeedback(),
                                               oscillators[2].getFeedback() };
    const std::array<double, 3> baseVoices { (double) oscillators[0].getUnisonCount(),
                                             (double) oscillators[1].getUnisonCount(),
                                             (double) oscillators[2].getUnisonCount() };
    // Epic 8.3 — base values for the full per-module target coverage (captured once, modulated around).
    const double baseWtAmp        = wavetable.getAmplitude();
    const double baseSamplerLevel = sampler.getLevel();   // Story 12.1
    const double baseWtVoices     = (double) wavetable.getUnisonCount();
    const double baseWtDetune     = wavetable.getDetuneAmount();
    const double baseFormantReso  = formant.resonance;
    const double baseFormantMix   = formant.mix;
    const double baseWavefoldSym  = wavefolder.symmetry;
    const double baseWavefoldMix  = wavefolder.mix;
    const double baseDistMix      = distortion.mix;
    const double baseBits         = bitcrusher.bits;
    const double baseRate         = bitcrusher.rate;
    const double baseChorusRate   = chorus.rate;
    const double baseChorusMix    = chorus.mix;
    const double baseDelayFb      = delay.feedback;
    const double baseReverbRoom   = reverb.roomSize;
    const double baseReverbDamp   = reverb.damping;
    const double basePhaserRate   = phaser.rate;
    const double basePhaserDepth  = phaser.depth;
    const double basePhaserFb     = phaser.feedback;
    const double basePhaserMix    = phaser.mix;
    // Per-voice generator/modulator targets appended 2026-07-26 (NOISE/KARPLUS/PITCH ENV). Karplus
    // FREQ is deliberately not modulatable — its pitch is baked into the delay line at pluck time.
    const double baseNoiseAmp     = noise.getAmplitude();
    const double baseKarplusAmp   = karplus.getAmplitude();
    const double baseKarplusDamp  = karplus.getDamping();
    const double baseKarplusStr   = karplus.getStretch();
    const double basePitchEnvAmt  = pitchEnvAmount;

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

    // tActive gates the GLOBAL per-target apply. A per-oscillator slot (oscIndex 0..2) is handled
    // separately below, so it must NOT set tActive for FREQ/AMP/DETUNE — otherwise the global apply
    // would fire too and double-modulate. "Alle OSC" (oscIndex -1) stays global, as before.
    std::array<bool, ModMatrixConfig::kNumTargets> tActive {};
    bool oscAmpActive[3] = { false, false, false };   // a per-oscillator AMP routing exists on this OSC
    bool anyOscDetune    = false;                      // any per-oscillator DETUNE routing exists
    bool anyOscFeedback  = false;                      // any per-oscillator FEEDBACK routing exists
    bool anyOscVoices    = false;                      // any per-oscillator VOICES routing exists
    bool anyOscPan       = false;                      // any per-oscillator PAN routing exists (Epic 10)
    if (modMatrixOn)
        for (const auto& s : modSlots)
            if (s.target > 0 && s.target < ModMatrixConfig::kNumTargets && s.amount != 0.0f)
            {
                if (s.oscIndex >= 0 && s.oscIndex < 3)   // per-oscillator routing (FREQ/AMP/DETUNE/FB/VOICES)
                {
                    if (s.target == (int) LFOTarget::Amplitude)   oscAmpActive[s.oscIndex] = true;
                    if (s.target == (int) LFOTarget::OscDetune)   anyOscDetune = true;
                    if (s.target == (int) LFOTarget::OscFeedback) anyOscFeedback = true;
                    if (s.target == (int) LFOTarget::OscVoices)   anyOscVoices = true;
                    if (s.target == (int) LFOTarget::OscPan)      anyOscPan    = true;
                    continue;   // pitch is always applied (harmless at 0); the rest use the flags
                }
                tActive[(size_t) s.target] = true;
            }
    std::array<double, ModMatrixConfig::kNumTargets> modOffset {};   // per-sample summed offsets
    OscModOffsets oscOffset {};                                      // per-sample per-OSC offsets

    // ── Spatialization (Epic 10) ────────────────────────────────────────────────────────────
    // Channel count for this block: 1 (Mono / Pseudo-Stereo — byte-identical mono path) or 2
    // (Stereo-Pan). Per-generator equal-power pan gains, computed once per block (no trig per sample).
    const bool binauralMode = (outputMode == (int) OutputMode::Binaural);
    const bool hrtfMode     = (outputMode == (int) OutputMode::Kunstkopf);   // 10.3: HRIR convolution (2-ch)
    const int nCh = (outputMode == (int) OutputMode::StereoPan || binauralMode || hrtfMode) ? kMaxOutChannels : 1;
    for (int g = 0; g < kNumPanGenerators; ++g)
        positionToGains(generatorPan[(size_t) g], nCh, panGains[g]);
    // Effective per-generator pan value (base + modulation) — the binaural renderer needs the pan
    // VALUE (curGains carries only the equal-power amplitude/ILD gains). Static per block unless a PAN
    // mod routing is active (then refreshed per sample alongside curGains, below).
    float effPan[kNumPanGenerators];
    for (int g = 0; g < kNumPanGenerators; ++g) effPan[g] = generatorPan[g];

    // HRTF (Kunstkopf) selects each generator's HRIR kernel ONCE PER BLOCK (anti-zipper granularity);
    // under an active PAN mod it is refreshed a few times per block inside the sample loop (throttled).
    if (hrtfMode)
        for (int g = 0; g < kNumPanGenerators; ++g) hrtf[(size_t) g].setPanForBlock(effPan[g]);

    // PAN as a mod target (Epic 10): auto-panning. Only meaningful in Stereo-Pan (nCh>1). When active,
    // the per-generator pan gains are re-derived PER SAMPLE from the base pan + the modulation; else the
    // per-block panGains stand. curGains holds whichever is current; the mix reads it.
    const bool panMod = (nCh > 1) && (anyOscPan
                        || tActive[(size_t) LFOTarget::OscPan]      || tActive[(size_t) LFOTarget::SubPan]
                        || tActive[(size_t) LFOTarget::NoisePan]    || tActive[(size_t) LFOTarget::KarplusPan]
                        || tActive[(size_t) LFOTarget::WavetablePan]);
    float curGains[kNumPanGenerators][kMaxOutChannels];
    for (int g = 0; g < kNumPanGenerators; ++g)
        for (int c = 0; c < kMaxOutChannels; ++c) curGains[g][c] = panGains[g][c];

    // The per-sample effect-parameter MODULATION, applied to ONE channel strip. Runs per active
    // strip (nCh==1 → strip 0 only = today). Only FX targets here; generator targets (wavetable/sub/
    // noise/karplus/pitch-env) stay pre-pan and are applied to the shared generators inline below.
    auto applyFxMods = [&] (ChannelStrip& s)
    {
        if (tActive[(size_t) LFOTarget::FilterCutoff])
            s.filter.setCutoff(std::clamp(baseCutoff * std::pow(2.0, modOffset[(size_t) LFOTarget::FilterCutoff] * 3.0), 20.0, 20000.0));
        if (tActive[(size_t) LFOTarget::FormantVowel])
            s.formant.vowel = std::clamp(baseVowel + modOffset[(size_t) LFOTarget::FormantVowel] * 0.5, 0.0, 1.0);
        if (tActive[(size_t) LFOTarget::FilterResonance])
            s.filter.setResonance(std::clamp(baseReso * std::pow(2.0, modOffset[(size_t) LFOTarget::FilterResonance] * 1.5), 0.1, 10.0));
        if (tActive[(size_t) LFOTarget::WavefolderDrive])
            s.wavefolder.drive = std::clamp(baseFold + modOffset[(size_t) LFOTarget::WavefolderDrive] * 0.5, 0.0, 1.0);
        if (tActive[(size_t) LFOTarget::DelayTime])
            s.delay.time = std::clamp(baseDelayTime + modOffset[(size_t) LFOTarget::DelayTime] * 0.2, 0.01, 2.0);
        if (tActive[(size_t) LFOTarget::DelayMix])
            s.delay.mix = std::clamp(baseDelayMix + modOffset[(size_t) LFOTarget::DelayMix] * 0.5, 0.0, 1.0);
        if (tActive[(size_t) LFOTarget::ReverbMix])
            s.reverb.mix = std::clamp(baseReverbMix + modOffset[(size_t) LFOTarget::ReverbMix] * 0.5, 0.0, 1.0);
        if (tActive[(size_t) LFOTarget::ChorusDepth])
            s.chorus.depth = std::clamp(baseChorusDep + modOffset[(size_t) LFOTarget::ChorusDepth] * 0.01, 0.001, 0.02);
        if (tActive[(size_t) LFOTarget::DistortionDrive])
            s.distortion.drive = std::clamp(baseDistDrive + modOffset[(size_t) LFOTarget::DistortionDrive] * 0.5, 0.0, 1.0);
        if (tActive[(size_t) LFOTarget::BitcrushMix])
            s.bitcrusher.mix = std::clamp(baseCrushMix + modOffset[(size_t) LFOTarget::BitcrushMix] * 0.5, 0.0, 1.0);
        if (tActive[(size_t) LFOTarget::FormantReso])
            s.formant.resonance = std::clamp(baseFormantReso + modOffset[(size_t) LFOTarget::FormantReso] * 0.5, 0.0, 1.0);
        if (tActive[(size_t) LFOTarget::FormantMix])
            s.formant.mix = std::clamp(baseFormantMix + modOffset[(size_t) LFOTarget::FormantMix] * 0.5, 0.0, 1.0);
        if (tActive[(size_t) LFOTarget::WavefolderSym])
            s.wavefolder.symmetry = std::clamp(baseWavefoldSym + modOffset[(size_t) LFOTarget::WavefolderSym] * 0.5, -1.0, 1.0);
        if (tActive[(size_t) LFOTarget::WavefolderMix])
            s.wavefolder.mix = std::clamp(baseWavefoldMix + modOffset[(size_t) LFOTarget::WavefolderMix] * 0.5, 0.0, 1.0);
        if (tActive[(size_t) LFOTarget::DistortionMix])
            s.distortion.mix = std::clamp(baseDistMix + modOffset[(size_t) LFOTarget::DistortionMix] * 0.5, 0.0, 1.0);
        if (tActive[(size_t) LFOTarget::BitcrushBits])
            s.bitcrusher.bits = (double) juce::roundToInt(std::clamp(baseBits + modOffset[(size_t) LFOTarget::BitcrushBits] * 8.0, 1.0, 16.0));
        if (tActive[(size_t) LFOTarget::BitcrushRate])
            s.bitcrusher.rate = (double) juce::roundToInt(std::clamp(baseRate + modOffset[(size_t) LFOTarget::BitcrushRate] * 25.0, 1.0, 50.0));
        if (tActive[(size_t) LFOTarget::ChorusRate])
            s.chorus.rate = std::clamp(baseChorusRate + modOffset[(size_t) LFOTarget::ChorusRate] * 2.0, 0.1, 5.0);
        if (tActive[(size_t) LFOTarget::ChorusMix])
            s.chorus.mix = std::clamp(baseChorusMix + modOffset[(size_t) LFOTarget::ChorusMix] * 0.5, 0.0, 1.0);
        if (tActive[(size_t) LFOTarget::DelayFeedback])
            s.delay.feedback = std::clamp(baseDelayFb + modOffset[(size_t) LFOTarget::DelayFeedback] * 0.4, 0.0, 0.95);
        if (tActive[(size_t) LFOTarget::ReverbRoom])
            s.reverb.roomSize = std::clamp(baseReverbRoom + modOffset[(size_t) LFOTarget::ReverbRoom] * 0.5, 0.0, 1.0);
        if (tActive[(size_t) LFOTarget::ReverbDamp])
            s.reverb.damping = std::clamp(baseReverbDamp + modOffset[(size_t) LFOTarget::ReverbDamp] * 0.5, 0.0, 1.0);
        if (tActive[(size_t) LFOTarget::PhaserRate])
            s.phaser.rate = std::clamp(basePhaserRate + modOffset[(size_t) LFOTarget::PhaserRate] * 2.0, 0.05, 5.0);
        if (tActive[(size_t) LFOTarget::PhaserDepth])
            s.phaser.depth = std::clamp(basePhaserDepth + modOffset[(size_t) LFOTarget::PhaserDepth] * 0.5, 0.0, 1.0);
        if (tActive[(size_t) LFOTarget::PhaserFeedback])
            s.phaser.feedback = std::clamp(basePhaserFb + modOffset[(size_t) LFOTarget::PhaserFeedback] * 0.4, 0.0, 0.95);
        if (tActive[(size_t) LFOTarget::PhaserMix])
            s.phaser.mix = std::clamp(basePhaserMix + modOffset[(size_t) LFOTarget::PhaserMix] * 0.5, 0.0, 1.0);
    };

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

        // Sum the explicit matrix slots into the per-target offset + the per-OSC offsets
        // (LFOs are just sources now).
        modMatrixAccumulate(modSlots, modMatrixOn, srcVals, modOffset, oscOffset);

        // Pitch: 2^offset octaves (offset 0 => ×1 = unchanged). The GLOBAL ("Alle OSC") FREQ routing
        // and each oscillator's OWN per-OSC FREQ offset SUM in octave space; the total is CLAMPED to
        // ±kMaxPitchOct so many stacked slots can't drive the pitch into absurd, aliased territory
        // (every other FREQ-like target already clamps to its range). Small/single routings unaffected.
        constexpr double kMaxPitchOct = 4.0;   // ±4 octaves ceiling on summed FREQ modulation
        const double gPitch = modOffset[(size_t) LFOTarget::Frequency];   // global "Alle OSC" octaves
        // Pitch envelope: one-shot 1→0 sweep, applied as a pitch multiplier to all
        // pitched generators (osc/wavetable/sub; the plucked Karplus string is excluded).
        const double pitchEnvMul = pitchEnvOn
                                       ? std::pow(2.0, (pitchEnvAmount / 12.0) * pitchEnv.process())
                                       : 1.0;
        // The GLOBAL octaves modulate EVERY pitched generator (osc + wavetable + sub); each oscillator
        // adds its own per-OSC offset, the wavetable its own WT-Freq offset, both clamped after summing.
        for (int i = 0; i < 3; ++i)
        {
            const double oct = std::clamp(gPitch + oscOffset.pitch[i], -kMaxPitchOct, kMaxPitchOct);
            oscillators[i].setFrequency(baseFrequencies[i] * ratio * std::pow(2.0, oct) * pitchEnvMul);
        }
        const double wtOct = std::clamp(gPitch + modOffset[(size_t) LFOTarget::WavetableFreq], -kMaxPitchOct, kMaxPitchOct);
        wavetable.setFrequency(baseWtFreq * ratio * std::pow(2.0, wtOct) * pitchEnvMul);
        subOsc.setFrequency(baseFrequencies[0] * ratio * subMul
                            * std::pow(2.0, std::clamp(gPitch, -kMaxPitchOct, kMaxPitchOct)) * pitchEnvMul);

        // GENERATOR-target modulation (pre-pan, applied to the shared generators ONCE). The FX-target
        // modulation moved into applyFxMods() above and runs per channel strip in the chain loop below.
        if (tActive[(size_t) LFOTarget::WavetablePosition])
            wavetable.setPosition(std::clamp(basePos + modOffset[(size_t) LFOTarget::WavetablePosition] * 0.5, 0.0, 1.0));
        if (tActive[(size_t) LFOTarget::SubLevel])
            subOsc.setAmplitude(std::clamp(baseSubLevel + modOffset[(size_t) LFOTarget::SubLevel] * 0.5, 0.0, 1.0));
        if (tActive[(size_t) LFOTarget::NoiseLevel])
            noise.setAmplitude(std::clamp(baseNoiseAmp + modOffset[(size_t) LFOTarget::NoiseLevel] * 0.5, 0.0, 1.0));
        if (tActive[(size_t) LFOTarget::SamplerLevel])
            sampler.setLevel(std::clamp(baseSamplerLevel + modOffset[(size_t) LFOTarget::SamplerLevel] * 0.5, 0.0, 1.0));
        if (tActive[(size_t) LFOTarget::KarplusAmp])
            karplus.setAmplitude(std::clamp(baseKarplusAmp + modOffset[(size_t) LFOTarget::KarplusAmp] * 0.5, 0.0, 1.0));
        if (tActive[(size_t) LFOTarget::KarplusDamping])
            karplus.setDamping(std::clamp(baseKarplusDamp + modOffset[(size_t) LFOTarget::KarplusDamping] * 0.5, 0.0, 1.0));
        if (tActive[(size_t) LFOTarget::KarplusStretch])
            karplus.setStretch(std::clamp(baseKarplusStr + modOffset[(size_t) LFOTarget::KarplusStretch] * 0.5, 0.0, 1.0));
        if (tActive[(size_t) LFOTarget::PitchEnvAmount])
            pitchEnvAmount = std::clamp(basePitchEnvAmt + modOffset[(size_t) LFOTarget::PitchEnvAmount] * 24.0, -48.0, 48.0);
        if (tActive[(size_t) LFOTarget::WavetableAmp])
            wavetable.setAmplitude(std::clamp(baseWtAmp + modOffset[(size_t) LFOTarget::WavetableAmp] * 0.5, 0.0, 1.0));
        if (tActive[(size_t) LFOTarget::WavetableVoices])
            wavetable.setUnisonCount(juce::roundToInt(std::clamp(baseWtVoices + modOffset[(size_t) LFOTarget::WavetableVoices] * 3.0, 1.0, 7.0)));
        if (tActive[(size_t) LFOTarget::WavetableDetune])
            wavetable.setDetuneAmount(std::clamp(baseWtDetune + modOffset[(size_t) LFOTarget::WavetableDetune] * 50.0, 0.0, 100.0));

        // Detune: base + GLOBAL ("Alle OSC") offset + this OSC's OWN offset. Only re-applied when a
        // detune routing exists (else the base set by applyToVoice stands — no per-sample writes).
        if (tActive[(size_t) LFOTarget::OscDetune] || anyOscDetune)
        {
            const double gDetune = tActive[(size_t) LFOTarget::OscDetune]
                                       ? modOffset[(size_t) LFOTarget::OscDetune] * 50.0 : 0.0;
            for (int i = 0; i < 3; ++i)
                oscillators[i].setDetuneAmount(std::clamp(baseDetune[(size_t) i] + gDetune
                                                          + oscOffset.detune[i] * 50.0, 0.0, 100.0));
        }
        // Self-FM feedback: base + GLOBAL ("Alle OSC") offset + this OSC's OWN offset (per-OSC + global).
        if (tActive[(size_t) LFOTarget::OscFeedback] || anyOscFeedback)
        {
            const double gFb = tActive[(size_t) LFOTarget::OscFeedback]
                                   ? modOffset[(size_t) LFOTarget::OscFeedback] * 0.5 : 0.0;
            for (int i = 0; i < 3; ++i)
                oscillators[i].setFeedback(std::clamp(baseFeedback[(size_t) i] + gFb
                                                      + oscOffset.feedback[i] * 0.5, 0.0, 1.0));
        }
        // Unison voice count: base + GLOBAL ("Alle OSC") + per-OSC offset (stepped 1..7, rounded).
        if (tActive[(size_t) LFOTarget::OscVoices] || anyOscVoices)
        {
            const double gVoices = tActive[(size_t) LFOTarget::OscVoices]
                                       ? modOffset[(size_t) LFOTarget::OscVoices] * 3.0 : 0.0;
            for (int i = 0; i < 3; ++i)
                oscillators[i].setUnisonCount(juce::roundToInt(std::clamp(baseVoices[(size_t) i] + gVoices
                                                                          + oscOffset.voices[i] * 3.0, 1.0, 7.0)));
        }

        // Mix oscillators according to the mix mode. When Mix-Mode is disabled (Story 2.4)
        // the OSCs are summed plainly, regardless of the selected mode.
        // Epic 5: A/B are user-selectable operands (0..2 => OSC 1/2/3); the third OSC
        // (o = 3-a-b) is summed plainly. Defaults A=0,B=1,o=2 == the prior fixed OSC1<->OSC2.
        // EVERY oscillator is advanced exactly once per sample in every branch (no phase drift);
        // a==b falls back to plain additive so no oscillator is advanced twice.
        // Per-oscillator AMP (tremolo) gain: 1.0 unless a per-OSC AMP slot is routed to this OSC,
        // in which case the same (1+v)*0.5 map used by the global "Alle OSC" amplitude applies to
        // THIS oscillator's sample before it enters the mix. (Global "Alle OSC" stays post-mix.)
        float oscGain[3];
        for (int i = 0; i < 3; ++i)
            oscGain[i] = oscAmpActive[i] ? std::clamp((1.0f + (float) oscOffset.amp[i]) * 0.5f, 0.0f, 1.0f)
                                         : 1.0f;   // clamp: stacked AMP slots must not boost >1 or invert phase

        const int a = juce::jlimit(0, 2, mixSrcA);
        const int b = juce::jlimit(0, 2, mixSrcB);

        // Mix the generators into the per-channel bus via each generator's PAN (Epic 10). Every
        // generator is still advanced EXACTLY once per sample (no phase drift). For nCh==1 every pan
        // gain is 1.0, so channel[0] == the old mono mixedSample (byte-identical).
        // Auto-pan: re-derive the per-generator pan gains from base pan + modulation this sample.
        // OSC pan = base + GLOBAL ("Alle OSC", modOffset[OscPan]) + this OSC's own offset (oscOffset.pan).
        if (panMod)
        {
            const double gPan = modOffset[(size_t) LFOTarget::OscPan];   // "Alle OSC" global pan
            for (int g = 0; g < kNumPanGenerators; ++g)
            {
                double p = generatorPan[(size_t) g];
                if      (g <= PanOsc3)        p += gPan + oscOffset.pan[(size_t) g];
                else if (g == PanSub)         p += modOffset[(size_t) LFOTarget::SubPan];
                else if (g == PanNoise)       p += modOffset[(size_t) LFOTarget::NoisePan];
                else if (g == PanKarplus)     p += modOffset[(size_t) LFOTarget::KarplusPan];
                else if (g == PanWavetable)   p += modOffset[(size_t) LFOTarget::WavetablePan];
                else if (g == PanSamplerL || g == PanSamplerR)
                                              p += modOffset[(size_t) LFOTarget::SamplerPan];   // both sub-sources move together
                effPan[g] = (float) std::clamp(p, -1.0, 1.0);
                positionToGains(effPan[g], nCh, curGains[g]);
            }
            // HRTF auto-pan: refresh each generator's HRIR kernel a few times per block (every 64
            // samples) — cheap enough, smooth enough, and far below per-sample cost (128-tap reselect).
            if (hrtfMode && (sample & 63) == 0)
                for (int g = 0; g < kNumPanGenerators; ++g) hrtf[(size_t) g].setPanForBlock(effPan[g]);
        }

        float channel[kMaxOutChannels] = {};
        auto addPanned = [&] (float s, int gen)
        {
            if (binauralMode)   // 10.3: per-generator ITD/ILD/head-shadow render to L/R (headphones 3D)
            {
                const auto o = binaural[(size_t) gen].process(s, effPan[gen]);
                channel[0] += o.l; channel[1] += o.r;
            }
            else if (hrtfMode)  // 10.3: per-generator HRIR convolution → measured L/R (Kunstkopf 3D)
            {
                const auto o = hrtf[(size_t) gen].process(s, effPan[gen]);
                channel[0] += o.l; channel[1] += o.r;
            }
            else
                for (int c = 0; c < nCh; ++c) channel[c] += s * curGains[gen][c];
        };

        if (mixModeOn && a != b && mixMode == MixMode::FM)
        {
            // FM: OSC A modulates OSC B's frequency; the third OSC is additive. The modulator (OSC A) is
            // consumed, not emitted → no pan; the carrier is panned by OSC B, the third OSC by its own pan.
            const int o = 3 - a - b;
            float modulator = oscillators[a].nextSample() * oscGain[a];
            double fmOffset = modulator * oscillators[a].getFrequency() * 2.0;
            addPanned(oscillators[b].nextSample(fmOffset) * oscGain[b], PanOsc1 + b);
            addPanned(oscillators[o].nextSample() * oscGain[o],         PanOsc1 + o);
        }
        else if (mixModeOn && a != b && mixMode == MixMode::RingMod)
        {
            // Ring: OSC A × OSC B (product panned by OSC B), + the third OSC (own pan).
            const int o = 3 - a - b;
            float sa = oscillators[a].nextSample() * oscGain[a];
            float sb = oscillators[b].nextSample() * oscGain[b];
            addPanned(sa * sb * 2.0f,                            PanOsc1 + b);
            addPanned(oscillators[o].nextSample() * oscGain[o], PanOsc1 + o);
        }
        else
        {
            for (int i = 0; i < 3; ++i)
                addPanned(oscillators[i].nextSample() * oscGain[i], PanOsc1 + i);
        }

        // Noise + Karplus + wavetable + sub, each panned by its own generator pan.
        addPanned(noise.nextSample(),     PanNoise);
        addPanned(karplus.nextSample(),   PanKarplus);
        addPanned(wavetable.nextSample(), PanWavetable);
        addPanned(subOsc.nextSample(),    PanSub);
        {
            // SAMPLER (12.1): a stereo set renders as TWO panned sub-sources (own placement each);
            // in the single-channel modes it collapses to a level-true (L+R)/2 downmix instead.
            const auto sm = sampler.nextSample();
            if (nCh == 1)
                addPanned(0.5f * (sm.l + sm.r), PanSamplerL);
            else if (sampler.sourceIsStereo())
            {
                addPanned(sm.l, PanSamplerL);
                addPanned(sm.r, PanSamplerR);
            }
            else
                addPanned(sm.l, PanSamplerL);
        }

        // Global "Alle OSC" amplitude (tremolo) — post-mix, same factor on every channel; and the
        // envelope gain (ADSR advanced once above; reused here, not re-advanced — bypass gate when off).
        const float ampGain = tActive[(size_t) LFOTarget::Amplitude]
                                  ? std::clamp((1.0f + (float) modOffset[(size_t) LFOTarget::Amplitude]) * 0.5f, 0.0f, 1.0f)
                                  : 1.0f;
        const float envGain = (adsrOn ? envValue : gateG);

        // Per-channel effect chain: each panned channel runs its OWN strip. nCh==1 → strip 0 only, the
        // exact chain + order as before. Wavefold → amp → filter → formant → envelope → dist → bitcrush
        // → phaser → chorus → delay → reverb → clamp. FX modulation (applyFxMods) is applied per strip.
        for (int ch = 0; ch < nCh; ++ch)
        {
            applyFxMods(strips[(size_t) ch]);
            auto& st = strips[(size_t) ch];
            float x = channel[ch];
            x = st.wavefolder.process(x);
            x *= ampGain;
            x = st.filter.process(x);
            x = st.formant.process(x);
            x *= envGain;
            x = st.distortion.process(x);
            x = st.bitcrusher.process(x);
            x = st.phaser.process(x);
            x = st.chorus.process(x);
            x = st.delay.process(x);
            x = st.reverb.process(x);
            channel[ch] = std::clamp(x, -1.0f, 1.0f);
        }

        // Output write: mono → both channels (today); stereo-pan → L/R (channels beyond nCh reuse the last).
        const int nOut = outputBuffer.getNumChannels();
        if (nCh == 1)
            for (int c = 0; c < nOut; ++c)
                outputBuffer.addSample(c, startSample + sample, channel[0]);
        else
            for (int c = 0; c < nOut; ++c)
                outputBuffer.addSample(c, startSample + sample, channel[std::min(c, nCh - 1)]);

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
    sampler.setLevel(baseSamplerLevel);   // Story 12.1
    filter.setCutoff(baseCutoff);
    filter.setResonance(baseReso);
    formant.vowel = baseVowel;
    wavefolder.drive = baseFold;
    delay.time = baseDelayTime;
    delay.mix = baseDelayMix;
    reverb.mix = baseReverbMix;
    chorus.depth = baseChorusDep;
    distortion.drive = baseDistDrive;
    bitcrusher.mix = baseCrushMix;
    subOsc.setAmplitude(baseSubLevel);
    for (int i = 0; i < 3; ++i) oscillators[i].setDetuneAmount(baseDetune[(size_t) i]);
}
