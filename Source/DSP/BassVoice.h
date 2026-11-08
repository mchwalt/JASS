#pragma once
#include <JuceHeader.h>
#include <cmath>
#include "Oscillator.h"
#include "BiquadFilter.h"
#include "AdsrEnvelope.h"

// BASS (Story 20.2, AD-15) — ONE monophonic sequenced bass voice beside the patch, the PERC way.
//
// JASS is one patch on the keyboard: the STEP SEQ takes the held chord over, so a pad on the
// keyboard and a figure from the same instance were mutually exclusive. AD-15 allows a fixed BOX
// beside the patch if it has its own engine at processor level, no part selector, sums dry into
// the master bus at PERC's point, and is driven by a sequencer, never by the keyboard. This is
// that box for the bass line: an SH-101-sized voice — one oscillator (saw or square), a square sub
// an octave down, one resonant lowpass with a decay envelope, a gate-shaped amp envelope, glide for
// SLIDE steps. Nothing of the patch's chain touches it; "dry" is the construction, as in
// PercSequencer.
//
// Fed by the STEP SEQ's own MIDI output (channel 1: note-on velocity 100 = plain, 127 = accented)
// and its legato events (TIE = retune without a new attack, SLIDE = the same, gliding), plus the
// editor's step preview on `auditionChannel`. Root, latch, quantised entry and the host clock are
// the sequencer's; only the consumer changes. Allocation-free on the audio thread: fixed members,
// parameters are plain fields the processor sets per block (the PercSequencer pattern).
class BassVoice
{
public:
    // Parameters — set per block by the processor from the APVTS (`enabled` through setEnabled).
    int    wave        = 0;       // 0 = Sawtooth, 1 = Square
    double sub         = 0.3;     // level of the square sub-oscillator one octave down
    double cutoff      = 500.0;   // Hz — the filter's rest position (envelope decays back to it)
    double reso        = 1.5;     // biquad Q, the FILTER module's scale
    double envSec      = 0.25;    // filter envelope decay (one time constant)
    double envOct      = 2.0;     // filter envelope depth in octaves at a plain (velocity 100) step
    double level       = 0.5;
    double pan         = 0.0;     // -1 left … +1 right, equal-power
    double accentDepth = 0.5;     // STEP SEQ's ACCENT knob: what velocity 127 adds (gain + envelope)
    int    auditionChannel = 0;   // the editor's preview channel (SynthyProcessor::kAuditionChannel)

