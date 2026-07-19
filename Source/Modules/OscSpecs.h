#pragma once
#include "ModuleSpec.h"

// OSC 1-3 — indexed factory (like addOsc). FREQ shows the played frequency (freqDisplay) and
// carries the Frequency ring; AMP carries the Amplitude ring. Default freq differs per osc
// (octave stack C4/C5/C3). Body order: WAVE, FREQ, AMP, VOICES, DETUNE, FB. Values verbatim.
namespace Modules
{
    inline ModuleSpec osc (int i)
    {
        static const float defFreq[3] = { 261.63f, 523.25f, 130.81f };
        const juce::String p = "osc" + juce::String (i);
        ModuleSpec m;
        m.id = p; m.helpId = "osc1"; m.title = "OSC " + juce::String (i); m.persistObject = "Osc" + juce::String (i); m.enableParamId = p + "On";
        m.type = rack::ModuleType::Generator; m.zone = rack::Zone::Generators; m.size = rack::SizeClass::W8H1;
        m.params = {
            { p + "On",        "Enabled",      "",       ParamSpec::Kind::Bool },
            { p + "Wave",      "Waveform",     "WAVE",   ParamSpec::Kind::Choice, {}, (i == 1 ? 1.0f : 0.0f), { "Sine", "Sawtooth", "Square", "Triangle" } },   // OSC 1 defaults to Sawtooth (harmonics-rich starting point); OSC 2/3 Sine
            { p + "Freq",      "Frequency",    "FREQ",   ParamSpec::Kind::Float, juce::NormalisableRange<float> (20.0f, 10000.0f, 1.0f, 0.3f), defFreq[i - 1], {}, {}, LFOTarget::Frequency, true },
            { p + "Amp",       "Amplitude",    "AMP",    ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f, {}, {}, LFOTarget::Amplitude },
            { p + "UniVoices", "UnisonVoices", "VOICES", ParamSpec::Kind::Int,   juce::NormalisableRange<float> (1.0f, 7.0f, 1.0f), 1.0f },
            { p + "UniDetune", "UnisonDetune", "DETUNE", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.2f },
            { p + "Feedback",  "Feedback",     "FB",     ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f },
        };
        return m;
    }
}
