#pragma once
#include "ModuleSpec.h"

// BASS (Story 20.2, AD-15) — the sequenced bass box beside the patch. One monophonic voice at
// processor level (DSP/BassVoice.h), summed dry into the master bus where PERC is summed, played
// by the STEP SEQ when its BASS switch is on — never by the keyboard, which stays the patch's.
//
// Deliberately SH-101-sized: WAVE, SUB, CUTOFF, RESO, ENV, ENV>CUT, GLIDE, LEVEL, PAN. No second
// OSC trio, no effects of its own, no matrix targets (AD-15 rule 5). Sits next to STEP SEQ and
// PERC in the MODULATION zone — the sequencer row — because it is the sequencer's instrument.
namespace Modules
{
    inline ModuleSpec bass()
    {
        ModuleSpec m;
        m.id = "bass"; m.title = "BASS"; m.persistObject = "Bass"; m.enableParamId = "bassOn";
        m.type = rack::ModuleType::Generator;   // a sound source, like PERC
        m.zone = rack::Zone::Modulation;        // the sequencer row (PERC's reasoning, 2026-08-10)
        m.size = rack::SizeClass::W10H1;        // WAVE (2 slots) + 8 knobs = 10 slots, one row
        m.defaultVisible = false;               // hidden until switched on (stock rack = Init set)
        m.params = {
            { "bassOn",     "Enabled",  "",        ParamSpec::Kind::Bool },
            { "bassWave",   "Waveform", "WAVE",    ParamSpec::Kind::Choice, {}, 0.0f, { "Sawtooth", "Square" } },
            { "bassSub",    "Sub",      "SUB",     ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f },
            // Same scale as the FILTER module's CUTOFF / RESO, so a number means the same thing.
            { "bassCutoff", "Cutoff",   "CUTOFF",  ParamSpec::Kind::Float, juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.3f), 500.0f },
            { "bassReso",   "Reso",     "RESO",    ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.1f, 10.0f, 0.01f), 1.5f },
            // ENV = the filter envelope's decay. One knob, one envelope: a sequenced bass is a
            // pluck, and a second ADSR would be rack space spent on nothing the ear asks for.
            { "bassEnv",    "Env",      "ENV",     ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.005f, 2.0f, 0.001f, 0.4f), 0.25f },
            // ENV>CUT: how far the envelope opens the filter above CUTOFF — 0..4 octaves at a
            // plain step; an accented step adds ACCENT's share on top (STEP SEQ's knob).
            { "bassEnvAmt", "EnvAmt",   "ENV>CUT", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f },
            // GLIDE = the time a SLIDE step takes to reach the next note; a plain step jumps, a
            // TIE retunes at once. 60 ms is the 303-ish value the STEP SEQ used on the patch.
            { "bassGlide",  "Glide",    "GLIDE",   ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 0.5f, 0.001f, 0.5f), 0.06f },
            { "bassLevel",  "Level",    "LEVEL",   ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f },
            { "bassPan",    "Pan",      "PAN",     ParamSpec::Kind::Float, juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f },
        };
        m.params[6].hostName = "Env Amount";   // "ENV>CUT" is a rack caption, not a DAW name
        return m;
    }
}
