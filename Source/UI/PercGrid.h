#pragma once
#include <JuceHeader.h>
#include "../Audio/Parameters.h"
#include "../DSP/PercSequencer.h"

// The PERC step field (Story 16.1): 4 lanes x 32 steps, PAINTED rather than built from components.
//
// Why not the module grid every other body uses: a cell there is at least 62 px so a knob reaches
// its standard size, and 128 cells at that width would make the module six rack units tall. A step
// is a box, not a knob — at ~30 px the whole field fits one unit. So this is a Display element (the
// route the on-screen keyboard takes) that draws the switches and hit-tests clicks, writing the
// APVTS parameters directly. No child components: 128 ToggleButtons would cost more in layout and
// repaint than the four rows of rectangles they would draw.
//
// Reading the params back every frame (rather than caching) is what makes a preset load, a host
// automation move and an undo all show up without a notification path of their own.
// What to call the instrument a lane fires (Story 16.1, decision B: the NOTE knob reads out a name
// instead of a number). Three sources, best first:
//   1. the ZONE's own name — the sample's filename stem, kept since 16.1 ("Kick", "SnareRim").
//      Real, per-kit, and right even for a kit mapped to its own keys;
//   2. the General MIDI drum map, for kits whose files are called s01.wav. Every GM-mapped kit —
//      SamsSonor among them — lands here correctly;
//   3. the plain note name, which is also what shows before a kit has finished loading.
// The stored parameter stays the NOTE NUMBER in all three cases. A name is a label, never a value:
// a kit arriving late, or a different kit, must not move a lane to another drum.
namespace PercNames
{
    inline juce::String gmDrum (int note)
    {
        switch (note)
        {
            case 35: return "Kick 2";      case 36: return "Kick";        case 37: return "Rimshot";
            case 38: return "Snare";       case 39: return "Clap";        case 40: return "Snare 2";
            case 41: return "Tom Lo 2";    case 42: return "HH Closed";   case 43: return "Tom Lo";
            case 44: return "HH Pedal";    case 45: return "Tom Mid 2";   case 46: return "HH Open";
            case 47: return "Tom Mid";     case 48: return "Tom Hi 2";    case 49: return "Crash";
            case 50: return "Tom Hi";      case 51: return "Ride";        case 52: return "China";
            case 53: return "Ride Bell";   case 54: return "Tambourine";  case 55: return "Splash";
            case 56: return "Cowbell";     case 57: return "Crash 2";     case 59: return "Ride 2";
            case 60: return "Bongo Hi";    case 61: return "Bongo Lo";    case 62: return "Conga Mute";
            case 63: return "Conga Hi";    case 64: return "Conga Lo";    case 69: return "Cabasa";
            case 70: return "Maracas";     case 75: return "Claves";
            default: return {};
        }
    }

    inline juce::String forNote (const SampleSet* kit, int note)
    {
        // GM first WITHIN the drum octaves (35..81). Inside that range the General MIDI meaning of
        // a key is what virtually every kit follows, and it is the shorter, canonical word: the
        // SamsSonor kick is a file called "KickSamples", which as a lane label says less than
        // "Kick" does. Outside the range the kit's own name is the only thing that can be right.
        if (const auto gm = gmDrum (note); gm.isNotEmpty())
            return gm;
        if (kit != nullptr)
            if (const auto* z = kit->zoneFor (note, 100); z != nullptr && z->name.isNotEmpty())
                return z->name;
        return juce::MidiMessage::getMidiNoteName (note, true, true, 4);   // JASS labels 60 as C4
    }
}

class PercGrid : public juce::Component, private juce::Timer
{
public:
    // playhead: which step is sounding right now (-1 = not running), polled from the processor.
    PercGrid (juce::AudioProcessorValueTreeState& s, std::function<int()> playheadSource,
              std::function<void (int lane)> auditionLane)
        : apvts (s), playhead (std::move (playheadSource)), audition (std::move (auditionLane))
    {
        startTimerHz (30);
    }
    ~PercGrid() override { stopTimer(); }

    // Width of the name column on the left. Four unlabelled rows of boxes were the first thing the
    // maintainer stumbled over ("mir ist unklar, wie ich das programmiere"): a step grid says what
    // is switched on, but not what it plays. Each row now carries the instrument its NOTE knob
    // resolves to, which also ties the knobs below to the rows above.
    int labelWidth() const { return juce::jlimit (60, 130, getWidth() / 9); }

