#pragma once
#include <JuceHeader.h>
#include <cmath>
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
// `param` is the layer-A id (or a special target, below); an optional `"layer": "A" | "B"` pins the entry to one layer, without
// it the entry follows the A | B | A+B focus. A malformed entry or an unknown id is skipped with a
// DBG line — the rest of the profile still works; an unparsable file is not offered at all.
//
// Pure and message-thread only: the processor captures raw CC values on the audio thread and hands
// them to resolve() from a message-thread timer (see SynthyProcessor::pollControllers). Nothing in
// here is touched by the audio thread, so it needs no lock. Testable without a device
// (JASS_Render --selftest, controllerChecks).
//
// Story 22.5: `param` may also be a special target instead of a parameter id:
//
//   "@module:1" .. "@module:7"  the n-th control of the selected module's current bank; an optional
//                               "fallback": "<param id>" is what the entry moves while no module is
//                               selected (the fixed 22.1 mapping)
//   "@select"                   turning it right / left selects the next / previous module; while
//                               Shift is held, the next / previous bank. Always relative.
//   "@shift"                    the controller's momentary Shift (CC >= 64 = held); several entries
//                               are the same Shift (a device may use one CC per program)
//   "@tempoFine"                a relative encoder (binary offset 64): TEMPO +-1 BPM per message
//   "@power"                    a button: each press (CC >= 64) switches the selected module on or
//                               off (its enable switch); several entries, one per device program
//
// Story 22.8: a control never writes its position, it moves the value it finds. How is the entry's
// optional "takeover":
//
//   "scale"    (the default, safe for unknown hardware) anything with physical ends, a fader: moved
//              up by a share of its remaining way up, the value rises by the same share of its own
//              remaining way (down likewise) - so both meet at the ends, top = maximum, always
//   "encoder"  an endless knob: what it was turned by is added to the value; its full travel (127
//              steps) is the parameter's full range. At its end it keeps sending 127 / 0 while
//              turned further - that keeps moving the value
//   "jump"     the 22.1 behaviour, an explicit opt-out: the position is the value
//
// The first message of a control (its position is not known yet) moves nothing.
//
// A bank is up to seven controls ("page" is taken by the step pages of STEP SEQ / PERC); which
// bank a control lies in comes with the pushed control (ModuleControl::bank).
class ControllerProfiles
{
public:
    // Channels 12..16 carry JASS's own traffic (drone 16, step previews 15 / 12, pattern channels
    // 14 / 13) — never controller input.
    static constexpr int kFirstReservedChannel = 12;
    static bool isControllerChannel (int channel) noexcept { return channel >= 1 && channel < kFirstReservedChannel; }

    static constexpr const char* kGenericName = "Generic (GM)";   // the fallback when no "match" fits

    static constexpr int kBankSize = 7;                            // controls per bank (knobs 1..7)
    // Shift + selector moves the bank RELATIVELY: this much knob travel (in CC units) is one bank.
    static constexpr int kBankStep = 12;
    // The selector is an endless encoder emulating an absolute value: at an end it keeps sending
    // 127 (or 0) while turned further — a repeated end value counts as this much travel.
    static constexpr int kBankEndTravel = 4;
    // The same for a knob that moves a value ("takeover": "encoder"): one poll tick with the end
    // value repeated moves the value by this many CC units.
    static constexpr int kEncoderEndTravel = 2;

