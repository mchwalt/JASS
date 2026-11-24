#pragma once
#include <JuceHeader.h>
#include <functional>
#include <vector>
#include "PresetIO.h"                  // jassFolder()
#include "ControllerProfileFiles.h"    // embedded factory profiles (juce_add_binary_data)
#include "../Modules/ModuleRegistry.h" // isLayeredParamId, kLayerBPrefix

// Story 22.1: controller profiles — which hardware knob / fader (MIDI CC) moves which parameter.
//
// A profile is a JSON file in %AppData%\JASS\Controllers\ (configuration as a file, never code):
//
//   { "name": "MiniLab 3", "match": "Minilab3",
//     "controls": [ { "name": "Knob 1", "cc": 74, "channel": 0, "param": "filterCutoff" }, ... ] }
//
// `channel` 0 = any, 1..11 = that channel only (12..16 are reserved by JASS and never read);
// `param` is the layer-A id; an optional `"layer": "A" | "B"` pins the entry to one layer, without
// it the entry follows the A | B | A+B focus. A malformed entry or an unknown id is skipped with a
// DBG line — the rest of the profile still works; an unparsable file is not offered at all.
//
// Pure and message-thread only: the processor captures raw CC values on the audio thread and hands
// them to resolve() from a message-thread timer (see SynthyProcessor::pollControllers). Nothing in
// here is touched by the audio thread, so it needs no lock. Testable without a device
// (JASS_Render --selftest, controllerChecks).
class ControllerProfiles
{
public:
    // Channels 12..16 carry JASS's own traffic (drone 16, step previews 15 / 12, pattern channels
    // 14 / 13) — never controller input.
    static constexpr int kFirstReservedChannel = 12;
    static bool isControllerChannel (int channel) noexcept { return channel >= 1 && channel < kFirstReservedChannel; }

    static constexpr const char* kGenericName = "Generic (GM)";   // the fallback when no "match" fits

    enum class Pin { Follow, A, B };
    struct Control
    {
        juce::String name;
        int cc = -1;
        int channel = 0;          // 0 = any
        juce::String param;       // layer-A id
        Pin layer = Pin::Follow;
    };
    struct Profile
    {
        juce::String name, match;
        std::vector<Control> controls;
    };
    // One parameter a CC moves. `pinned` = the entry named its layer: in A+B the processor must not
    // mirror it to B (linked editing), the entry wants that one layer only.
    struct Target
    {
        juce::String paramId;
        bool pinned = false;
    };
    using IdPredicate = std::function<bool (const juce::String&)>;

    //==============================================================================================
    // Parse one profile. False = not a profile (no object, no "controls" array) — not offered.
    // `paramExists` decides which ids are known; unknown ones are skipped, not fatal.
    static bool parse (const juce::var& v, const juce::String& fallbackName, const IdPredicate& paramExists, Profile& out)
    {
        auto* obj = v.getDynamicObject();
        if (obj == nullptr)
            return false;
        const auto& controls = obj->getProperty ("controls");
        if (! controls.isArray())
            return false;

        Profile p;
        p.name  = obj->getProperty ("name").toString().trim();
        p.match = obj->getProperty ("match").toString().trim();
        if (p.name.isEmpty())
            p.name = fallbackName;
        if (p.name.isEmpty())
            return false;

        for (const auto& e : *controls.getArray())
        {
            auto* eo = e.getDynamicObject();
            if (eo == nullptr) { DBG ("Controller profile '" << p.name << "': entry is not an object, skipped"); continue; }

            Control c;
            c.name  = eo->getProperty ("name").toString();
            c.param = eo->getProperty ("param").toString().trim();
            const auto& ccVar = eo->getProperty ("cc");
            const auto& chVar = eo->getProperty ("channel");
            auto isNumber = [] (const juce::var& x) { return x.isInt() || x.isInt64() || x.isDouble(); };
            if (! isNumber (ccVar))
            { DBG ("Controller profile '" << p.name << "': '" << c.name << "' has no cc, skipped"); continue; }
            if (! chVar.isVoid() && ! isNumber (chVar))
            { DBG ("Controller profile '" << p.name << "': '" << c.name << "' channel is not a number, skipped"); continue; }
            // Range-check as double first: casting a huge value to int is undefined.
            const double ccD = (double) ccVar, chD = chVar.isVoid() ? 0.0 : (double) chVar;
            if (! (ccD >= 0.0 && ccD < 128.0) || ! (chD >= 0.0 && chD < 17.0))
            { DBG ("Controller profile '" << p.name << "': '" << c.name << "' cc/channel out of range, skipped"); continue; }
            c.cc = (int) ccD;
            c.channel = (int) chD;
            if (c.channel >= kFirstReservedChannel)
            { DBG ("Controller profile '" << p.name << "': '" << c.name << "' reserved channel " << c.channel << ", skipped"); continue; }

            const auto layer = eo->getProperty ("layer").toString().trim().toUpperCase();
            if      (layer.isEmpty()) c.layer = Pin::Follow;
            else if (layer == "A")    c.layer = Pin::A;
            else if (layer == "B")    c.layer = Pin::B;
            else { DBG ("Controller profile '" << p.name << "': '" << c.name << "' layer must be A or B, skipped"); continue; }

            if (c.param.isEmpty() || (paramExists != nullptr && ! paramExists (c.param)))
            { DBG ("Controller profile '" << p.name << "': unknown parameter '" << c.param << "', skipped"); continue; }

            p.controls.push_back (std::move (c));
        }
        out = std::move (p);
        return true;
    }