    void prepare (double sr)
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        for (auto* o : { &osc, &subOsc })
        {
            o->setSampleRate (sampleRate);
            o->setEnabled (true);
            o->setAmplitude (1.0);
            o->setUnisonCount (1);
            o->setDetuneAmount (0.0);
            o->setFeedback (0.0);
            o->reset();
        }
        subOsc.setWaveform (WaveformType::Square);
        filter.setSampleRate (sampleRate);
        filter.setType (FilterType::Lowpass);
        filter.reset();
        amp.setSampleRate (sampleRate);
        amp.setAttack (0.002);     // a bass note starts now
        amp.setDecay (0.0);
        amp.setSustain (1.0);      // the step's GATE is the note length
        amp.setRelease (0.03);     // short enough for staccato, long enough not to click
        amp.reset();
        glide.reset (sampleRate, 0.0);
        glide.setCurrentAndTargetValue (110.0);
        filtEnv = 0.0; accent = 0.0; seqNote = -1; auditionNote = -1; seqTargetHz = 110.0;
        coeffCountdown = 0;
    }

    // The module's Enable. Off: no new note starts (the sequencer may still feed the box, as a
    // switched-off PERC is still fed by its grid), a sounding note is released so the switch is
    // audible at once, and only the release tail is rendered.
    void setEnabled (bool on)
    {
        if (enabled && ! on) allNotesOff();
        enabled = on;
    }
    bool isEnabled() const noexcept { return enabled; }

    // --- events (from the STEP SEQ's MIDI out, the preview channel, and the legato list) --------
    void noteOn (int midiNote, int velocity, bool audition)
    {
        if (! enabled) return;   // a disabled box starts nothing (review 2026-11-08)
        const double hz = hzOf (midiNote);
        if (audition)
        {
            // A preview rides on top of the sequencer's note: the pitch jumps to the previewed step
            // and comes back when the preview ends, the gate stays as it is if a note is held.
            auditionNote = midiNote;
            if (seqNote < 0) { filtEnv = 1.0; amp.gateOn(); accent = accentOf (velocity); }
        }
        else
        {
            seqNote     = midiNote;
            seqTargetHz = hz;
            auditionNote = -1;          // the figure's own note wins over a preview
            accent  = accentOf (velocity);
            filtEnv = 1.0;              // the envelope restarts with every attack
            amp.gateOn();
        }
        glide.reset (sampleRate, 0.0);  // a plain step jumps; SLIDE glides (slideTo)
        glide.setCurrentAndTargetValue (hz);
    }

    void noteOff (int midiNote, bool audition)
    {
        if (audition)
        {
            if (midiNote != auditionNote) return;
            auditionNote = -1;
            if (seqNote >= 0)
            {
                // Back to the figure's note (wherever its slides have taken it meanwhile).
                glide.reset (sampleRate, 0.0);
                glide.setCurrentAndTargetValue (seqTargetHz);
            }
            else
                amp.gateOff();
        }
        else
        {
            if (midiNote != seqNote) return;
            seqNote = -1;
            if (auditionNote < 0) amp.gateOff();   // a preview still held keeps the gate
        }
    }

    void allNotesOff() { amp.gateOff(); seqNote = -1; auditionNote = -1; }

    // TIE/SLIDE boundary (StepSequencer::Legato): move the sounding pitch by `semitones`, over
    // `seconds` for a SLIDE (the GLIDE knob) or at once for a TIE. The envelope is untouched —
    // the same contract as SynthVoice::slideTo.
    void slideTo (double semitones, double seconds)
    {
        const double factor = std::pow (2.0, semitones / 12.0);
        seqTargetHz *= factor;                    // the figure's pitch, even under a preview
        if (auditionNote >= 0) return;            // the preview holds its pitch; restored on its note-off
        const double target = glide.getTargetValue() * factor;
        const double now    = glide.getCurrentValue();
        glide.reset (sampleRate, seconds > 0.001 ? seconds : 0.0);
        glide.setCurrentAndTargetValue (now);
        glide.setTargetValue (target);
    }

    bool isSounding() const noexcept { return amp.getStage() != AdsrEnvelope::Stage::Idle; }

    // Render ADDITIVELY into the buffer (after the voices, before the bus stages — PERC's point).
    // `events` is this block's MIDI for the box: handled sample-accurately inline. Runs even when
    // the module is off, so a note released at the switch still fades instead of hanging.
    void processBlock (juce::AudioBuffer<float>& buffer, int numSamples, const juce::MidiBuffer& events)
    {
        auto it = events.begin();
        const auto end = events.end();
        const int nCh = buffer.getNumChannels();   // a host with the output bus off hands over 0
        float* outL = nCh > 0 ? buffer.getWritePointer (0) : nullptr;
        float* outR = nCh > 1 ? buffer.getWritePointer (1) : nullptr;
        const double p  = (juce::jlimit (-1.0, 1.0, pan) + 1.0) * juce::MathConstants<double>::pi / 4.0;
        const float gL  = (float) std::cos (p), gR = (float) std::sin (p);
        const double envCoef = std::exp (-1.0 / (juce::jmax (0.001, envSec) * sampleRate));
        osc.setWaveform (wave == 1 ? WaveformType::Square : WaveformType::Sawtooth);
        const float gain = (float) level;

        for (int i = 0; i < numSamples; ++i)
        {
            while (it != end && (*it).samplePosition <= i)
            {
                handle ((*it).getMessage());
                ++it;
            }
            if (outL == nullptr || ! isSounding())
                continue;   // silent (or nowhere to write): nothing to render, no state to advance

            const double f = glide.getNextValue();
            osc.setFrequency (f);
            subOsc.setFrequency (f * 0.5);
            const double x = osc.nextSample() + sub * subOsc.nextSample();

            filtEnv *= envCoef;
            if (coeffCountdown-- <= 0)
            {
                // Coefficients every 16 samples: the envelope is a slow curve, the biquad's
                // trig per sample would cost more than the whole voice.
                coeffCountdown = 15;
                const double oct = envOct * filtEnv * (1.0 + accent);
                filter.setCutoff (juce::jlimit (20.0, 20000.0, cutoff * std::pow (2.0, oct)));
                filter.setResonance (juce::jlimit (0.1, 10.0, reso));
            }
            float y = filter.process ((float) x);
            y *= amp.process() * gain * (float) (1.0 + 0.5 * accent);
            if (outR != nullptr) { outL[i] += y * gL; outR[i] += y * gR; }
            else                   outL[i] += y;
        }
        // Events after the last sample (none by construction, but never leave one unconsumed).
        while (it != end) { handle ((*it).getMessage()); ++it; }
    }

private:
    static double hzOf (int midiNote) { return 440.0 * std::pow (2.0, (midiNote - 69) / 12.0); }
    double accentOf (int velocity) const
    {
        return juce::jlimit (0.0, 1.0, (velocity - 100) / 27.0) * accentDepth;   // 100 = plain, 127 = accented
    }

    void handle (const juce::MidiMessage& m)
    {
        const bool audition = (m.getChannel() == auditionChannel);
        if (m.isNoteOn() && m.getVelocity() > 0)         noteOn (m.getNoteNumber(), (int) m.getVelocity(), audition);
        else if (m.isNoteOff() || m.isNoteOn())          noteOff (m.getNoteNumber(), audition);
        else if (m.isAllNotesOff() || m.isAllSoundOff()) allNotesOff();
    }

    Oscillator   osc, subOsc;
    BiquadFilter filter;
    AdsrEnvelope amp;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> glide;
    bool   enabled    = false;
    double sampleRate = 44100.0;
    double filtEnv    = 0.0;      // 1 at the attack, decays with envSec
    double accent     = 0.0;      // 0..accentDepth, from the note-on velocity
    int    seqNote    = -1;       // the sequencer's sounding note (its note-off names it)
    double seqTargetHz = 110.0;   // ...and its pitch, kept across a preview and its slides
    int    auditionNote = -1;     // the editor's preview note while held, else -1
    int    coeffCountdown = 0;
};
