#pragma once
#include "ModTargets.h"   // LFOTarget = the flat DSP-apply vocabulary

// ── MOD MATRIX destination catalog: MODULE → PARAMETER ───────────────────────────────────────
// The matrix DEST is chosen in two steps: first the MODULE (MOD combo), then the PARAMETER within
// it (PARAM combo). This layer sits ON TOP of the flat LFOTarget vocabulary (ModTargets.h): every
// (module, param) pair resolves to exactly ONE LFOTarget for the per-sample DSP apply, plus an
// oscIndex for the per-oscillator targets (Pitch/Amplitude/Detune on OSC 1/2/3).
//
//   • oscIndex 0..2  → per-oscillator: only that oscillator is modulated (the reported bug fix).
//   • oscIndex  -1   → global / not osc-scoped: the target drives its single object as before.
//     "Alle OSC" is oscIndex -1 too: it reproduces the OLD global Pitch/Amplitude/Detune behaviour.
//
// APPEND-ONLY: the module order and each module's param order are persisted (modSlotModule label /
// modSlotParam index). New modules/params go at the END; existing entries never move.
//
// Dependency-free (no JUCE) so the audio, UI and persistence layers all share this one table.

namespace ModDest
{
    inline constexpr int kMaxParams = 7;   // most params any one module exposes (WAVETABLE: AMP/DETUNE/FREQ/POS/VOICES/PAN/FB)

    struct Param  { const char* label; LFOTarget target; };
    struct Module
    {
        const char* label;      // MOD combo entry (also the persisted module name)
        const char* enableId;   // APVTS enable-param of this module ("" = none → no auto-enable)
        int         oscIndex;   // 0..2 => per-oscillator target; -1 => global / not osc-scoped
        Param       params[kMaxParams];
        int         numParams;
    };

