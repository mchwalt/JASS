#pragma once
#include <JuceHeader.h>
#include <vector>
#include <algorithm>

// Step arpeggiator: turns a held chord (channel-1 notes) into an automatic note
// sequence. Driven sample-accurately from processBlock and emits note on/off into
// the synth's MIDI buffer; the raw held notes are filtered out upstream so only
// the arp pattern sounds. Free-running rate in steps/second (a later tempo-sync
// could replace rateHz with a musical division).
class Arpeggiator
{
public:
    enum class Mode { Up = 0, Down, UpDown, Random };

    bool   enabled = false;
    double rateHz  = 8.0;   // steps per second
    Mode   mode    = Mode::Up;
    int    octaves = 1;     // 1..4
    double gate    = 0.6;   // note length as a fraction of the step (0.05..1.0)

    void prepare(double sr) { sampleRate = sr; reset(); }

    void reset()
    {
        sampleCounter = 0;
        seqIndex = 0;
        soundingNote = -1;
        gateCountdown = -1;
    }

    // Copy-assign reuses the vector's capacity → no per-block reallocation.
    void setHeldNotes(const std::vector<int>& notes) { held = notes; }

    // Release any note the arp left sounding (arp switched off / chord released).
    void releaseAll(juce::MidiBuffer& out, int channel, int sampleOffset = 0)
    {
        if (soundingNote >= 0)
        {
            out.addEvent(juce::MidiMessage::noteOff(channel, soundingNote), sampleOffset);
            soundingNote = -1;
        }
        gateCountdown = -1;
    }

    void processBlock(int numSamples, juce::MidiBuffer& out, int channel)
    {
        if (held.empty())
        {
            releaseAll(out, channel);
            sampleCounter = 0;   // start the clock fresh on the next chord
            seqIndex = 0;
            return;
        }

        buildSequence();
        if (sequence.empty()) return;

        const int interval = juce::jmax(1, (int) (sampleRate / juce::jmax(0.1, rateHz)));

        for (int i = 0; i < numSamples; ++i)
        {
            // Gate: release the current note after gate*interval samples.
            if (gateCountdown == 0 && soundingNote >= 0)
            {
                out.addEvent(juce::MidiMessage::noteOff(channel, soundingNote), i);
                soundingNote = -1;
            }
            if (gateCountdown > 0) --gateCountdown;

            if (sampleCounter == 0)
            {
                if (soundingNote >= 0)   // safety: release if the gate hasn't yet
                {
                    out.addEvent(juce::MidiMessage::noteOff(channel, soundingNote), i);
                    soundingNote = -1;
                }
                int note = nextNote();
                if (note >= 0 && note <= 127)
                {
                    out.addEvent(juce::MidiMessage::noteOn(channel, note, (juce::uint8) 100), i);
                    soundingNote = note;
                    gateCountdown = juce::jmax(1, (int) (interval * juce::jlimit(0.05, 1.0, gate)));
                }
            }

            if (++sampleCounter >= interval) sampleCounter = 0;
        }
    }

private:
    void buildSequence()
    {
        sequence.clear();
        std::sort(held.begin(), held.end());
        const int oct = juce::jlimit(1, 4, octaves);

        ascending.clear();
        for (int o = 0; o < oct; ++o)
            for (int n : held)
                if (n + 12 * o <= 127) ascending.push_back(n + 12 * o);
        if (ascending.empty()) return;

        switch (mode)
        {
            case Mode::Up:
                sequence = ascending;
                break;
            case Mode::Down:
                sequence.assign(ascending.rbegin(), ascending.rend());
                break;
            case Mode::UpDown:
                sequence = ascending;
                for (int i = (int) ascending.size() - 2; i >= 1; --i)  // bounce, no doubled ends
                    sequence.push_back(ascending[(size_t) i]);
                break;
            case Mode::Random:
                sequence = ascending;   // index picked randomly in nextNote()
                break;
        }
    }

    int nextNote()
    {
        if (sequence.empty()) return -1;
        if (mode == Mode::Random)
        {
            seqIndex = rng.nextInt((int) sequence.size());
            return sequence[(size_t) seqIndex];
        }
        int note = sequence[(size_t) (seqIndex % (int) sequence.size())];
        seqIndex = (seqIndex + 1) % (int) sequence.size();
        return note;
    }

    std::vector<int> held, sequence, ascending;
    double sampleRate = 44100.0;
    int sampleCounter = 0;
    int seqIndex = 0;
    int soundingNote = -1;
    int gateCountdown = -1;
    juce::Random rng;
};