    enum class Pin { Follow, A, B };
    enum class Kind { Param, Module, Select, Shift, TempoFine, Power };   // Param = a fixed parameter id
    enum class Takeover { Scale, Encoder, Jump };                         // Story 22.8, see above
    struct Control
    {
        juce::String name;
        int cc = -1;
        int channel = 0;          // 0 = any
        juce::String param;       // Kind::Param: layer-A id; Kind::Module: the fallback id (may be empty); else empty
        Pin layer = Pin::Follow;
        Kind kind = Kind::Param;
        int moduleSlot = 0;       // Kind::Module: 1..kBankSize
        Takeover takeover = Takeover::Scale;   // Kind::Param / Module: how the control moves the value
    };
    struct Profile
    {
        juce::String name, match;
        std::vector<Control> controls;
        // Selection (and its marker) exists only while the active profile has such an entry.
        bool hasModuleTargets() const noexcept
        {
            for (const auto& c : controls)
                if (c.kind == Kind::Module || c.kind == Kind::Select) return true;
            return false;
        }
    };
    // One control of a selectable module as the rack shows it (pushed by the editor, display order).
    enum class ControlKind { Knob, Combo, Toggle, List };          // List: a combo stepped through its own value list
    struct ModuleControl
    {
        juce::String paramId;     // as the frame on screen uses it (a layer-B frame: the "b" id)
        ControlKind kind = ControlKind::Knob;
        int items = 0;            // Combo: number of items
        // Optional: false = the control is dimmed on screen right now (RATE while SYNC is on a
        // note value) — it keeps its slot, but a knob moves nothing on it. Asked on the message
        // thread when a value arrives, so it follows the module's state without a new push.
        std::function<bool()> active;
        int bank = 0;             // 0-based bank this control lies in ("@module:n" = the n-th of the current bank)
        // List: the item count and the value per item (empty = the index is the value), asked on
        // the message thread when a value arrives — a dependent list is always current.
        std::function<int()> listCount;
        std::function<juce::Array<int>()> listValues;
    };
    // The one conversion from the rack's countable control (rack::CountableControl — a template,
    // so this header needs no rack include). The editor and the selftest both push through it.
    template <class Countable>
    static ModuleControl moduleControlFrom (const Countable& c)
    {
        using K = typename Countable::Kind;
        return { c.paramId,
                 c.kind == K::Combo ? ControlKind::Combo : c.kind == K::Toggle ? ControlKind::Toggle
               : c.kind == K::List  ? ControlKind::List  : ControlKind::Knob,
                 c.items, c.active, c.bank, c.listCount, c.listValues };
    }

    //==============================================================================================
    // Pure value math (Story 22.5).

    // A knob's travel split into `count` equal parts: which part `v01` is in. -1 when count < 1.
    // Used for a combo's item under "takeover": "jump" (the position is the value).
    static int stepIndex (float v01, int count) noexcept
    {
        if (count < 1) return -1;
        return juce::jlimit (0, count - 1, (int) std::floor (juce::jlimit (0.0f, 1.0f, v01) * (float) count));
    }
    static int bankCount (int controls) noexcept { return controls <= 0 ? 0 : (controls + kBankSize - 1) / kBankSize; }
    static bool toggleOn (int ccValue) noexcept { return ccValue >= 64; }
    // A relative encoder in binary offset: 65 = one detent up, 63 = one down, 66 = two up (accelerated).
    static int relativeDelta (int ccValue) noexcept { return ccValue - 64; }
    // What one message counts: its direction only. Turned fast, the MiniLab 3 accelerates to
    // +-2 / +-3 per message (measured 2026-11-27) — a fine control wants one step per message.
    static int relativeStep (int ccValue) noexcept { return ccValue > 64 ? 1 : ccValue < 64 ? -1 : 0; }
    //----------------------------------------------------------------------------------------------
    // Story 22.8: soft takeover. `last` / `now` are CC values 0..127, `last` < 0 = not known yet.