    // index 0 == "Off" => slot inactive (mirrors the old target==0 convention).
    inline const Module modules[] =
    {
        // Modules are sorted A→Z (Off pinned first); each module's params are sorted A→Z by label.
        // The MOD combo (ComboBoxAttachment) and PARAM combo (indexIsValue) follow this order, so the
        // lists show alphabetically with ascending indices. Reordering changes the persisted param
        // INT, so a v5→v6 migration remaps saved presets (PresetIO::migrateV5ParamOrder). OSC 1/2/3
        // are PER-OSCILLATOR (oscIndex 0..2); "Alle OSC" (-1) is the classic global variant.
        { "Off",        "",             -1, { { "-",      LFOTarget::Off } }, 1 },
        { "Alle OSC",   "",             -1, { { "AMP",    LFOTarget::Amplitude }, { "DETUNE", LFOTarget::OscDetune }, { "FB", LFOTarget::OscFeedback }, { "FREQ", LFOTarget::Frequency }, { "VOICES", LFOTarget::OscVoices }, { "PAN", LFOTarget::OscPan } }, 6 },
        { "BITCRUSH",   "bitcrushOn",   -1, { { "BITS",   LFOTarget::BitcrushBits }, { "MIX", LFOTarget::BitcrushMix }, { "RATE", LFOTarget::BitcrushRate } }, 3 },
        { "CHORUS",     "chorusOn",     -1, { { "DEPTH",  LFOTarget::ChorusDepth }, { "MIX", LFOTarget::ChorusMix }, { "RATE", LFOTarget::ChorusRate } }, 3 },
        { "DELAY",      "delayOn",      -1, { { "FB",     LFOTarget::DelayFeedback }, { "MIX", LFOTarget::DelayMix }, { "TIME", LFOTarget::DelayTime } }, 3 },
        { "DISTORTION", "distortionOn", -1, { { "DRIVE",  LFOTarget::DistortionDrive }, { "MIX", LFOTarget::DistortionMix } }, 2 },
        { "FILTER",     "filterOn",     -1, { { "CUTOFF", LFOTarget::FilterCutoff }, { "RESO", LFOTarget::FilterResonance } }, 2 },
        { "FORMANT",    "formantOn",    -1, { { "MIX",    LFOTarget::FormantMix }, { "RESO", LFOTarget::FormantReso }, { "VOWEL", LFOTarget::FormantVowel } }, 3 },
        { "OSC 1",      "osc1On",        0, { { "AMP",    LFOTarget::Amplitude }, { "DETUNE", LFOTarget::OscDetune }, { "FB", LFOTarget::OscFeedback }, { "FREQ", LFOTarget::Frequency }, { "VOICES", LFOTarget::OscVoices }, { "PAN", LFOTarget::OscPan } }, 6 },
        { "OSC 2",      "osc2On",        1, { { "AMP",    LFOTarget::Amplitude }, { "DETUNE", LFOTarget::OscDetune }, { "FB", LFOTarget::OscFeedback }, { "FREQ", LFOTarget::Frequency }, { "VOICES", LFOTarget::OscVoices }, { "PAN", LFOTarget::OscPan } }, 6 },
        { "OSC 3",      "osc3On",        2, { { "AMP",    LFOTarget::Amplitude }, { "DETUNE", LFOTarget::OscDetune }, { "FB", LFOTarget::OscFeedback }, { "FREQ", LFOTarget::Frequency }, { "VOICES", LFOTarget::OscVoices }, { "PAN", LFOTarget::OscPan } }, 6 },
        { "PHASER",     "phaserOn",     -1, { { "DEPTH",  LFOTarget::PhaserDepth }, { "FB", LFOTarget::PhaserFeedback }, { "MIX", LFOTarget::PhaserMix }, { "RATE", LFOTarget::PhaserRate } }, 4 },
        { "REVERB",     "reverbOn",     -1, { { "DAMP",   LFOTarget::ReverbDamp }, { "MIX", LFOTarget::ReverbMix }, { "ROOM", LFOTarget::ReverbRoom } }, 3 },
        { "SUB",        "subOn",        -1, { { "AMP",    LFOTarget::SubLevel }, { "PAN", LFOTarget::SubPan }, { "FB", LFOTarget::SubFeedback } }, 3 },   // label AMP (generator standard); param INDEX unchanged, so presets are safe
        { "WAVEFOLD",   "wavefoldOn",   -1, { { "DRIVE",  LFOTarget::WavefolderDrive }, { "MIX", LFOTarget::WavefolderMix }, { "SYM", LFOTarget::WavefolderSym } }, 3 },
        { "WAVETABLE",  "wavetableOn",  -1, { { "AMP",    LFOTarget::WavetableAmp }, { "DETUNE", LFOTarget::WavetableDetune }, { "FREQ", LFOTarget::WavetableFreq }, { "POS", LFOTarget::WavetablePosition }, { "VOICES", LFOTarget::WavetableVoices }, { "PAN", LFOTarget::WavetablePan }, { "FB", LFOTarget::WavetableFeedback } }, 7 },
        // APPENDED (2026-07-26): previously-missing modules. They go at the END so the persisted MOD
        // combo index of every module above is unchanged (no preset/DAW migration). The combo is thus
        // no longer strictly A→Z for these six — the append-only contract wins over display ordering.
        // COMPRESSOR/MASTER/STEREO are GLOBAL master-bus stages: their targets are applied in
        // PluginProcessor::processBlock (block-rate, LFO sources only), not per voice. NOISE/KARPLUS/
        // PITCH ENV are per-voice like the rest. KARPLUS omits FREQ (pitch is fixed at pluck time).
        { "COMPRESSOR", "compOn",       -1, { { "ATK",    LFOTarget::CompAttack }, { "GAIN", LFOTarget::CompMakeup }, { "RATIO", LFOTarget::CompRatio }, { "REL", LFOTarget::CompRelease }, { "THRESH", LFOTarget::CompThreshold } }, 5 },
        { "KARPLUS",    "karplusOn",    -1, { { "AMP",    LFOTarget::KarplusAmp }, { "DAMP", LFOTarget::KarplusDamping }, { "STR", LFOTarget::KarplusStretch }, { "PAN", LFOTarget::KarplusPan } }, 4 },
        { "MASTER",     "masterOn",     -1, { { "TEMPO",  LFOTarget::MasterTempo }, { "VOL", LFOTarget::MasterVol } }, 2 },
        { "NOISE",      "noiseOn",      -1, { { "AMP",    LFOTarget::NoiseLevel }, { "PAN", LFOTarget::NoisePan } }, 2 },
        { "PITCH ENV",  "pitchEnvOn",   -1, { { "AMOUNT", LFOTarget::PitchEnvAmount } }, 1 },
        { "STEREO",     "stereoOn",     -1, { { "TIME",   LFOTarget::StereoTime }, { "WIDTH", LFOTarget::StereoWidth } }, 2 },
        // APPENDED (Story 12.1): SAMPLER — per-voice like NOISE/KARPLUS.
        { "SAMPLER",    "samplerOn",    -1, { { "AMP",    LFOTarget::SamplerLevel }, { "PAN", LFOTarget::SamplerPan } }, 2 },   // label AMP (generator standard); param INDEX unchanged, so presets are safe
    };