    static bool parseText (const juce::String& json, const juce::String& fallbackName, const IdPredicate& paramExists, Profile& out)
    {
        juce::var v;
        if (juce::JSON::parse (json, v).failed())
        {
            DBG ("Controller profile '" << fallbackName << "': not valid JSON, not offered");
            return false;
        }
        return parse (v, fallbackName, paramExists, out);
    }

    //==============================================================================================
    // Which profile is active: the hand pick if it names one ("" / "Auto" = automatic); else the
    // first profile whose "match" occurs in an enabled input name (case and spaces ignored, so
    // "MiniLab 3" also finds the port "Minilab3 MIDI"); else Generic (GM). -1 = none at all.
    static int pick (const std::vector<Profile>& profiles, const juce::StringArray& inputNames, const juce::String& handPick)
    {
        auto indexOf = [&] (const juce::String& name)
        {
            for (size_t i = 0; i < profiles.size(); ++i)
                if (profiles[i].name == name) return (int) i;
            return -1;
        };
        if (handPick.isNotEmpty() && handPick != "Auto")
            if (const int i = indexOf (handPick); i >= 0)
                return i;

        auto squash = [] (const juce::String& s) { return s.removeCharacters (" ").toLowerCase(); };
        for (const auto& input : inputNames)
            for (size_t i = 0; i < profiles.size(); ++i)
                if (const auto m = squash (profiles[i].match); m.isNotEmpty() && squash (input).contains (m))
                    return (int) i;
        return indexOf (kGenericName);
    }

    // What CC `cc` on `channel` (1..16) moves, with the layer focus `focus` (0 A, 1 B, 2 A+B).
    // A layered parameter follows the focus (B: the "b" id; A+B: A's id — linked editing moves B by
    // the same knob distance), unless the entry is pinned; anything else is set by its own id.
    static std::vector<Target> resolve (const Profile& profile, int channel, int cc, int focus,
                                        const IdPredicate& isLayered = Modules::isLayeredParamId)
    {
        std::vector<Target> out;
        if (! isControllerChannel (channel))
            return out;
        const juce::String bp (Modules::kLayerBPrefix);
        for (const auto& c : profile.controls)
        {
            if (c.cc != cc || (c.channel != 0 && c.channel != channel))
                continue;
            if (! isLayered (c.param))
                out.push_back ({ c.param, false });
            else if (c.layer == Pin::A)
                out.push_back ({ c.param, true });
            else if (c.layer == Pin::B)
                out.push_back ({ bp + c.param, true });
            else
                out.push_back ({ focus == 1 ? bp + c.param : c.param, false });
        }
        return out;
    }

    //==============================================================================================
    // %AppData%\JASS\Controllers
    static juce::File folder()
    {
        auto dir = PresetIO::jassFolder().getChildFile ("Controllers");
        dir.createDirectory();
        return dir;
    }

    // Copy each shipped profile in if it is missing — an edited file is never overwritten, a
    // deleted one comes back on the next start. Call once at startup.
    static void seedFactoryProfiles()
    {
        auto dir = folder();
        for (int i = 0; i < ControllerProfileFiles::namedResourceListSize; ++i)
        {
            int size = 0;
            const char* data = ControllerProfileFiles::getNamedResource (ControllerProfileFiles::namedResourceList[i], size);
            if (data == nullptr || size <= 0)
                continue;
            auto dest = dir.getChildFile (ControllerProfileFiles::getNamedResourceOriginalFilename (ControllerProfileFiles::namedResourceList[i]));
            if (! dest.existsAsFile())
                dest.replaceWithData (data, (size_t) size);
        }
    }

    // Every *.json in the folder that parses, sorted by name; a second file with a name already
    // taken is skipped (the hand pick and the combo address profiles by name).
    static std::vector<Profile> loadFolder (const IdPredicate& paramExists)
    {
        std::vector<Profile> out;
        auto files = folder().findChildFiles (juce::File::findFiles, false, "*.json");
        files.sort();
        for (const auto& f : files)
        {
            Profile p;
            if (! parseText (f.loadFileAsString(), f.getFileNameWithoutExtension(), paramExists, p))
                continue;
            bool taken = false;
            for (const auto& q : out) taken = taken || q.name == p.name;
            if (taken) { DBG ("Controller profile '" << p.name << "' in " << f.getFileName() << ": name taken, skipped"); continue; }
            out.push_back (std::move (p));
        }
        std::sort (out.begin(), out.end(), [] (const Profile& a, const Profile& b) { return a.name.compareNatural (b.name) < 0; });
        return out;
    }

    // Names + modification times of the folder's files: a change means "load again" (an edit
    // takes effect without a restart).
    static juce::String folderSignature()
    {
        juce::String sig;
        auto files = folder().findChildFiles (juce::File::findFiles, false, "*.json");
        files.sort();
        for (const auto& f : files)
            sig << f.getFileName() << ':' << f.getLastModificationTime().toMilliseconds() << ':' << f.getSize() << ';';
        return sig;
    }
};
