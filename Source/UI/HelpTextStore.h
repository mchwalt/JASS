#pragma once
#include <JuceHeader.h>
#include <map>
#include "HelpEN.h"   // embedded EN help resources (namespace HelpEN)
#include "HelpDE.h"   // embedded DE help resources (namespace HelpDE)

// Loads the per-module online-help texts (Story 6.1) from the embedded language resources.
// Each language is a folder of Markdown files — Resources/<LANG>/<moduleId>.md — compiled in
// via its own juce_add_binary_data target/namespace (see CMakeLists.txt). Header-only singleton,
// mirroring the WavetableBankStore pattern: read once on first use. English ("EN") is the base
// language and the fallback whenever another language lacks an entry.
//
// The texts live OUTSIDE the code (as Markdown resources), so translations are edited there — and
// Markdown lets them carry formatting (headings, bold, bullets), rendered by MarkdownRenderer /
// HelpPanel. A module shows its info icon iff has(id) is true. The Markdown filename stem IS the
// module id (osc1.md -> "osc1").
//
// Adding a LANGUAGE: create Resources/<LANG>/*.md, add a juce_add_binary_data target for it in
// CMakeLists.txt, then add one registerLanguage(...) line below.
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

    // Markdown source for id in the requested language, falling back to EN, then empty.
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
    using ResourceFn = const char* (*) (const char*, int&);   // getNamedResource
    using FilenameFn = const char* (*) (const char*);         // getNamedResourceOriginalFilename

    HelpTextStore()
    {
        registerLanguage ("EN", HelpEN::namedResourceList, HelpEN::namedResourceListSize,
                          &HelpEN::getNamedResource, &HelpEN::getNamedResourceOriginalFilename);
        registerLanguage ("DE", HelpDE::namedResourceList, HelpDE::namedResourceListSize,
                          &HelpDE::getNamedResource, &HelpDE::getNamedResourceOriginalFilename);
        loaded = ! byLang.empty();
        jassert (loaded);   // no help resource embedded — check CMakeLists.txt
    }

    // Slurp one language's binary-data namespace into (id -> markdown). The id is the .md
    // filename stem, so it matches the module slug used by has()/get().
    void registerLanguage (const juce::String& lang, const char** names, int count,
                           ResourceFn getResource, FilenameFn getFilename)
    {
        std::map<juce::String, juce::String> m;
        for (int i = 0; i < count; ++i)
        {
            int size = 0;
            const char* data = getResource (names[i], size);
            if (data == nullptr || size <= 0)
                continue;

            const juce::String file = juce::String::fromUTF8 (getFilename (names[i]));   // "osc1.md"
            const juce::String id   = file.upToLastOccurrenceOf (".", false, false);     // "osc1"
            if (id.isEmpty())
                continue;

            m[id] = juce::String::createStringFromData (data, size).trim();
        }

        if (! m.empty())
            byLang[lang] = std::move (m);
    }

    std::map<juce::String, std::map<juce::String, juce::String>> byLang;   // lang -> (id -> markdown)
    bool loaded = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HelpTextStore)
};
