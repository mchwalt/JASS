#pragma once
#include <JuceHeader.h>
#include <array>
#include <cstdint>

// MIDI router in front of the two patch engines (AD-16 rule 6, Story 21.5).
//
//   LAYER  every note to A and B.
//   SPLIT  below the split note to B (left hand), from it up to A.
//
// A note-off goes where ITS note-on went — the router remembers, per channel and note, which
// layers a note-on reached. Moving the split point or switching LAYER/SPLIT while keys are held
// therefore never hangs a voice. The drone and the step preview go to the focused layer; while
// the ARP or the STEP SEQ replaces the chord, channel 1 belongs to layer A (the sequencers get
// their A|B target in S3). Everything that is not a note (all-notes-off, controllers) reaches both.
//
// Audio thread only. No allocation: the two output buffers are the caller's, pre-sized.
class LayerRouter
{
public:
    enum Dest : std::uint8_t { kNone = 0, kA = 1, kB = 2, kBoth = 3 };

    struct Settings
    {
        bool layerBOn      = false;
        bool split         = false;   // false = LAYER
        int  splitNote     = 48;
        bool focusB        = false;   // drone + step preview go to the edited layer
        bool patternOwnsCh1 = false;  // ARP / STEP SEQ (not to BASS) plays channel 1 → A
        int  droneChannel  = 16;
        int  auditionChannel = 15;
    };

    // B off: nothing is copied — the caller hands `in` to layer A untouched (bit-exact pre-AD-16
    // path); the router only keeps its note memory current, so a note held while B is switched
    // on still finds its note-off.
    void track (const juce::MidiBuffer& in)
    {
        for (const auto meta : in)
            remember (meta.getMessage(), kA);
    }

    void route (const juce::MidiBuffer& in, juce::MidiBuffer& toA, juce::MidiBuffer& toB, const Settings& s)
    {
        toA.clear();
        toB.clear();
        for (const auto meta : in)
        {
            const auto m  = meta.getMessage();
            const int  sp = meta.samplePosition;
            std::uint8_t d = kBoth;
            if (m.isNoteOn())
            {
                d = destForNoteOn (m, s);
                remember (m, d);
            }
            else if (m.isNoteOff())
            {
                d = slot (m);
                if (d == kNone) d = kA;   // never routed (cannot happen after track/route) — A, as before AD-16
                slot (m) = kNone;
            }
            else
            {
                if (m.isAllNotesOff() || m.isAllSoundOff())
                    forgetChannel (m.getChannel());
            }
            if (d & kA) toA.addEvent (m, sp);
            if (d & kB) toB.addEvent (m, sp);
        }
    }

    // Layer B was switched off (its voices are stopped): no note of it is held any more.
    void forgetLayerB()
    {
        for (auto& ch : held)
            for (auto& d : ch)
                d = (std::uint8_t) (d & ~kB);
    }

    void reset() { for (auto& ch : held) ch.fill (kNone); }

private:
    std::uint8_t destForNoteOn (const juce::MidiMessage& m, const Settings& s) const
    {
        if (! s.layerBOn)
            return kA;
        const int ch = m.getChannel();
        if (ch == s.droneChannel || ch == s.auditionChannel)
            return s.focusB ? kB : kA;
        if (ch == 1 && s.patternOwnsCh1)
            return kA;
        if (! s.split)
            return kBoth;
        return m.getNoteNumber() < s.splitNote ? kB : kA;
    }

    void remember (const juce::MidiMessage& m, std::uint8_t d)
    {
        if (m.isNoteOn())        slot (m) = (std::uint8_t) (slot (m) | d);
        else if (m.isNoteOff())  slot (m) = kNone;
        else if (m.isAllNotesOff() || m.isAllSoundOff()) forgetChannel (m.getChannel());
    }

    std::uint8_t& slot (const juce::MidiMessage& m)
    {
        return held[(size_t) juce::jlimit (1, 16, m.getChannel()) - 1][(size_t) (m.getNoteNumber() & 127)];
    }

    void forgetChannel (int channel)
    {
        held[(size_t) juce::jlimit (1, 16, channel) - 1].fill (kNone);
    }

    std::array<std::array<std::uint8_t, 128>, 16> held {};
};
