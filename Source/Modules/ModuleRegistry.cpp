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
                        obj->setProperty (p.persistKey, (double) v);
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
                const juce::var val = obj[juce::Identifier (p.persistKey)];
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
