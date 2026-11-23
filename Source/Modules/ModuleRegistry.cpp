#include "ModuleRegistry.h"
#include "AllModules.h"

// Bridges the audio-safe declaration (ModuleRegistry.h) to the UI-side module list (AllModules.h):
// only .params is read here, so pulling the UI headers into this one TU is harmless.
namespace Modules
{
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        // One group per module. Group ids are the module ids, which are unique by construction
        // (the rack layout is keyed by them).
        //
        // ORDER: by rack zone (GENERATORS, MODULATION, PROCESSING, VISUALIZATION, MASTER BUS,
        // INPUT), then by title — so a DAW's folder list reads like the rack and not like the
        // project's history (maintainer in Cubase, 2026-10-10: "warum sind die Parameter
        // unsortiert?"). all() keeps its registration order for the preset reader/writer, which
        // walks the specs; the APVTS order is free to differ: VST3 parameter ids are hashes of
        // the id strings (JUCE_FORCE_USE_LEGACY_PARAM_IDS is off), the state tree matches by id,
        // and the processor's per-run snapshots index getParameters() consistently within a run.
        auto mods = all();
        auto byZoneAndTitle = [] (const ModuleSpec& x, const ModuleSpec& y)
        {
            if (x.zone != y.zone) return (int) x.zone < (int) y.zone;
            return x.title.compareIgnoreCase (y.title) < 0;
        };
        std::stable_sort (mods.begin(), mods.end(), byZoneAndTitle);
        // AD-16 (Story 21.5): layer B = every PATCH module once more, ids prefixed "b", titles
        // "B <title>" (so the host names read "B FILTER Cutoff"). Appended after ALL of layer A's
        // and the instrument's groups, in the same order among themselves, so a DAW lists B as
        // a second block instead of interleaving it with A.
        std::vector<ModuleSpec> layerB;
        for (const auto& m : mods)
            if (m.layered)
            {
                ModuleSpec b;
                b.id = kLayerBPrefix + m.id;
                b.title = "B " + m.title;
                b.zone = m.zone;
                b.params = m.params;
                for (auto& p : b.params)
                    p.id = kLayerBPrefix + p.id;
                layerB.push_back (std::move (b));
            }
        for (auto& b : layerB)
            mods.push_back (std::move (b));
        // ...and Cubase lists VST3 units by their NUMERIC id, not by index — and JUCE derives
        // that id from the group id string (hashCode & 0x7fffffff), so a sorted group order
        // alone still showed Cubase a hash-ordered folder list (maintainer 2026-10-11: "OSC 1,
        // SUB, OSC 2, OSC 3"). Give the k-th group an id whose hash lands in the k-th of n
        // ascending bands: "<n>#<module id>", the number searched upwards until it fits. The
        // number goes in FRONT on purpose: juce::String::hashCode is 31*h + c per character, so a
        // varying PREFIX is multiplied through by 31^len(id) and sweeps the whole range, while a
        // varying suffix only nudged the last few digits and never left its narrow stripe —
        // the first cut (suffix) found no fit for most modules and silently fell back to the
        // plain id, so Cubase still showed hash order. ~130 tries at worst, once at construction.
        // A group id names nothing persistent — units carry no state and no project refers to
        // them — so it is free to be ugly.
        const int n = (int) mods.size();
        const juce::int64 band = (juce::int64) 0x7fffffff / (juce::int64) juce::jmax (1, n);
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        for (int k = 0; k < n; ++k)
        {
            const auto& m = mods[(size_t) k];
            const juce::int64 lo = 1 + (juce::int64) k * band;   // 1: never kRootUnitId (0)
            const juce::int64 hi = (juce::int64) (k + 1) * band;
            juce::String gid = m.id;   // last resort only — see the search below
            for (int suffix = 0; suffix < 100000; ++suffix)
            {
                const juce::String candidate = juce::String (suffix) + "#" + m.id;
                const juce::int64 h = (juce::int64) (candidate.hashCode() & 0x7fffffff);
                if (h >= lo && h < hi) { gid = candidate; break; }
            }
            jassert (gid != m.id);   // a band without a fit would put this module out of order in Cubase
            layout.add (makeModuleParameterGroup (gid, m.title, m.params));
        }
        return layout;
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

    // Which modules a pass covers: the preset ROOT (layer A + instrument) or one layer's patch part.
    enum class Pass { Root, LayerOnly, InstrumentOnly };

    static void writeModules (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix,
                              Pass pass, juce::DynamicObject& root)
    {
        for (const auto& m : all())
        {
            if (pass == Pass::LayerOnly && ! m.layered) continue;
            if (pass == Pass::InstrumentOnly && m.layered) continue;
            auto* obj = new juce::DynamicObject();
            for (const auto& p : m.params)
            {
                auto* raw = apvts.getRawParameterValue (prefix + p.id);
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

    static void readModules (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix,
                             Pass pass, const juce::var& root)
    {
        for (const auto& m : all())
        {
            if (pass == Pass::LayerOnly && ! m.layered) continue;
            if (pass == Pass::InstrumentOnly && m.layered) continue;
            const juce::var obj = root[juce::Identifier (m.persistObject)];
            if (! obj.isObject()) continue;
            for (const auto& p : m.params)
            {
                juce::var val = obj[juce::Identifier (p.persistKey)];
                if (val.isVoid() && p.legacyPersistKey.isNotEmpty())
                    val = obj[juce::Identifier (p.legacyPersistKey)];   // renamed key: old presets still load
                if (val.isVoid()) continue;   // missing field => keep factory default
                auto* param = apvts.getParameter (prefix + p.id);
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

    void writeState (juce::AudioProcessorValueTreeState& apvts, juce::DynamicObject& root)
    {
        writeModules (apvts, {}, Pass::Root, root);
    }

    void readState (juce::AudioProcessorValueTreeState& apvts, const juce::var& root)
    {
        readModules (apvts, {}, Pass::Root, root);
    }

    void writeLayerState (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix, juce::DynamicObject& root)
    {
        writeModules (apvts, prefix, Pass::LayerOnly, root);
    }

    void readLayerState (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix, const juce::var& root)
    {
        readModules (apvts, prefix, Pass::LayerOnly, root);
    }

    void writeInstrumentState (juce::AudioProcessorValueTreeState& apvts, juce::DynamicObject& root)
    {
        writeModules (apvts, {}, Pass::InstrumentOnly, root);
    }

    void readInstrumentState (juce::AudioProcessorValueTreeState& apvts, const juce::var& root)
    {
        readModules (apvts, {}, Pass::InstrumentOnly, root);
    }

    const juce::StringArray& layeredParamIds()
    {
        static const juce::StringArray ids = []
        {
            juce::StringArray a;
            for (const auto& m : all())
                if (m.layered)
                    for (const auto& p : m.params)
                        a.add (p.id);
            return a;
        }();
        return ids;
    }

    bool isLayeredParamId (const juce::String& id)
    {
        return layeredParamIds().contains (id);
    }

    bool isLayeredEnableParamId (const juce::String& id)
    {
        static const juce::StringArray ids = []
        {
            juce::StringArray a;
            for (const auto& m : all())
                if (m.layered && m.enableParamId.isNotEmpty())
                    a.add (m.enableParamId);
            return a;
        }();
        return ids.contains (id);
    }
}
