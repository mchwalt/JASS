#pragma once

// ── SINGLE SOURCE OF TRUTH for modulation targets (MOD MATRIX destinations) ──────────────────
// Add a target = ONE line in the table below. Everything is generated from it:
//   • the LFOTarget enum            (this file)
//   • the target count kCount        → ModMatrixConfig::kNumTargets, LiveModFeed size
//   • the persisted string names     → PresetIO kLfoTarget
//   • the DEST combo labels          → ModMatrixSpecs
//   • the target → module-enable map → PluginProcessor auto-enable
// The rack's ModTarget is a type alias for LFOTarget, so knobs/rings share the same vocabulary.
//
// The ONE thing not generated is the per-target DSP APPLY in SynthVoice: each target writes a
// different object/field with its own scale + clamp, so it is inherently object-specific (and
// must stay allocation-free on the audio thread). Adding a target => this table + one apply line.
//
// Dependency-free on purpose (no JUCE, no audio headers) so both the audio and UI layers can
// share it. APPEND-ONLY: new rows go at the END (indices are persisted in presets / DAW state).
//
//                       ENUM,               "PersistName",       "DEST Label",   "enableParamId"
#define JASS_MOD_TARGETS(X)                                                                       \
    X(Off,               "Off",              "Off",          "")                                  \
    X(Frequency,         "Frequency",        "Pitch",        "")                                  \
    X(Amplitude,         "Amplitude",        "Amplitude",    "")                                  \
    X(FilterCutoff,      "FilterCutoff",     "Cutoff",       "filterOn")                          \
    X(WavetablePosition, "WavetablePosition","WT Pos",       "wavetableOn")                       \
    X(FormantVowel,      "FormantVowel",     "Vowel",        "formantOn")                         \
    X(FilterResonance,   "FilterResonance",  "Resonance",    "filterOn")                          \
    X(WavefolderDrive,   "WavefolderDrive",  "Wavefold",     "wavefoldOn")                        \
    X(DelayTime,         "DelayTime",        "Delay Time",   "delayOn")                           \
    X(DelayMix,          "DelayMix",         "Delay Mix",    "delayOn")                           \
    X(ReverbMix,         "ReverbMix",        "Reverb Mix",   "reverbOn")                          \
    X(ChorusDepth,       "ChorusDepth",      "Chorus Depth", "chorusOn")                          \
    X(DistortionDrive,   "DistortionDrive",  "Dist Drive",   "distortionOn")                      \
    X(BitcrushMix,       "BitcrushMix",      "Bitcrush",     "bitcrushOn")                         \
    X(SubLevel,          "SubLevel",         "Sub Level",    "subOn")                             \
    X(OscDetune,         "OscDetune",        "Detune",       "")                                  \
    /* Epic 8.3 — full per-module coverage. Labels/enableId columns are now informational only    \
       (the MOD MATRIX combo + auto-enable are driven by ModMatrixCatalog); persist strings are   \
       used solely by the v4→v5 migration, which never encounters these new targets. Append-only.*/\
    X(WavetableFreq,     "WavetableFreq",    "WT Freq",      "wavetableOn")                       \
    X(WavetableAmp,      "WavetableAmp",     "WT Amp",       "wavetableOn")                       \
    X(FormantReso,       "FormantReso",      "Formant Reso", "formantOn")                         \
    X(FormantMix,        "FormantMix",       "Formant Mix",  "formantOn")                         \
    X(WavefolderSym,     "WavefolderSym",    "Wavefold Sym", "wavefoldOn")                        \
    X(WavefolderMix,     "WavefolderMix",    "Wavefold Mix", "wavefoldOn")                        \
    X(DistortionMix,     "DistortionMix",    "Dist Mix",     "distortionOn")                      \
    X(BitcrushBits,      "BitcrushBits",     "Bitcrush Bits","bitcrushOn")                        \
    X(BitcrushRate,      "BitcrushRate",     "Bitcrush Rate","bitcrushOn")                        \
    X(ChorusRate,        "ChorusRate",       "Chorus Rate",  "chorusOn")                          \
    X(ChorusMix,         "ChorusMix",        "Chorus Mix",   "chorusOn")                          \
    X(DelayFeedback,     "DelayFeedback",    "Delay FB",     "delayOn")                           \
    X(ReverbRoom,        "ReverbRoom",       "Reverb Room",  "reverbOn")                          \
    X(ReverbDamp,        "ReverbDamp",       "Reverb Damp",  "reverbOn")                          \
    X(OscFeedback,       "OscFeedback",      "Feedback",     "")                                  \
    X(OscVoices,         "OscVoices",        "Voices",       "")                                  \
    X(WavetableVoices,   "WavetableVoices",  "WT Voices",    "wavetableOn")                       \
    X(WavetableDetune,   "WavetableDetune",  "WT Detune",    "wavetableOn")

// Off = 0 (slot inactive / no ring). Order == the table above.
enum class LFOTarget
{
   #define X(name, persist, label, enable) name,
    JASS_MOD_TARGETS(X)
   #undef X
};

namespace ModTargets
{
    inline constexpr int kCount =
       #define X(a, b, c, d) + 1
        JASS_MOD_TARGETS(X)
       #undef X
        ;

    // Persisted enum-string name for a target index (canonical; used in .jass presets).
    inline const char* persist (int i)
    {
        static const char* const a[] = {
           #define X(name, persist, label, enable) persist,
            JASS_MOD_TARGETS(X)
           #undef X
        };
        return (i >= 0 && i < kCount) ? a[i] : "";
    }

    // Human label shown in the MOD MATRIX DEST combo.
    inline const char* label (int i)
    {
        static const char* const a[] = {
           #define X(name, persist, label, enable) label,
            JASS_MOD_TARGETS(X)
           #undef X
        };
        return (i >= 0 && i < kCount) ? a[i] : "";
    }

    // APVTS enable-param id of the module a target drives ("" = none / global voice param).
    inline const char* enableId (int i)
    {
        static const char* const a[] = {
           #define X(name, persist, label, enable) enable,
            JASS_MOD_TARGETS(X)
           #undef X
        };
        return (i >= 0 && i < kCount) ? a[i] : "";
    }
}
