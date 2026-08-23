#include "ModuleRegistry.h"
#include "AllModules.h"

// Bridges the audio-safe declaration (ModuleRegistry.h) to the UI-side module list (AllModules.h):
// only .params is read here, so pulling the UI headers into this one TU is harmless.
namespace Modules
{
    void appendAllParameters (std::vector<std::unique_ptr<juce::RangedAudioParameter>>& out)
    {
        for (const auto& m : all())
            appendModuleParameters (m.params, m.title, out);
    }

    // Parameters live as float32; casting one straight to double drags its binary error into the
    // JSON ("0.6" becomes 0.599999964237213). Writing the SHORTEST decimal that parses back to the
    // very same float32 keeps the file human-readable and diff-friendly without losing a bit —
    // "0.6" and 0.599999964237213 are the identical parameter value after the round trip.
    static double shortestRoundTrip (float v)
    {
        for (int places = 1; places <= 12; ++places)
        {
            const double d = juce::String ((double) v, places).getDoubleValue();
            if ((float) d == v)
                return d;
        }
        return (double) v;
    }

    void writeState (juce::AudioProcessorValueTreeState& apvts, juce::DynamicObject& root)
    {
        for (const auto& m : all())
        {
            auto* obj = new juce::DynamicObject();
            for (const auto& p : m.params)
            {
                auto* raw = apvts.getRawParameterValue (p.id);
                if (raw == nullptr) continue;
                const float v = raw->load();
                switch (p.kind)
                {
                    case ParamSpec::Kind::Bool:
                        obj->setProperty (p.persistKey, v > 0.5f);
                        break;
                    case ParamSpec::Kind::Choice:
                    {
                        const int idx = (int) v;
                        obj->setProperty (p.persistKey, juce::isPositiveAndBelow (idx, p.choices.size()) ? p.choices[idx] : juce::String());
                        break;
                    }
                    default:   // Float / Int
                        obj->setProperty (p.persistKey, shortestRoundTrip (v));
                        break;
                }
            }
            root.setProperty (m.persistObject, juce::var (obj));
        }
    }

    void readState (juce::AudioProcessorValueTreeState& apvts, const juce::var& root)
    {
        for (const auto& m : all())
        {
            const juce::var obj = root[juce::Identifier (m.persistObject)];
            if (! obj.isObject()) continue;
            for (const auto& p : m.params)
            {
                juce::var val = obj[juce::Identifier (p.persistKey)];
                if (val.isVoid() && p.legacyPersistKey.isNotEmpty())
                    val = obj[juce::Identifier (p.legacyPersistKey)];   // renamed key: old presets still load
                if (val.isVoid()) continue;   // missing field => keep factory default
                auto* param = apvts.getParameter (p.id);
                if (param == nullptr) continue;
                float raw;
                switch (p.kind)
                {
                    case ParamSpec::Kind::Bool:
                        raw = ((bool) val) ? 1.0f : 0.0f;
                        break;
                    case ParamSpec::Kind::Choice:
                    {
                        const int idx = p.choices.indexOf (val.toString());
                        if (idx < 0) continue;   // unknown choice => keep default
                        raw = (float) idx;
                        break;
                    }
                    default:   // Float / Int
                        raw = (float) (double) val;
                        break;
                }
                param->setValueNotifyingHost (param->convertTo0to1 (raw));
            }
        }
    }
}