    void paint (juce::Graphics& g) override
    {
        const auto full = getLocalBounds().reduced (2);
        const auto r = full.withTrimmedLeft (labelWidth());
        const float cw = (float) r.getWidth()  / (float) PercSequencer::kMaxSteps;
        const float ch = (float) r.getHeight() / (float) (PercSequencer::kLanes + 1);   // +1 = the number strip

        // Lane names, from the same three sources the NOTE read-out uses.
        {
            const auto* kit = SampleBankStore::instance().getSet (      // 0 = no kit, n = index n-1
                (int) *apvts.getRawParameterValue (Parameters::ID::percKit) - 1);
            // Uppercase and as large as the row allows: at the rack's display-fit scale an 11 px
            // label was unreadable (maintainer 2026-08-11, "so klein, dass man sie nicht lesen
            // kann"). Capitals read better at small sizes than mixed case, and they match the
            // knob captions.
            g.setFont (juce::FontOptions (juce::jlimit (12.0f, 17.0f, ch * 0.42f), juce::Font::bold));
            for (int l = 0; l < PercSequencer::kLanes; ++l)
            {
                const int note = (int) *apvts.getRawParameterValue (Parameters::ID::percNote (l + 1));
                const bool silent = *apvts.getRawParameterValue (Parameters::ID::percLevel (l + 1)) <= 0.001f;
                g.setColour (juce::Colours::white.withAlpha (silent ? 0.3f : 0.8f));
                g.drawText (PercNames::forNote (kit, note).toUpperCase(),
                            juce::Rectangle<float> ((float) full.getX(), r.getY() + (l + 1) * ch,
                                                    (float) labelWidth() - 6.0f, ch),
                            juce::Justification::centredRight, true);
            }
        }

        // Step numbers, and the four-step grouping that makes a bar readable at a glance.
        g.setFont (juce::FontOptions (10.0f));
        const int len = (int) *apvts.getRawParameterValue (Parameters::ID::percLength);
        for (int s = 0; s < PercSequencer::kMaxSteps; ++s)
        {
            const bool inPattern = s < len;
            g.setColour (juce::Colours::white.withAlpha (inPattern ? 0.45f : 0.15f));
            if (s % 4 == 0)
                g.drawText (juce::String (s + 1),
                            juce::Rectangle<float> (r.getX() + s * cw, (float) r.getY(), cw, ch),
                            juce::Justification::centredLeft);
        }

        const int head = playhead ? playhead() : -1;
        for (int l = 0; l < PercSequencer::kLanes; ++l)
            for (int s = 0; s < PercSequencer::kMaxSteps; ++s)
            {
                auto cell = juce::Rectangle<float> (r.getX() + s * cw,
                                                    r.getY() + (l + 1) * ch,
                                                    cw, ch).reduced (1.5f);
                const bool on  = *apvts.getRawParameterValue (Parameters::ID::percStep (l + 1, s + 1)) > 0.5f;
                // Steps beyond LEN are drawn but dimmed: the pattern is still there, it just does
                // not play — the same honesty the greyed-out knobs elsewhere in the rack carry.
                const bool live = s < len;
                const bool beat = (s % 4) == 0;

                g.setColour (on ? juce::Colour (0xff6f86ad).withAlpha (live ? 1.0f : 0.35f)
                                : juce::Colours::white.withAlpha (live ? (beat ? 0.14f : 0.07f) : 0.04f));
                g.fillRoundedRectangle (cell, 2.0f);
                if (s == head && live)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.55f));   // playhead
                    g.drawRoundedRectangle (cell, 2.0f, 1.0f);
                }
            }
    }

    // Left sets a step AND sounds it, right clears one (maintainer 2026-08-11). Two buttons, two
    // meanings — a toggle made every correction a guess about what the click would do, and placing
    // a step without hearing the instrument is the blind writing this module set out to end.
    // Dragging keeps going in the same sense, so a run of sixteen hats is one gesture.
    void mouseDown (const juce::MouseEvent& e) override { lastCell = -1; setAt (e); }
    void mouseDrag (const juce::MouseEvent& e) override { setAt (e); }

private:
    void setAt (const juce::MouseEvent& e)
    {
        const bool value = ! e.mods.isRightButtonDown();
        auto p = e.position;
        const auto r = getLocalBounds().reduced (2).withTrimmedLeft (labelWidth()).toFloat();
        const float cw = r.getWidth()  / (float) PercSequencer::kMaxSteps;
        const float ch = r.getHeight() / (float) (PercSequencer::kLanes + 1);
        const int s = (int) ((p.x - r.getX()) / cw);
        const int l = (int) ((p.y - r.getY()) / ch) - 1;   // row 0 is the number strip
        if (s < 0 || s >= PercSequencer::kMaxSteps || l < 0 || l >= PercSequencer::kLanes)
            return;
        auto* param = apvts.getParameter (Parameters::ID::percStep (l + 1, s + 1));
        if (param == nullptr)
            return;

        // One preview per CELL, not per mouse event — otherwise holding the button still would
        // machine-gun the sample, and a drag would fire it per pixel.
        const int cell = l * PercSequencer::kMaxSteps + s;
        const bool newCell = (cell != lastCell);
        lastCell = cell;

        // Left click on a step that is ALREADY set still sounds it (maintainer 2026-08-11): asking
        // "what is on this one?" is as common as placing it, and the answer costs nothing.
        if (value && newCell && audition)
            audition (l);
        if ((param->getValue() > 0.5f) == value)
            return;   // nothing to write — the preview above has already happened
        param->setValueNotifyingHost (value ? 1.0f : 0.0f);
        repaint();
    }

    void timerCallback() override { repaint(); }

    juce::AudioProcessorValueTreeState& apvts;
    std::function<int()> playhead;
    std::function<void (int lane)> audition;
    int lastCell = -1;   // lane*32+step the pointer last previewed (one sound per cell)
};