    inline constexpr int kNumModules = (int) (sizeof (modules) / sizeof (modules[0]));

    inline int clampModule (int m) noexcept { return (m < 0 || m >= kNumModules) ? 0 : m; }
    inline int clampParam  (int m, int p) noexcept
    {
        const Module& M = modules[clampModule (m)];
        return (p < 0 || p >= M.numParams) ? 0 : p;
    }

    inline const Module& moduleAt (int m) noexcept          { return modules[clampModule (m)]; }
    inline int         numParams  (int m) noexcept          { return modules[clampModule (m)].numParams; }
    inline int         oscIndexOf (int m) noexcept          { return modules[clampModule (m)].oscIndex; }
    inline const char* enableIdOf (int m) noexcept          { return modules[clampModule (m)].enableId; }
    inline const char* moduleLabel(int m) noexcept          { return modules[clampModule (m)].label; }
    inline const char* paramLabel (int m, int p) noexcept   { return modules[clampModule (m)].params[clampParam (m, p)].label; }

    // Resolve a (module,param) selection to its DSP target (LFOTarget). Off => LFOTarget::Off.
    inline LFOTarget targetOf (int m, int p) noexcept { return modules[clampModule (m)].params[clampParam (m, p)].target; }

    inline constexpr int kOscRingSlots = 6;   // per-OSC ring slots: FREQ, AMP, DETUNE, FB, VOICES, PAN

    // Per-oscillator ring slot for the OSC-scoped targets (FREQ=0..VOICES=4); -1 else.
    // Used by the live-ring feed so a per-OSC routing lights ONLY that oscillator's knob.
    inline int oscParamSlot (LFOTarget t) noexcept
    {
        switch (t)
        {
            case LFOTarget::Frequency:   return 0;
            case LFOTarget::Amplitude:   return 1;
            case LFOTarget::OscDetune:   return 2;
            case LFOTarget::OscFeedback: return 3;
            case LFOTarget::OscVoices:   return 4;
            case LFOTarget::OscPan:      return 5;
            default:                     return -1;
        }
    }

    // Module index for a persisted module-name string ("OSC 2" → 2). -1 if unknown.
    inline int moduleIndexForLabel (const char* name) noexcept
    {
        for (int i = 0; i < kNumModules; ++i)
        {
            const char* a = modules[i].label; const char* b = name;
            while (*a && *b && *a == *b) { ++a; ++b; }
            if (*a == 0 && *b == 0) return i;
        }
        return -1;
    }

    // Reverse map used by the v4→v5 preset migration: an OLD flat LFOTarget → (module, param).
    // The three OSC-global targets (Frequency/Amplitude/OscDetune) map to "Alle OSC" so old
    // presets keep their global behaviour; every other target maps to its owning module.
    struct ModParam { int module; int param; };
    inline ModParam fromLegacyTarget (int legacyTarget) noexcept
    {
        const auto t = (LFOTarget) legacyTarget;
        if (t == LFOTarget::Frequency || t == LFOTarget::Amplitude || t == LFOTarget::OscDetune)
        {
            const int alleOsc = moduleIndexForLabel ("Alle OSC");
            const Module& M = modules[clampModule (alleOsc)];
            for (int p = 0; p < M.numParams; ++p)
                if (M.params[p].target == t) return { alleOsc, p };
        }
        for (int i = 0; i < kNumModules; ++i)
            for (int p = 0; p < modules[i].numParams; ++p)
                if (modules[i].params[p].target == t) return { i, p };
        return { 0, 0 };   // Off
    }
}
