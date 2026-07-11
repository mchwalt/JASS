#pragma once
#include <JuceHeader.h>
#include <map>
#include "BinaryData.h"

// Loads the per-module online-help texts (Story 6.1) from the embedded language resources
// (Resources/help_<lang>.json, keyed by module id). Header-only singleton, mirroring the
// WavetableBankStore pattern: parsed once on first use. English ("EN") is the base language
// and the fallback whenever another language lacks an entry.
//
// The texts live OUTSIDE the code (in the JSON resources), so translations are edited there,
// not inline in the descriptors. A module shows its info icon iff has(id) is true.
class HelpTextStore
{
public:
    static HelpTextStore& instance()
    {
        static HelpTextStore s;
        return s;
    }

    // True if the BASE language (EN) has a non-empty entry for this module id.
    bool has (const juce::String& id) const
    {
        auto en = byLang.find ("EN");
        if (en == byLang.end()) return false;
        auto it = en->second.find (id);
        return it != en->second.end() && it->second.isNotEmpty();
    }

    // Description for id in the requested language, falling back to EN, then empty.
    juce::String get (const juce::String& id, const juce::String& lang) const
    {
        if (auto l = byLang.find (lang); l != byLang.end())
            if (auto it = l->second.find (id); it != l->second.end() && it->second.isNotEmpty())
                return it->second;
        if (auto en = byLang.find ("EN"); en != byLang.end())
            if (auto it = en->second.find (id); it != en->second.end())
                return it->second;
        return {};
    }

    bool isLoaded() const { return loaded; }

private:
    HelpTextStore()
    {
        // Filenames map to BinaryData symbols with '.' -> '_'. Add a language = list its
        // resource here (and in CMakeLists.txt's juce_add_binary_data).
        parse ("EN", BinaryData::help_en_json, BinaryData::help_en_jsonSize);
        parse ("DE", BinaryData::help_de_json, BinaryData::help_de_jsonSize);
        loaded = ! byLang.empty();
    }

    void parse (const juce::String& lang, const char* data, int size)
    {
        auto text = juce::String::createStringFromData (data, size);
        auto v = juce::JSON::parse (text);
        if (auto* obj = v.getDynamicObject())
        {
            std::map<juce::String, juce::String> m;
            for (auto& prop : obj->getProperties())
                m[prop.name.toString()] = prop.value.toString();
            byLang[lang] = std::move (m);
        }
        else
        {
            jassertfalse;   // a help resource failed to parse — check the JSON
        }
    }

    std::map<juce::String, std::map<juce::String, juce::String>> byLang;   // lang -> (id -> text)
    bool loaded = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HelpTextStore)
};