    // What an endless knob was turned by, in CC units. Its first message only says where it stands;
    // at an end it repeats 127 / 0 while turned further, which counts as `endTravel`.
    static int encoderDelta (int last, int now, int endTravel) noexcept
    {
        if (last < 0)    return 0;
        if (now != last) return now - last;
        if (now >= 127)  return endTravel;
        if (now <= 0)    return -endTravel;
        return 0;
    }
    // Value scaling: the control went from fPrev to fNow (0..1) - the value p (0..1) goes the same
    // share of its own remaining way in that direction. Control at an end = value at that end.
    static float scaledValue (float p, float fPrev, float fNow) noexcept
    {
        if (fNow > fPrev)
            return fNow >= 1.0f ? 1.0f : p + (1.0f - p) * (fNow - fPrev) / (1.0f - fPrev);
        if (fNow < fPrev)
            return fNow <= 0.0f ? 0.0f : p * fNow / fPrev;
        return p;
    }
    // A control with ends at rest may flicker by one CC unit, and scaling up and down are not
    // inverse - the value would creep towards the control's position. So a one-unit move against
    // the direction of the control's previous move is no move (the ends always count). `ref` is
    // where the control last really stood (< 0 = not known yet), `dir` its last direction.
    // Returns where to scale from: -1 on the first message, `now` (= nothing moves) for a move
    // that is ignored, else the old reference - which then becomes `now`.
    static int scaleReference (int& ref, int now, int& dir) noexcept
    {
        if (ref < 0) { ref = now; dir = 0; return -1; }
        if (now == ref) return now;
        const int d = now > ref ? 1 : -1;
        if (std::abs (now - ref) == 1 && dir != 0 && d != dir && now > 0 && now < 127)
            return now;
        const int from = ref;
        ref = now;
        dir = d;
        return from;
    }
    // Travel that steps (the module and bank selector, combos, lists, switches): `d` is added to
    // `travel`, every whole `step` of it is one step, the rest stays. Turned back: counted afresh.
    static int takeSteps (float& travel, float d, float step) noexcept
    {
        if ((d > 0.0f && travel < 0.0f) || (d < 0.0f && travel > 0.0f))
            travel = 0.0f;
        travel += d;
        const int steps = (int) (travel / step);   // towards zero
        travel -= (float) steps * step;
        return steps;
    }
    // Where a continuous value p (0..1) goes.
    static float takenOver (Takeover mode, float p, int last, int now) noexcept
    {
        if (mode == Takeover::Jump) return (float) now / 127.0f;
        if (last < 0)               return p;
        if (mode == Takeover::Encoder)
            return juce::jlimit (0.0f, 1.0f, p + (float) encoderDelta (last, now, kEncoderEndTravel) / 127.0f);
        return juce::jlimit (0.0f, 1.0f, scaledValue (p, (float) last / 127.0f, (float) now / 127.0f));
    }
    // Where a stepped value goes: item `index` of `count` (a combo, a list, a parameter with few
    // steps). -1 when there is nothing to pick. An encoder's full travel steps through all items
    // (never more than one item per CC unit); anything with exactly two steps is a switch: a clear
    // turn (kBankStep) right is on, left is off. `travel` is the control's accumulator, in CC units.
    static int steppedIndex (Takeover mode, int index, int count, int last, int now, float& travel) noexcept
    {
        if (count < 1) return -1;
        index = juce::jlimit (0, count - 1, index);
        if (mode == Takeover::Jump)
            return stepIndex ((float) now / 127.0f, count);   // two steps: on from CC 64
        if (last < 0 || (mode == Takeover::Scale && now == last))
            return index;
        const float step = juce::jmax (1.0f, 127.0f / (float) count);
        int steps = 0;
        if (mode == Takeover::Encoder)
            steps = takeSteps (travel, (float) encoderDelta (last, now, kEncoderEndTravel), count == 2 ? (float) kBankStep : step);
        else
        {
            if (now >= 127) return count - 1;
            if (now <= 0)   return 0;
            const float pos = juce::jlimit (0.0f, 1.0f, ((float) index + 0.5f) / (float) count + travel / 127.0f);
            steps = takeSteps (travel, (scaledValue (pos, (float) last / 127.0f, (float) now / 127.0f) - pos) * 127.0f, step);
        }
        return juce::jlimit (0, count - 1, index + steps);
    }

    // TEMPO after `detents` encoder steps: whole BPM, inside lo..hi.
    static double tempoAfterDetents (double bpm, int detents, double lo = 40.0, double hi = 250.0) noexcept
    {
        return juce::jlimit (lo, hi, std::round (bpm) + (double) detents);
    }
    // The MiniLab 3 reports a program change (ARTURIA / DAW / User) as
    // F0 00 20 6B 7F 42 02 00 40 62 xx F7. Raw bytes, safe on the audio thread.
    static bool isProgramChangeSysEx (const juce::uint8* d, int n) noexcept
    {
        static constexpr juce::uint8 head[] = { 0xF0, 0x00, 0x20, 0x6B, 0x7F, 0x42, 0x02, 0x00, 0x40, 0x62 };
        if (d == nullptr || n != 12 || d[11] != 0xF7)
            return false;
        for (int i = 0; i < 10; ++i)
            if (d[i] != head[i]) return false;
        return true;
    }
    static bool matches (const Control& c, int channel, int cc) noexcept
    {
        return c.cc == cc && (c.channel == 0 || c.channel == channel);
    }
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

