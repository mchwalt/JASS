#pragma once
#include "ModuleSpec.h"

// SAMPLER (Story 12.1) — recordings as a sound source. Params only; the rack body is hand-built
// in the editor (dynamic SET combo + LOAD file action, like WAVETABLE). The SET index references
// the session's SampleBankStore; the selected file NAME is persisted separately by PresetIO
// ("Sampler.File") so presets restore across sessions from %AppData%\JASS\Samples.
namespace Modules
{
    inline ModuleSpec sampler()
    {
        ModuleSpec m;
        m.id = "sampler"; m.title = "SAMPLER"; m.persistObject = "Sampler"; m.enableParamId = "samplerOn";
        m.type = rack::ModuleType::Generator; m.zone = rack::Zone::Generators; m.size = rack::SizeClass::W12H1;
        m.params = {
            { "samplerOn",    "Enabled", "",      ParamSpec::Kind::Bool, {}, 0.0f },
            // SET = index into the session store (dynamic combo in the editor; not shown from spec).
            // Default 2 = "CH_01" in the shipped alphabetical catalog (user pick, 2026-07-30) —
            // keep in step with Samples/ if the catalog changes below index 2.
            { "samplerSet",   "Set",     "",      ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 31.0f, 1.0f), 2.0f, {}, {}, LFOTarget::Off, false, false },
            { "samplerRoot",  "Root",    "ROOT",  ParamSpec::Kind::Int,   juce::NormalisableRange<float> (24.0f, 96.0f, 1.0f), 60.0f },
            { "samplerStart", "Start",   "START", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f },
            { "samplerEnd",   "End",     "END",   ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f },
            { "samplerMode",  "Mode",    "MODE",  ParamSpec::Kind::Choice, {}, 1.0f, { "One-Shot", "Loop", "Reverse", "Rev-Loop" } },   // default Loop (user pick)
            { "samplerLevel", "Level",   "LEVEL", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f, {}, {}, LFOTarget::SamplerLevel },
            { "samplerPan",   "Pan",     "PAN",   ParamSpec::Kind::Float, juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f, {}, {}, LFOTarget::SamplerPan },
            // Playback-speed multiplier on top of the key transposition (tape-style: pitch moves
            // with it). Log-skewed so 1.0 sits centred. Append-only (added after first build).
            { "samplerSpeed", "Speed",   "SPEED", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.25f, 4.0f, 0.01f, 0.5f), 1.0f },
            // Story 12.3: pitch/time DECOUPLING. Off (default) = 12.1 tape-style, bit-identical.
            // On = the key sets only the pitch (signalsmith-stretch, ~60 ms latency) and SPEED
            // sets only the time — every loop voice traverses START..END in the same wall-clock
            // time regardless of pitch, so transposed loops stay beat-locked by construction.
            { "samplerStretch", "Stretch", "STRETCH", ParamSpec::Kind::Bool, {}, 0.0f },
            // Story 12.4: the sampler's OWN note-off fade (seconds to −60 dB). A zone's .sfz
            // ampeg_release wins where present; this knob covers zones without one (folder /
            // single-sample sets). Default 0 = OFF — old presets keep the pre-12.4 behaviour
            // (missing ⇒ default). Append-only (added after first build).
            { "samplerRelease", "Release", "REL", ParamSpec::Kind::Float, juce::NormalisableRange<float> (0.0f, 8.0f, 0.01f, 0.5f), 0.0f },
        };
        return m;
    }
}
