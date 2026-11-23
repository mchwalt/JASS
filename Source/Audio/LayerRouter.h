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
// therefore never hangs a voice. The drone and the step preview go to the focused layer. Each
// layer's ARP / STEP SEQ plays on its own channel (Story 21.7; one per layer since 21.8), routed to
// that layer, not by pitch — an arpeggio crossing the split note stays in its layer. The keys on
// channel 1 are the live hand; a layer whose pattern is running takes none of them (liveBlock), so a
// key plays a layer live OR feeds its pattern. Everything that is not a note (all-notes-off,
// controllers) reaches both.
//
// Audio thread only. No allocation: the two output buffers are the caller's, pre-sized.
class LayerRouter
{
public:
    enum Dest : std::uint8_t { kNone = 0, kA = 1, kB = 2, kBoth = 3 };

    struct Settings
    {
        bool playA         = true;    // the layers that take NEW notes (Single: the selected one,
        bool playB         = false;   // Dual / Split: both)
        bool split         = false;   // with both: false = DUAL (every key to both)
        int  splitNote     = 48;
        bool focusB        = false;   // drone + step preview go to the edited layer
        int  patternChannelA = 14;    // layer A's ARP / STEP SEQ (21.8: one channel per layer)
        int  patternChannelB = 13;    // layer B's
        std::uint8_t liveBlock = kNone;   // layers whose pattern runs: they take no live notes (21.8)
        int  droneChannel  = 16;
        int  auditionChannel = 15;
        int  auditionChannelB = 12;   // 21.8: layer B's STEP SEQ preview — always to B
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

    void reset() { for (auto& ch : held) ch.fill (kNone); }

    // Did a note-on of this note reach a layer and is it still held there? (21.7: a key the
    // pattern swallowed never did — its note-off is swallowed too; a key that played live keeps
    // its note-off, whatever the pattern does meanwhile.)
    bool isRouted (int channel, int note) const
    {
        return held[(size_t) juce::jlimit (1, 16, channel) - 1][(size_t) (note & 127)] != kNone;
    }

private:
    std::uint8_t destForNoteOn (const juce::MidiMessage& m, const Settings& s) const
    {
        const int ch = m.getChannel();
        if (ch == s.droneChannel || ch == s.auditionChannel)
            return s.focusB ? kB : kA;
        if (ch == s.auditionChannelB)
            return kB;
        if (ch == s.patternChannelA) return kA;
        if (ch == s.patternChannelB) return kB;
        std::uint8_t d;
        if (s.playA && s.playB)
            d = ! s.split ? kBoth : (m.getNoteNumber() < s.splitNote ? kB : kA);
        else
            d = s.playB ? kB : kA;
        if (ch == 1) d = (std::uint8_t) (d & ~s.liveBlock);   // the live keys only (as the chord filter); kNone = the pattern's
        return d;
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