            // 22.8: how the control moves its value. An unknown word is not fatal: the default.
            const auto takeover = eo->getProperty ("takeover").toString().trim().toLowerCase();
            if      (takeover == "encoder") c.takeover = Takeover::Encoder;
            else if (takeover == "jump")    c.takeover = Takeover::Jump;
            else if (takeover.isNotEmpty() && takeover != "scale")
            { DBG ("Controller profile '" << p.name << "': '" << c.name << "' unknown takeover '" << takeover << "', scale used"); }

            auto known = [&] (const juce::String& id)
            { return id.isNotEmpty() && ! id.startsWithChar ('@') && (paramExists == nullptr || paramExists (id)); };
            if (c.param.startsWithChar ('@'))
            {
                // A special target (22.5) is no parameter id: it is never checked against the layout
                // and never leaves this class as one.
                const auto target = c.param;
                c.param.clear();
                if      (target == "@select")    c.kind = Kind::Select;
                else if (target == "@shift")     c.kind = Kind::Shift;
                else if (target == "@tempoFine") c.kind = Kind::TempoFine;
                else if (target == "@power")     c.kind = Kind::Power;
                else if (target.startsWith ("@module:") && target.length() == 9
                         && target[8] >= '1' && target[8] < '1' + kBankSize)
                {
                    c.kind = Kind::Module;
                    c.moduleSlot = (int) (target[8] - '0');
                    // An unknown fallback drops only the fallback: the knob still follows the module.
                    const auto fb = eo->getProperty ("fallback").toString().trim();
                    if (known (fb))
                        c.param = fb;
                    else if (fb.isNotEmpty())
                    { DBG ("Controller profile '" << p.name << "': '" << c.name << "' unknown fallback '" << fb << "', fallback dropped"); }
                }
                else { DBG ("Controller profile '" << p.name << "': unknown target '" << target << "', skipped"); continue; }
            }
            else if (! known (c.param))
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

    // The fixed parameter one entry moves with the layer focus `focus` (0 A, 1 B, 2 A+B): its
    // `param`, or for an "@module:n" entry its fallback. A layered parameter follows the focus (B:
    // the "b" id; A+B: A's id — linked editing moves B by the same knob distance), unless the entry
    // is pinned; anything else is set by its own id. False = the entry has no fixed parameter
    // ("@select", "@shift", "@tempoFine", "@power", an "@module:n" without fallback).
    static bool fixedTarget (const Control& c, int focus, Target& out,
                             const IdPredicate& isLayered = Modules::isLayeredParamId)
    {
        if ((c.kind != Kind::Param && c.kind != Kind::Module) || c.param.isEmpty())
            return false;
        const juce::String bp (Modules::kLayerBPrefix);
        if (! isLayered (c.param))     out = { c.param, false };
        else if (c.layer == Pin::A)    out = { c.param, true };
        else if (c.layer == Pin::B)    out = { bp + c.param, true };
        else                           out = { focus == 1 ? bp + c.param : c.param, false };
        return true;
    }

    // What CC `cc` on `channel` (1..16) moves while no module is selected — the fixed path. Never
    // returns an "@..." target as a parameter id.
    static std::vector<Target> resolve (const Profile& profile, int channel, int cc, int focus,
                                        const IdPredicate& isLayered = Modules::isLayeredParamId)
    {
        std::vector<Target> out;
        if (! isControllerChannel (channel))
            return out;
        for (const auto& c : profile.controls)
        {
            Target t;
            if (matches (c, channel, cc) && fixedTarget (c, focus, t, isLayered))
                out.push_back (std::move (t));
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
