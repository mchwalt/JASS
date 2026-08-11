#pragma once
#include <JuceHeader.h>
#include <array>
#include "Parameters.h"
#include "../Modules/ModuleRegistry.h"   // spec-driven nested read/write (writeState/readState)
#include "DemoPresets.h"   // embedded shipped demo presets (juce_add_binary_data)
#include "Wavetables.h"    // embedded shipped example wavetables (juce_add_binary_data)
#include "Samples.h"       // embedded shipped SAMPLER examples (Story 12.1)

// Reads/writes JASS's ".jass" JSON preset format (nested-per-module). The C# compatibility
// (shared "%AppData%\Synthy" folder + ".synthy" extension) was dropped; migrateLegacyAppData()
// performs a one-time rebrand of any existing Synthy folder to JASS. Enum strings are still the
// canonical (no-space) names the old format used, so old presets keep loading after the rename.
namespace PresetIO
{
    inline const juce::StringArray kWaveform   { "Sine", "Sawtooth", "Square", "Triangle" };
    inline const juce::StringArray kMixMode    { "Additive", "RingMod", "FM" };
    inline const juce::StringArray kMixSrc     { "OSC 1", "OSC 2", "OSC 3" };   // Epic 5
    inline const juce::StringArray kFilterType { "Off", "Lowpass", "Highpass" };
    inline const juce::StringArray kDistortion { "Off", "SoftClip", "HardClip", "Foldback" };
    inline const juce::StringArray kLfoWave    { "Sine", "Triangle", "Square", "Sawtooth" };
    // Persisted target names — generated from the single source (ModTargets.h). Index == LFOTarget.
    inline const juce::StringArray kLfoTarget = [] { juce::StringArray a;
        for (int i = 0; i < ModTargets::kCount; ++i) a.add (ModTargets::persist (i)); return a; }();
    inline const juce::StringArray kNoiseType  { "Off", "White", "Pink", "Brown", "Blue" };
    inline const juce::StringArray kSubWave     { "Sine", "Square" };
    inline const juce::StringArray kArpMode     { "Up", "Down", "UpDown", "Random" };
    inline const juce::StringArray kPhaserType  { "Phaser", "Flanger" };   // Feature 2 (append-only; C# ignores)
    inline const juce::StringArray kGlideMode   { "Mono", "Poly" };        // Feature 4 (append-only; C# ignores)
    inline const juce::StringArray kModSource   { "LFO1", "Envelope", "Velocity", "LFO2", "LFO3", "LFO4" };   // Epic 8 (append-only)
    // Mod-matrix MODULE names — generated from the single source (ModMatrixCatalog.h). Index == ModDest.
    inline const juce::StringArray kModModule = [] { juce::StringArray a;
        for (int i = 0; i < ModDest::kNumModules; ++i) a.add (ModDest::moduleLabel (i)); return a; }();
    // kLfoTarget (above) is still used to READ the legacy v4 "SlotNTarget" strings during migration.

    // Bumped to 2 in the layout era (Story 4.3: RackLayout added). Loading is version-tolerant:
    // applyVar always factory-resets first, so older files (v1 / no version) load safely and
    // missing fields fall back to factory. The number is for future *value* migrations.
    constexpr int kFormatVersion = 6;   // v6 = MOD MATRIX modules + params sorted A→Z (param INT remapped).
                                        // v5 = MOD MATRIX DEST split into MODULE + PARAM (per-OSC targets).
                                        // v4 = LFO built-in target folded into matrix slots.
                                        // v3 = nested-per-module. v<3 = flat (legacy).

    // App-data root: %AppData%\Roaming\JASS (renamed from "Synthy" after the C# break).
    inline juce::File jassFolder()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("JASS");
        dir.createDirectory();
        return dir;
    }

    // One-time rebrand: move a legacy %AppData%\Synthy tree to %AppData%\JASS, renaming every
    // *.synthy to *.jass (LiveState, presets, backups, sub-folders). Runs ONLY when JASS does
    // not exist yet and Synthy does. MUST be called first at startup, before any jassFolder()
    // use (jassFolder() creates the dir, which would suppress the migration). The old folder is
    // left untouched as a safety copy.
    inline void migrateLegacyAppData()
    {
        auto root   = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
        auto oldDir = root.getChildFile("Synthy");
        auto newDir = root.getChildFile("JASS");
        if (newDir.exists() || ! oldDir.isDirectory())
            return;   // already on JASS, or nothing to migrate

        newDir.createDirectory();
        std::function<void(const juce::File&, const juce::File&)> copyTree =
            [&] (const juce::File& src, const juce::File& dst)
        {
            for (const auto& child : src.findChildFiles(juce::File::findFilesAndDirectories, false))
            {
                if (child.isDirectory())
                {
                    auto sub = dst.getChildFile(child.getFileName());
                    sub.createDirectory();
                    copyTree(child, sub);
                }
                else
                {
                    auto name = child.hasFileExtension("synthy")
                                    ? child.getFileNameWithoutExtension() + ".jass"
                                    : child.getFileName();
                    child.copyFileTo(dst.getChildFile(name));
                }
            }
        };
        copyTree(oldDir, newDir);
    }

    // Named presets folder: %AppData%\Roaming\JASS\Presets.
    inline juce::File presetsFolder()
    {
        auto dir = jassFolder().getChildFile("Presets");
        dir.createDirectory();
        return dir;
    }

    // Live state: auto-loaded on start, auto-saved on change — carries the working patch across
    // launches (also used for A/B). %AppData%\Roaming\JASS\LiveState.jass.
    inline juce::File liveStateFile()
    {
        return jassFolder().getChildFile("LiveState.jass");
    }

    // ---- Preset quick-access bank (F1..F12) --------------------------------------------------
    // The 12 slot->preset assignments are a GLOBAL app setting (like the UI language / 3D-title
    // toggle), NOT part of any preset — so they live in their own tiny JSON file, independent of
    // the loaded patch. Each slot stores a preset NAME (without extension); the loader resolves it
    // to presetsFolder()/<name>.jass at trigger time (robust if the folder moves). Empty = unassigned.
    inline constexpr int kNumPresetSlots = 12;

    // %AppData%\Roaming\JASS\PresetBanks.json
    inline juce::File presetBanksFile()
    {
        return jassFolder().getChildFile("PresetBanks.json");
    }

    // Factory-default bank: the four shipped demo presets pre-assigned to F1..F4 (rest empty).
    // RESET restores this and first run seeds it. Names MUST match the DemoPresets/*.jass basenames
    // (they are seeded into the Presets folder on first run by seedDemoPresets).
    inline std::array<juce::String, kNumPresetSlots> defaultPresetBank()
    {
        std::array<juce::String, kNumPresetSlots> slots;   // default-constructed (empty strings)
        slots[0] = "Matrix Demo";
        slots[1] = "Matrix Demo 2";
        slots[2] = "FX Motion";
        slots[3] = "Helikopter";
        slots[4] = "Matrix Showcase";   // full MOD MATRIX demo (per-OSC + full coverage + 8 slots)
        slots[5] = "Kopfkino";          // Kunstkopf/ROOM showcase (Story 10.4): plucks circling the head
        slots[6] = "Sampler Demo";      // SAMPLER showcase (Story 12.1, user-authored)
        slots[7] = "GrandPiano";        // the plain instrument: SplendidPiano set, nothing else on
        slots[8] = "DAF Bass";          // STEP SEQ showcase (Story 15.1): the measured 16-step figure
        slots[9] = "Drum Pattern";      // STEP SEQ with rests, driving the SamsSonor drum map
        slots[10] = "DAF Beat";         // 16.1: the same bass with PERC underneath it
        return slots;
    }

    // Load the 12 slot assignments. Missing file => factory default (demos on F1..F4); an existing
    // file is honoured as-is (a short array leaves the rest empty).
    inline std::array<juce::String, kNumPresetSlots> loadPresetBanks()
    {
        if (! presetBanksFile().existsAsFile())
            return defaultPresetBank();   // first run: pre-assign the demo presets

        std::array<juce::String, kNumPresetSlots> slots;   // default-constructed (empty strings)
        auto v = juce::JSON::parse(presetBanksFile().loadFileAsString());
        if (auto* arr = v.getArray())
            for (int i = 0; i < juce::jmin((int) arr->size(), kNumPresetSlots); ++i)
                slots[(size_t) i] = (*arr)[i].toString();
        return slots;
    }

    // Persist the 12 slot assignments as a JSON array of strings.
    inline void savePresetBanks(const std::array<juce::String, kNumPresetSlots>& slots)
    {
        juce::Array<juce::var> arr;
        for (const auto& s : slots)
            arr.add(s);
        presetBanksFile().replaceWithText(juce::JSON::toString(juce::var(arr), false));
    }

    // First-run seeding: write each SHIPPED demo preset (embedded from DemoPresets/*.jass via
    // juce_add_binary_data) into the user's Presets folder if it isn't already there. Idempotent —
    // an existing file (incl. one the user edited) is never overwritten. Call once at startup.
    inline void seedDemoPresets()
    {
        auto dir = presetsFolder();
        for (int i = 0; i < DemoPresets::namedResourceListSize; ++i)
        {
            int size = 0;
            const char* data = DemoPresets::getNamedResource(DemoPresets::namedResourceList[i], size);
            if (data == nullptr || size <= 0)
                continue;
            auto dest = dir.getChildFile(DemoPresets::getNamedResourceOriginalFilename(DemoPresets::namedResourceList[i]));
            if (! dest.existsAsFile())
                dest.replaceWithData(data, (size_t) size);
        }
    }

    // Example wavetables live in %AppData%\Roaming\JASS\Wavetables — the LOAD WAV dialog opens here.
    inline juce::File wavetablesFolder()
    {
        auto dir = jassFolder().getChildFile("Wavetables");
        dir.createDirectory();
        return dir;
    }

    // SAMPLER user samples (Story 12.1): LOAD copies files here so presets can re-resolve them by
    // name across sessions ("Sampler.File").
    inline juce::File samplesFolder()
    {
        auto dir = jassFolder().getChildFile("Samples");
        dir.createDirectory();
        return dir;
    }

    // First-run seeding of the shipped SAMPLER examples (embedded from Samples/*.wav + *.sfz)
    // into %AppData%\JASS\Samples. Same idempotent pattern as seedWavetables below.
    // "<Folder>__<File>" names (Story 12.2 example sets) seed into a SUBFOLDER — binary-data
    // resources are flat, so the multisample folder structure is encoded in the filename.
    inline void seedSamples()
    {
        auto dir = samplesFolder();
        for (int i = 0; i < Samples::namedResourceListSize; ++i)
        {
            int size = 0;
            const char* data = Samples::getNamedResource(Samples::namedResourceList[i], size);
            if (data == nullptr || size <= 0) continue;
            juce::String name = Samples::getNamedResourceOriginalFilename(Samples::namedResourceList[i]);
            auto target = dir;
            if (name.contains("__"))
            {
                target = dir.getChildFile(name.upToFirstOccurrenceOf("__", false, false));
                target.createDirectory();
                name = name.fromFirstOccurrenceOf("__", false, false);
            }
            auto file = target.getChildFile(name);
            if (! file.existsAsFile())
                file.replaceWithData(data, (size_t) size);
        }
    }

    // Load a subfolder of the Samples folder as ONE mapped set (Story 12.2): a contained .sfz
    // wins (imported mapping); otherwise the folder naming convention applies. Message thread.
    inline int loadSampleSetFolder(const juce::File& dir, juce::String* error = nullptr,
                                   const std::function<bool()>& shouldAbort = {})
    {
        auto sfzs = dir.findChildFiles(juce::File::findFiles, false, "*.sfz");
        if (! sfzs.isEmpty())
        {
            sfzs.sort();
            return SampleBankStore::instance().loadSfz(sfzs.getReference(0), error, shouldAbort);
        }
        return SampleBankStore::instance().loadFolder(dir, error, shouldAbort);
    }

    // Import a user-chosen SAMPLER source (Story 12.2): copies it into %AppData%\JASS\Samples
    // first (preset portability — presets reference sets by NAME), then loads it. Handles all
    // three source kinds; returns the set index or -1. Copy failures fall back to loading in
    // place (exotic paths still play, they just won't survive into other sessions' presets).
    //   · audio file       → Samples\<name>.<ext>            (12.1 behaviour, unchanged)
    //   · folder           → Samples\<folder>\ (audio + any .sfz, flat)
    //   · .sfz             → Samples\<sfzname>\ (the .sfz + its referenced samples)
    // Copy an .sfz plus its referenced samples into `dest`, preserving the sfz's relative
    // paths. Paths escaping the sfz's tree (`..`, absolute, other drive) are skipped — that set
    // still plays from RAM this session, it just won't survive into future sessions.
    inline void copySfzPortable(const juce::File& sfz, const juce::File& dest)
    {
        dest.createDirectory();
        auto destSfz = dest.getChildFile(sfz.getFileName());
        if (! destSfz.existsAsFile())
            sfz.copyFileTo(destSfz);
        for (const auto& e : SampleMapping::entriesFromSfz(sfz))
        {
            auto rel = e.file.getRelativePathFrom(sfz.getParentDirectory());
            if (rel.startsWith("..") || juce::File::isAbsolutePath(rel)) continue;
            auto d = dest.getChildFile(rel);
            d.getParentDirectory().createDirectory();
            if (! d.existsAsFile())
                e.file.copyFileTo(d);
        }
    }

    inline int importSamplerSource(const juce::File& src, juce::String* error = nullptr)
    {
        auto& store = SampleBankStore::instance();
        // All branches: LOAD FIRST, COPY ONLY ON SUCCESS (review 2026-08-04: now including the
        // single-file branch). The audio lives in RAM once loaded — the AppData copy exists
        // purely so future sessions can re-resolve the set by name, so a REJECTED source must
        // not leave a dead copy behind (it would silently fail at every preload from then on).
        // Copy failures are non-fatal: the set still plays this session.
        if (src.isDirectory())
        {
            if (src == samplesFolder())
            {   // review 2026-08-04: confirming the chooser at its start location would import
                // the whole library root as a set named "Samples" (+ a recursive self-copy)
                if (error) *error = "this IS the JASS sample library folder - choose a folder "
                                    "that contains ONE instrument (or a subfolder in here)";
                return -1;
            }
            const int idx = loadSampleSetFolder(src, error);
            if (idx < 0) return -1;
            auto dest = samplesFolder().getChildFile(src.getFileName());
            auto sfzs = src.findChildFiles(juce::File::findFiles, false, "*.sfz");
            if (! sfzs.isEmpty())
            {   // mapped via the contained .sfz — its samples may live in SUBFOLDERS, which a
                // flat copy would lose (dead AppData folder, review 2026-08-04)
                sfzs.sort();
                copySfzPortable(sfzs.getReference(0), dest);
            }
            else
            {
                dest.createDirectory();
                for (auto& f : src.findChildFiles(juce::File::findFiles, false, SampleMapping::kAudioWildcard))
                {
                    auto d = dest.getChildFile(f.getFileName());
                    if (! d.existsAsFile())
                        f.copyFileTo(d);
                }
            }
            return idx;
        }
        if (src.hasFileExtension("sfz"))
        {
            const int idx = store.loadSfz(src, error);
            if (idx < 0) return -1;
            copySfzPortable(src, samplesFolder().getChildFile(src.getFileNameWithoutExtension()));
            return idx;
        }
        const int idx = store.loadFile(src, error);   // single audio file (12.1)
        if (idx < 0) return -1;
        auto dest = samplesFolder().getChildFile(src.getFileName());
        if (! dest.existsAsFile())
            src.copyFileTo(dest);
        return idx;
    }

    // Pre-load every sample from the Samples folder into the store at startup (message thread),
    // so the SET combo is populated without a manual LOAD. Alphabetical => deterministic indices
    // (stable preset SET references as long as the folder is stable); bounded by the store caps.
    // Single files first (12.1 indices stay put), then subfolders = multisample sets (12.2).
    // Installed by the processor (12.6): "this preset wants the set named X". Loading it is the
    // background loader's job — applyVar must never decode a set on the message thread. Left
    // empty where no loader exists (plugin build), in which case applyVar loads synchronously.
    // `targetParamId` says WHICH selector the set belongs to once it lands: the SAMPLER's SET, or
    // PERC's KIT (Story 16.1) — two independent instruments that must be able to wait for two
    // different sets. Without the parameter this hook would always drop the loaded set on the
    // sampler, which for a drum kit is exactly the wrong instrument.
    inline std::function<void(const juce::String& setName, const juce::String& targetParamId)> requestSamplerSet;

    // Set name a preset asked for that has not arrived yet. While this is pending, the SET index
    // still points at some OTHER set — and the LiveState is written every 1.5 s. Saving the name
    // behind that index would quietly swap the patch's instrument, so toVar() persists this name
    // instead. Cleared when the set is selected, when it cannot be found, and when the user picks
    // a set by hand (their choice wins over a pending restore).
    inline juce::String pendingSamplerSetName;
    inline juce::String pendingPercKitName;      // the same guard for PERC's kit (Story 16.1)

    // `shouldAbort` is polled between sets so the caller can cut a long preload short — the
    // background thread of 12.6 passes its threadShouldExit() here. Default: never abort.
    inline void preloadSamples(std::function<bool()> shouldAbort = {})
    {
        auto files = samplesFolder().findChildFiles(juce::File::findFiles, false, SampleMapping::kAudioWildcard);
        files.sort();
        for (auto& f : files)
        {
            if (shouldAbort && shouldAbort()) return;
            SampleBankStore::instance().loadFile(f);
        }
        auto dirs = samplesFolder().findChildFiles(juce::File::findDirectories, false);
        dirs.sort();
        for (auto& d : dirs)
        {
            if (shouldAbort && shouldAbort()) return;
            loadSampleSetFolder(d, nullptr, shouldAbort);   // also polled per zone inside
        }
    }

    // First-run seeding of the shipped example wavetables (embedded from Wavetables/*.wav). Same
    // idempotent pattern as seedDemoPresets — an existing file is never overwritten. Call once.
    inline void seedWavetables()
    {
        auto dir = wavetablesFolder();
        for (int i = 0; i < Wavetables::namedResourceListSize; ++i)
        {
            int size = 0;
            const char* data = Wavetables::getNamedResource(Wavetables::namedResourceList[i], size);
            if (data == nullptr || size <= 0)
                continue;
            auto dest = dir.getChildFile(Wavetables::getNamedResourceOriginalFilename(Wavetables::namedResourceList[i]));
            if (! dest.existsAsFile())
                dest.replaceWithData(data, (size_t) size);
        }
    }

    namespace detail
    {
        using APVTS = juce::AudioProcessorValueTreeState;

        inline double rawF(APVTS& a, const juce::String& id) { return (double) *a.getRawParameterValue(id); }
        inline int    rawI(APVTS& a, const juce::String& id) { return (int)    *a.getRawParameterValue(id); }
        inline bool   rawB(APVTS& a, const juce::String& id) { return *a.getRawParameterValue(id) > 0.5f; }
        inline juce::String rawChoice(APVTS& a, const juce::String& id, const juce::StringArray& names)
        {
            int i = rawI(a, id);
            return juce::isPositiveAndBelow(i, names.size()) ? names[i] : names[0];
        }

        // Set a parameter from an engineering (raw) value via its normalisable range.
        inline void setRaw(APVTS& a, const juce::String& id, float rawValue)
        {
            if (auto* p = a.getParameter(id))
                p->setValueNotifyingHost(p->convertTo0to1(rawValue));
        }
        inline void setChoice(APVTS& a, const juce::String& id, const juce::StringArray& names, const juce::var& v, int fallback)
        {
            int idx = names.indexOf(v.toString());
            setRaw(a, id, (float) (idx >= 0 ? idx : fallback));
        }

        // Filter/Distortion/LFO/Noise used to fold their bypass into the choice's "Off"
        // entry; now a separate <on> bool gates them. The on-DISK format is UNCHANGED for
        // C# compatibility: "Off" is still written when disabled. `names` is the full
        // canonical list incl. "Off" at index 0; the live choice param holds the trimmed
        // list, so its index maps to names[index + 1].
        inline juce::String choiceOrOff(APVTS& a, const juce::String& onId, const juce::String& typeId, const juce::StringArray& names)
        {
            if (! rawB(a, onId)) return names[0];               // disabled → "Off"
            int i = rawI(a, typeId) + 1;
            return juce::isPositiveAndBelow(i, names.size()) ? names[i] : names[0];
        }
        inline void setChoiceOrOff(APVTS& a, const juce::String& onId, const juce::String& typeId, const juce::StringArray& names, const juce::var& v)
        {
            int idx = names.indexOf(v.toString());
            if (idx < 0) return;                                // field missing/unknown → keep state
            if (idx == 0) { setRaw(a, onId, 0.0f); }            // "Off" → disabled (type kept as-is)
            else { setRaw(a, onId, 1.0f); setRaw(a, typeId, (float) (idx - 1)); }
        }

        // JSON readers with fallback to the current value (missing fields keep state).
        inline double jnum (const juce::var& o, const char* k, double def) { return o.hasProperty(k) ? (double) o[k] : def; }
        inline bool   jbool(const juce::var& o, const char* k, bool def)   { return o.hasProperty(k) ? (bool)   o[k] : def; }
        inline int    jint (const juce::var& o, const char* k, int def)    { return o.hasProperty(k) ? (int)    o[k] : def; }
    }

    using APVTS = juce::AudioProcessorValueTreeState;
    namespace ID = Parameters::ID;

    // ── Export ──
    // `modified` flags an unsaved working state (only meaningful for LiveState;
    // a freshly saved named preset is by definition unmodified).
    inline juce::var toVar(APVTS& a, const juce::String& name, bool modified = false)
    {
        using namespace detail;
        auto* root = new juce::DynamicObject();
        root->setProperty("FormatVersion", kFormatVersion);
        root->setProperty("Name", name);
        root->setProperty("Modified", modified);

        // Every module writes its own nested object (spec-driven; Source/Modules/*Specs.h).
        Modules::writeState(a, *root);

        // Rack layout (Story 4.3, AD-11): append-only. The editor's Rack stores the custom
        // layout as a JSON string on apvts.state ("rackLayout"); mirror it into the preset as
        // a nested, readable "RackLayout" field. Absent when the layout is the default ⇒ field
        // omitted (clean preset, missing⇒default on load; the C# app ignores the unknown field).
        if (auto s = a.state.getProperty(juce::Identifier("rackLayout")).toString(); s.isNotEmpty())
            root->setProperty("RackLayout", juce::JSON::parse(s));

        // SAMPLER (Story 12.1): persist the selected sample's NAME alongside the session-local SET
        // index, so applyVar can re-resolve it from the Samples folder across sessions.
        {
            const int idx = static_cast<int>(*a.getRawParameterValue(ID::samplerSet));
            const auto* set = SampleBankStore::instance().getSet(idx);
            // A set still being fetched for this patch wins over whatever the index points at
            // right now (see pendingSamplerSetName) — otherwise a save during the load would
            // rewrite the patch to a different instrument.
            const juce::String setName = pendingSamplerSetName.isNotEmpty() ? pendingSamplerSetName
                                       : (set != nullptr ? set->getName() : juce::String());
            if (setName.isNotEmpty())
                if (auto* mod = root->getProperty("Sampler").getDynamicObject())
                    mod->setProperty("File", setName);
        }

        // PERC (Story 16.1): the same treatment for the drum kit, and for the same reason — KIT is
        // an index into a store whose contents depend on what is installed locally. A preset that
        // carried only the index would point at whatever set happens to sit there on another
        // machine, which for a drum pattern means a piano.
        {
            const int idx = static_cast<int>(*a.getRawParameterValue(ID::percKit)) - 1;   // 0 = no kit
            const auto* set = SampleBankStore::instance().getSet(idx);
            const juce::String kitName = pendingPercKitName.isNotEmpty() ? pendingPercKitName
                                       : (set != nullptr ? set->getName() : juce::String());
            // "File", not "Kit": the module's own writeState already puts the session-local INDEX
            // under "Kit". The name goes beside it, exactly as the SAMPLER stores "Set" + "File".
            if (kitName.isNotEmpty())
                if (auto* mod = root->getProperty("Perc").getDynamicObject())
                    mod->setProperty("File", kitName);
        }

        return juce::var(root);
    }

    inline bool saveToFile(APVTS& a, const juce::File& file, const juce::String& name, bool modified = false)
    {
        return file.replaceWithText(juce::JSON::toString(toVar(a, name, modified), false));
    }

    // ── Import (LEGACY FLAT reader, v<3) ──
    // Reads the OLD flat .synthy format. Retained ONLY to convert existing presets to v3 once
    // (see convertLegacyPresetsToV3). New code uses applyVar (nested) below.
    inline void applyVarFlatLegacy(APVTS& a, const juce::var& v)
    {
        using namespace detail;
        if (! v.isObject())
            return;

        // Format version (Story: versioning). Absent ⇒ 1 (pre-versioned files). We ALWAYS load
        // by first establishing a factory baseline (below), then layering the file's values on
        // top, so any version — including older presets that lack newer fields — loads safely
        // and picks up factory defaults for whatever it omits. The version number is reserved
        // for future *value* migrations (a field whose meaning changed), not mere new fields
        // (those are handled by the missing⇒default fallback).
        const int fileVersion = jint(v, "FormatVersion", 1);
        juce::ignoreUnused(fileVersion);

        // FACTORY RESET before reading (a preset is a COMPLETE snapshot): reset every parameter
        // to its default AND clear the rack layout to factory. Any field the file omits then
        // falls back to factory instead of inheriting the previously loaded patch. (Per-field
        // readers below use the now-default current value as their "missing key" fallback.)
        for (auto* p : a.processor.getParameters())
            p->setValueNotifyingHost(p->getDefaultValue());
        a.state.removeProperty(juce::Identifier("rackLayout"), nullptr);   // factory layout baseline

        if (auto oscs = v["Oscillators"]; oscs.isArray())
        {
            for (int o = 0; o < juce::jmin(3, oscs.size()); ++o)
            {
                auto od = oscs[o];
                setRaw   (a, ID::oscOn(o + 1),        jbool(od, "Enabled", rawB(a, ID::oscOn(o + 1))) ? 1.f : 0.f);
                setChoice(a, ID::oscWave(o + 1), kWaveform, od["Waveform"], rawI(a, ID::oscWave(o + 1)));
                setRaw   (a, ID::oscFreq(o + 1),      (float) jnum(od, "Frequency", rawF(a, ID::oscFreq(o + 1))));
                setRaw   (a, ID::oscAmp(o + 1),       (float) jnum(od, "Amplitude", rawF(a, ID::oscAmp(o + 1))));
                setRaw   (a, ID::oscUniVoices(o + 1), (float) jint(od, "UnisonVoices", rawI(a, ID::oscUniVoices(o + 1))));
                setRaw   (a, ID::oscUniDetune(o + 1), (float) jnum(od, "UnisonDetune", rawF(a, ID::oscUniDetune(o + 1))));
                setRaw   (a, ID::oscFeedback(o + 1),  (float) jnum(od, "Feedback",     rawF(a, ID::oscFeedback(o + 1))));   // Self-FM (missing => factory 0)
            }
        }

        setRaw   (a, ID::masterVol, (float) jnum(v, "MasterVolume", rawF(a, ID::masterVol)));
        setRaw   (a, ID::syncTempo, (float) jnum(v, "SyncTempo", rawF(a, ID::syncTempo)));   // Tempo-Sync; missing => factory 120
        setRaw   (a, ID::masterOn,  jbool(v, "MasterOn",  rawB(a, ID::masterOn))  ? 1.f : 0.f);   // Story 2.4; missing => keep default (on)
        setChoiceOrOff(a, ID::mixModeOn, ID::mixMode, kMixMode, v["MixMode"]);   // "Additive" => off
        // Back-compat: a Story-2.4-era preset may carry an explicit MixModeOn=false next to a
        // RingMod/FM mode (off + mode were separate then) — honour it so it stays additive.
        if (v.hasProperty("MixModeOn") && ! (bool) v["MixModeOn"]) setRaw(a, ID::mixModeOn, 0.0f);
        setChoice(a, ID::mixSrcA, kMixSrc, v["MixSrcA"], rawI(a, ID::mixSrcA));   // Epic 5 (missing => default 0/1)
        setChoice(a, ID::mixSrcB, kMixSrc, v["MixSrcB"], rawI(a, ID::mixSrcB));
        setRaw   (a, ID::scopeOn,    jbool(v, "ScopeOn",    rawB(a, ID::scopeOn))    ? 1.f : 0.f);   // missing => on
        setRaw   (a, ID::spectrumOn, jbool(v, "SpectrumOn", rawB(a, ID::spectrumOn)) ? 1.f : 0.f);
        setRaw   (a, ID::keyboardOn, jbool(v, "KeyboardOn", rawB(a, ID::keyboardOn)) ? 1.f : 0.f);   // missing => on

        setRaw(a, ID::adsrOn,  jbool(v, "AdsrOn", rawB(a, ID::adsrOn)) ? 1.f : 0.f);   // Story 2.4; missing => keep default (on)
        setRaw(a, ID::attack,  (float) jnum(v, "Attack",  rawF(a, ID::attack)));
        setRaw(a, ID::decay,   (float) jnum(v, "Decay",   rawF(a, ID::decay)));
        setRaw(a, ID::sustain, (float) jnum(v, "Sustain", rawF(a, ID::sustain)));
        setRaw(a, ID::release, (float) jnum(v, "Release", rawF(a, ID::release)));

        setChoiceOrOff(a, ID::filterOn, ID::filterType, kFilterType, v["FilterType"]);
        setRaw(a, ID::filterCutoff, (float) jnum(v, "FilterCutoff",    rawF(a, ID::filterCutoff)));
        setRaw(a, ID::filterReso,   (float) jnum(v, "FilterResonance", rawF(a, ID::filterReso)));

        setRaw(a, ID::formantOn,    jbool(v, "FormantEnabled", rawB(a, ID::formantOn)) ? 1.f : 0.f);   // Feature 3; missing => off
        setRaw(a, ID::formantVowel, (float) jnum(v, "FormantVowel",     rawF(a, ID::formantVowel)));
        setRaw(a, ID::formantReso,  (float) jnum(v, "FormantResonance", rawF(a, ID::formantReso)));
        setRaw(a, ID::formantMix,   (float) jnum(v, "FormantMix",       rawF(a, ID::formantMix)));

        setChoiceOrOff(a, ID::distortionOn, ID::distortionType, kDistortion, v["DistortionType"]);
        setRaw(a, ID::distortionDrive, (float) jnum(v, "DistortionDrive", rawF(a, ID::distortionDrive)));
        setRaw(a, ID::distortionMix,   (float) jnum(v, "DistortionMix",   rawF(a, ID::distortionMix)));

        setRaw(a, ID::wavefoldOn,       jbool(v, "WavefoldEnabled", rawB(a, ID::wavefoldOn)) ? 1.f : 0.f);
        setRaw(a, ID::wavefoldDrive,    (float) jnum(v, "WavefoldDrive",    rawF(a, ID::wavefoldDrive)));
        setRaw(a, ID::wavefoldSymmetry, (float) jnum(v, "WavefoldSymmetry", rawF(a, ID::wavefoldSymmetry)));
        setRaw(a, ID::wavefoldMix,      (float) jnum(v, "WavefoldMix",      rawF(a, ID::wavefoldMix)));

        setRaw(a, ID::bitcrushOn,   jbool(v, "BitcrushEnabled", rawB(a, ID::bitcrushOn)) ? 1.f : 0.f);
        setRaw(a, ID::bitcrushBits, (float) jint(v, "BitcrushBits", rawI(a, ID::bitcrushBits)));
        setRaw(a, ID::bitcrushRate, (float) jint(v, "BitcrushRate", rawI(a, ID::bitcrushRate)));
        setRaw(a, ID::bitcrushMix,  (float) jnum(v, "BitcrushMix",  rawF(a, ID::bitcrushMix)));

        setRaw   (a, ID::phaserOn,       jbool(v, "PhaserEnabled", rawB(a, ID::phaserOn)) ? 1.f : 0.f);   // Feature 2; missing => off
        setChoice(a, ID::phaserType, kPhaserType, v["PhaserType"], rawI(a, ID::phaserType));
        setRaw   (a, ID::phaserRate,     (float) jnum(v, "PhaserRate",     rawF(a, ID::phaserRate)));
        setRaw   (a, ID::phaserDepth,    (float) jnum(v, "PhaserDepth",    rawF(a, ID::phaserDepth)));
        setRaw   (a, ID::phaserFeedback, (float) jnum(v, "PhaserFeedback", rawF(a, ID::phaserFeedback)));
        setRaw   (a, ID::phaserMix,      (float) jnum(v, "PhaserMix",      rawF(a, ID::phaserMix)));

        setRaw   (a, ID::subOn, jbool(v, "SubEnabled", rawB(a, ID::subOn)) ? 1.f : 0.f);
        setChoice(a, ID::subWave, kSubWave, v["SubWaveform"], rawI(a, ID::subWave));
        // Stored as -1 / -2; map back to choice index 0 / 1.
        setRaw   (a, ID::subOctave, (float) (-jint(v, "SubOctave", -(rawI(a, ID::subOctave) + 1)) - 1));
        setRaw   (a, ID::subLevel, (float) jnum(v, "SubLevel", rawF(a, ID::subLevel)));

        setRaw(a, ID::stereoOn,    jbool(v, "StereoEnabled", rawB(a, ID::stereoOn)) ? 1.f : 0.f);
        setRaw(a, ID::stereoWidth, (float) jnum(v, "StereoWidth", rawF(a, ID::stereoWidth)));
        setRaw(a, ID::stereoTime,  (float) jnum(v, "StereoTime",  rawF(a, ID::stereoTime)));

        setRaw(a, ID::compOn,        jbool(v, "CompEnabled", rawB(a, ID::compOn)) ? 1.f : 0.f);   // append-only; missing => off
        setRaw(a, ID::compThreshold, (float) jnum(v, "CompThreshold", rawF(a, ID::compThreshold)));
        setRaw(a, ID::compRatio,     (float) jnum(v, "CompRatio",     rawF(a, ID::compRatio)));
        setRaw(a, ID::compAttack,    (float) jnum(v, "CompAttack",    rawF(a, ID::compAttack)));
        setRaw(a, ID::compRelease,   (float) jnum(v, "CompRelease",   rawF(a, ID::compRelease)));
        setRaw(a, ID::compMakeup,    (float) jnum(v, "CompMakeup",    rawF(a, ID::compMakeup)));

        // Modulation matrix (Epic 8; append-only; missing => Off/0, ModMatrixOn missing => on).
        setRaw(a, ID::modMatrixOn, jbool(v, "ModMatrixOn", rawB(a, ID::modMatrixOn)) ? 1.f : 0.f);
        for (int n = 1; n <= ModMatrixConfig::kNumSlots; ++n)
        {
            const juce::String p = "ModSlot" + juce::String(n);
            setChoice(a, ID::modSlotSource(n), kModSource, v[juce::Identifier(p + "Source")], rawI(a, ID::modSlotSource(n)));
            // v<3 flat presets predate the matrix entirely: no Module/Param keys. Reset to Off so a
            // reused scratch APVTS never carries a stale routing; migrateSlotTargetsToModuleParam
            // (below) converts any legacy "Target" string when one is present.
            setRaw   (a, ID::modSlotModule(n), 0.0f);   // 0 == "Off"
            setRaw   (a, ID::modSlotParam(n),  0.0f);
            setRaw   (a, ID::modSlotAmount(n), (float) jnum(v, (p + "Amount").toRawUTF8(), rawF(a, ID::modSlotAmount(n))));
        }

        setRaw   (a, ID::arpOn,      jbool(v, "ArpEnabled", rawB(a, ID::arpOn)) ? 1.f : 0.f);
        setRaw   (a, ID::arpRate,    (float) jnum(v, "ArpRate", rawF(a, ID::arpRate)));
        setChoice(a, ID::arpMode, kArpMode, v["ArpMode"], rawI(a, ID::arpMode));
        setRaw   (a, ID::arpOctaves, (float) jint(v, "ArpOctaves", rawI(a, ID::arpOctaves)));
        setRaw   (a, ID::arpGate,    (float) jnum(v, "ArpGate", rawF(a, ID::arpGate)));

        setRaw   (a, ID::glideOn,   jbool(v, "GlideEnabled", rawB(a, ID::glideOn)) ? 1.f : 0.f);   // Feature 4; missing => off
        setRaw   (a, ID::glideTime, (float) jnum(v, "GlideTime", rawF(a, ID::glideTime)));
        setChoice(a, ID::glideMode, kGlideMode, v["GlideMode"], rawI(a, ID::glideMode));   // missing => Mono

        setRaw   (a, ID::pitchEnvOn,     jbool(v, "PitchEnvEnabled", rawB(a, ID::pitchEnvOn)) ? 1.f : 0.f);   // append-only; missing => off
        setRaw   (a, ID::pitchEnvAmount, (float) jnum(v, "PitchEnvAmount", rawF(a, ID::pitchEnvAmount)));
        setRaw   (a, ID::pitchEnvTime,   (float) jnum(v, "PitchEnvTime",   rawF(a, ID::pitchEnvTime)));

        // LFOs — indexed read-back. LFO 1 falls back to the PRE-INDEXING field names
        // ("LfoWaveform" …) so older JASS presets and the current LiveState keep their LFO 1
        // across the rename (C# compatibility dropped 2026-07-18).
        for (int i = 1; i <= kNumLFOs; ++i)
        {
            const juce::String p = "Lfo" + juce::String(i);
            auto pick = [&](const char* suffix, const char* legacy) -> juce::var
            {
                juce::var nv = v[juce::Identifier(p + suffix)];
                if (! nv.isVoid()) return nv;
                return (i == 1) ? v[juce::Identifier(legacy)] : juce::var();
            };
            setChoice(a, ID::lfoWave(i), kLfoWave, pick("Waveform", "LfoWaveform"), rawI(a, ID::lfoWave(i)));
            setChoiceOrOff(a, ID::lfoOn(i), ID::lfoTarget(i), kLfoTarget, pick("Target", "LfoTarget"));
            if (juce::var rv = pick("Rate",  "LfoRate");  ! rv.isVoid()) setRaw(a, ID::lfoRate(i),  (float) (double) rv);
            if (juce::var dv = pick("Depth", "LfoDepth"); ! dv.isVoid()) setRaw(a, ID::lfoDepth(i), (float) (double) dv);
            setChoice(a, ID::lfoSyncDiv(i), SyncDivision::kNames, pick("SyncDiv", "LfoSyncDiv"), rawI(a, ID::lfoSyncDiv(i)));
        }

        setRaw(a, ID::delayOn,       jbool(v, "DelayEnabled", rawB(a, ID::delayOn)) ? 1.f : 0.f);
        setRaw(a, ID::delayTime,     (float) jnum(v, "DelayTime",     rawF(a, ID::delayTime)));
        setRaw(a, ID::delayFeedback, (float) jnum(v, "DelayFeedback", rawF(a, ID::delayFeedback)));
        setRaw(a, ID::delayMix,      (float) jnum(v, "DelayMix",      rawF(a, ID::delayMix)));
        setChoice(a, ID::delaySyncDiv, SyncDivision::kNames, v["DelaySyncDiv"], rawI(a, ID::delaySyncDiv));   // Tempo-Sync; missing => Free

        setRaw(a, ID::chorusOn,    jbool(v, "ChorusEnabled", rawB(a, ID::chorusOn)) ? 1.f : 0.f);
        setRaw(a, ID::chorusRate,  (float) jnum(v, "ChorusRate",  rawF(a, ID::chorusRate)));
        setRaw(a, ID::chorusDepth, (float) jnum(v, "ChorusDepth", rawF(a, ID::chorusDepth)));
        setRaw(a, ID::chorusMix,   (float) jnum(v, "ChorusMix",   rawF(a, ID::chorusMix)));

        setRaw(a, ID::reverbOn,   jbool(v, "ReverbEnabled", rawB(a, ID::reverbOn)) ? 1.f : 0.f);
        setRaw(a, ID::reverbRoom, (float) jnum(v, "ReverbRoomSize", rawF(a, ID::reverbRoom)));
        setRaw(a, ID::reverbDamp, (float) jnum(v, "ReverbDamping",  rawF(a, ID::reverbDamp)));
        setRaw(a, ID::reverbMix,  (float) jnum(v, "ReverbMix",      rawF(a, ID::reverbMix)));

        setRaw(a, ID::karplusOn,      jbool(v, "KarplusEnabled", rawB(a, ID::karplusOn)) ? 1.f : 0.f);
        setRaw(a, ID::karplusFreq,    (float) jnum(v, "KarplusFrequency", rawF(a, ID::karplusFreq)));
        setRaw(a, ID::karplusAmp,     (float) jnum(v, "KarplusAmplitude", rawF(a, ID::karplusAmp)));
        setRaw(a, ID::karplusDamping, (float) jnum(v, "KarplusDamping",   rawF(a, ID::karplusDamping)));
        setRaw(a, ID::karplusStretch, (float) jnum(v, "KarplusStretch",   rawF(a, ID::karplusStretch)));

        setChoiceOrOff(a, ID::noiseOn, ID::noiseType, kNoiseType, v["NoiseType"]);
        setRaw(a, ID::noiseAmp, (float) jnum(v, "NoiseAmplitude", rawF(a, ID::noiseAmp)));

        setRaw(a, ID::wavetableOn,        jbool(v, "WavetableEnabled", rawB(a, ID::wavetableOn)) ? 1.f : 0.f);
        setRaw(a, ID::wavetableBank,      (float) jint(v, "WavetableBankIndex",    rawI(a, ID::wavetableBank)));
        setRaw(a, ID::wavetablePosition,  (float) jnum(v, "WavetablePosition",     rawF(a, ID::wavetablePosition)));
        setRaw(a, ID::wavetableFreq,      (float) jnum(v, "WavetableFrequency",    rawF(a, ID::wavetableFreq)));
        setRaw(a, ID::wavetableAmp,       (float) jnum(v, "WavetableAmplitude",    rawF(a, ID::wavetableAmp)));
        setRaw(a, ID::wavetableUniVoices, (float) jint(v, "WavetableUnisonVoices", rawI(a, ID::wavetableUniVoices)));
        setRaw(a, ID::wavetableUniDetune, (float) jnum(v, "WavetableUnisonDetune", rawF(a, ID::wavetableUniDetune)));

        // Rack layout (Story 4.3): layer the "RackLayout" field onto the factory-cleared
        // property (baseline set above). Absent ⇒ stays factory-default. The editor re-applies
        // it via Rack::reloadLayoutFromState() after a load.
        if (v.hasProperty("RackLayout"))
            a.state.setProperty(juce::Identifier("rackLayout"), juce::JSON::toString(v["RackLayout"]), nullptr);
    }

    // ── Import (nested v3 — the live reader) ──
    inline void applyVar(APVTS& a, const juce::var& v)
    {
        if (! v.isObject())
            return;
        // A preset is a COMPLETE snapshot: factory-reset every parameter first, then layer the
        // file's values on top (missing field => factory default). Clear the rack-layout baseline.
        for (auto* p : a.processor.getParameters())
            p->setValueNotifyingHost(p->getDefaultValue());
        a.state.removeProperty(juce::Identifier("rackLayout"), nullptr);

        Modules::readState(a, v);   // each module reads its own nested object (spec-driven)

        if (v.hasProperty("RackLayout"))
            a.state.setProperty(juce::Identifier("rackLayout"), juce::JSON::toString(v["RackLayout"]), nullptr);

        // SAMPLER (Story 12.1): the SET param is a session-local index, so the preset also carries
        // the selected sample's NAME ("Sampler.File", written by toVar). Re-resolve it: already
        // loaded => re-select; else load "<name>.*" from %AppData%\JASS\Samples — or, since 12.2,
        // the subfolder "<name>\" holding a multisample set. Missing source => the combo stays
        // where the index landed (documented: samples live in the Samples folder).
        // Message-thread only (applyVar always is), like every store load.
        // One routine, two instruments (Story 16.1): the SAMPLER's SET and PERC's KIT are both
        // session-local indices into the same store, so both persist a NAME and both re-resolve it
        // the same way. A patch can now be waiting for TWO sets at once — an instrument and a drum
        // kit — which is why this resolves both instead of returning after the first request.
        // `valueOffset` is 1 for PERC, whose KIT counts from 1 so that 0 can mean "no kit".
        auto resolveSet = [&a] (const juce::var& src, const char* object, const char* field,
                                const char* paramId, juce::String& pending, int valueOffset = 0)
        {
            auto* mod = src[object].getDynamicObject();
            if (mod == nullptr)
                return;
            const juce::String name = mod->getProperty (field).toString();
            if (name.isEmpty())
                return;
            int idx = SampleBankStore::instance().indexOf (name);
            if (idx < 0 && requestSamplerSet)
            {
                // Not in the store yet: hand it to the background loader (12.6) instead of
                // decoding it here. A grand piano is several hundred MB — doing that on the
                // message thread is what kept the window from appearing for ten seconds. The
                // loader pulls this set to the FRONT of its queue and selects it into `paramId`
                // when it lands, so the patch ends up correct, just a moment later.
                pending = name;   // guards the LiveState while it loads
                requestSamplerSet (name, paramId);
                return;
            }
            pending.clear();      // this preset's set is already here
            if (idx < 0)
            {   // no loader installed (plugin build / tests): the original synchronous path
                auto matches = samplesFolder().findChildFiles (juce::File::findFiles, false, name + ".*");
                if (! matches.isEmpty())
                    idx = SampleBankStore::instance().loadFile (matches.getReference (0));
                else if (auto sub = samplesFolder().getChildFile (name); sub.isDirectory())
                    idx = loadSampleSetFolder (sub);   // 12.2: mapped set by folder name
            }
            if (idx >= 0)
                if (auto* p = a.getParameter (paramId))
                    p->setValueNotifyingHost (p->convertTo0to1 ((float) (idx + valueOffset)));
        };
        resolveSet (v, "Sampler", "File", ID::samplerSet, pendingSamplerSetName);
        resolveSet (v, "Perc",    "File", ID::percKit,    pendingPercKitName, 1);
    }

    // v3→v4 step: fold each enabled LFO's (now UI-less) built-in TARGET into a free MOD MATRIX slot,
    // so patches that routed an LFO via its own target keep working now that LFOs are pure sources.
    // Operates on the already-loaded apvts. LFO target index i (0=Frequency…) maps to matrix target
    // i+1 (== LFOTarget); the LFO's ModSource is kLfoSrc[i]. Skips if no slot is free.
    inline void migrateLfoTargetsToSlots(APVTS& a)
    {
        using namespace detail;
        static constexpr int kLfoSrc[kNumLFOs] = { (int) ModSource::LFO1, (int) ModSource::LFO2,
                                                   (int) ModSource::LFO3, (int) ModSource::LFO4 };
        bool migratedAny = false;
        for (int i = 1; i <= kNumLFOs; ++i)
        {
            if (! rawB(a, ID::lfoOn(i))) continue;                 // LFO off => nothing was routing
            const int matrixTgt = rawI(a, ID::lfoTarget(i)) + 1;   // LFO target idx -> LFOTarget
            const auto mp = ModDest::fromLegacyTarget(matrixTgt);  // v5: LFOTarget -> (module, param)
            for (int n = 1; n <= ModMatrixConfig::kNumSlots; ++n)
            {
                if (rawI(a, ID::modSlotModule(n)) != 0) continue;  // slot occupied (module != Off)
                setRaw(a, ID::modSlotSource(n), (float) kLfoSrc[i - 1]);
                setRaw(a, ID::modSlotModule(n), (float) mp.module);
                setRaw(a, ID::modSlotParam(n),  (float) mp.param);
                setRaw(a, ID::modSlotAmount(n), 1.0f);
                migratedAny = true;
                break;
            }
        }
        // An LFO's built-in target used to modulate INDEPENDENTLY of the MOD MATRIX master switch.
        // Now the matrix is the only route, so a patch that relied on that routing (ModMatrixOn was
        // often false) must have the matrix turned ON — otherwise the migrated slot is silent and the
        // preset loses its modulation (e.g. Helikopter's amp chop, whuwhu's pitch wobble).
        if (migratedAny)
            setRaw(a, ID::modMatrixOn, 1.0f);
    }

    // v4→v5 step: the matrix DEST used to be a single "SlotNTarget" LFOTarget string. Convert each
    // present legacy target into the new MODULE + PARAM pair (global Pitch/Amp/Detune → "Alle OSC",
    // so old patches keep their all-oscillator behaviour). Reads the ORIGINAL parsed file `v`; the
    // spec-driven readState already reset Module/Param to Off (their keys are absent in a v4 file).
    inline void migrateSlotTargetsToModuleParam(APVTS& a, const juce::var& v)
    {
        using namespace detail;
        const auto mm = v[juce::Identifier("ModMatrix")];
        if (! mm.isObject()) return;   // no nested matrix object (pre-v4 / flat) — nothing to convert
        for (int n = 1; n <= ModMatrixConfig::kNumSlots; ++n)
        {
            const juce::var tv = mm[juce::Identifier("Slot" + juce::String(n) + "Target")];
            if (! tv.isString()) continue;                         // slot carried no legacy target
            const int legacy = kLfoTarget.indexOf(tv.toString());  // target string -> LFOTarget index
            if (legacy <= 0) continue;                             // 0 / unknown == Off (slot inactive)
            const auto mp = ModDest::fromLegacyTarget(legacy);
            setRaw(a, ID::modSlotModule(n), (float) mp.module);
            setRaw(a, ID::modSlotParam(n),  (float) mp.param);
        }
    }

    // v5→v6 step: the MOD MATRIX modules + params were reordered A→Z (ModMatrixCatalog). Modules
    // persist as a string (self-correcting on load), but PARAM persists as an INT index — so a v5
    // file's SlotNParam points into the OLD (append-order) param list and must be remapped to the new
    // alphabetical index. The v5 param order was append-only stable, so this table interprets every
    // v5 file (early or late in the session) correctly. Demos are v4 → handled by fromLegacyTarget.
    inline int v5ParamRemap (const juce::String& moduleLabel, int oldIdx)
    {
        using T = LFOTarget;
        static const std::vector<T> oscOrder  { T::Frequency, T::Amplitude, T::OscDetune, T::OscFeedback, T::OscVoices };
        static const std::vector<T> wtOrder   { T::WavetablePosition, T::WavetableFreq, T::WavetableAmp, T::WavetableVoices, T::WavetableDetune };
        static const std::vector<T> filtOrder { T::FilterCutoff, T::FilterResonance };
        static const std::vector<T> formOrder { T::FormantVowel, T::FormantReso, T::FormantMix };
        static const std::vector<T> foldOrder { T::WavefolderDrive, T::WavefolderSym, T::WavefolderMix };
        static const std::vector<T> distOrder { T::DistortionDrive, T::DistortionMix };
        static const std::vector<T> crushOrd  { T::BitcrushMix, T::BitcrushBits, T::BitcrushRate };
        static const std::vector<T> chorOrder { T::ChorusDepth, T::ChorusRate, T::ChorusMix };
        static const std::vector<T> delOrder  { T::DelayTime, T::DelayMix, T::DelayFeedback };
        static const std::vector<T> revOrder  { T::ReverbMix, T::ReverbRoom, T::ReverbDamp };
        static const std::vector<T> subOrder  { T::SubLevel };
        const std::vector<T>* ord = nullptr;
        if      (moduleLabel.startsWith ("OSC") || moduleLabel == "Alle OSC") ord = &oscOrder;
        else if (moduleLabel == "WAVETABLE")  ord = &wtOrder;
        else if (moduleLabel == "FILTER")     ord = &filtOrder;
        else if (moduleLabel == "FORMANT")    ord = &formOrder;
        else if (moduleLabel == "WAVEFOLD")   ord = &foldOrder;
        else if (moduleLabel == "DISTORTION") ord = &distOrder;
        else if (moduleLabel == "BITCRUSH")   ord = &crushOrd;
        else if (moduleLabel == "CHORUS")     ord = &chorOrder;
        else if (moduleLabel == "DELAY")      ord = &delOrder;
        else if (moduleLabel == "REVERB")     ord = &revOrder;
        else if (moduleLabel == "SUB")        ord = &subOrder;
        if (ord == nullptr || oldIdx < 0 || oldIdx >= (int) ord->size())
            return oldIdx;                                   // unknown module / out of range → leave as-is
        const T target = (*ord)[(size_t) oldIdx];
        const int m = ModDest::moduleIndexForLabel (moduleLabel.toRawUTF8());
        if (m < 0) return oldIdx;
        for (int p = 0; p < ModDest::numParams (m); ++p)     // find the target's NEW alphabetical index
            if (ModDest::targetOf (m, p) == target) return p;
        return oldIdx;
    }

    inline void migrateV5ParamOrder (APVTS& a, const juce::var& v)
    {
        using namespace detail;
        const auto mm = v[juce::Identifier ("ModMatrix")];
        if (! mm.isObject()) return;
        for (int n = 1; n <= ModMatrixConfig::kNumSlots; ++n)
        {
            const juce::var modV = mm[juce::Identifier ("Slot" + juce::String (n) + "Module")];
            if (! modV.isString()) continue;   // not a v5 file (v4 has SlotNTarget; pre-v4 has nothing)
            const int oldIdx = (int) mm[juce::Identifier ("Slot" + juce::String (n) + "Param")];
            setRaw (a, ID::modSlotParam (n), (float) v5ParamRemap (modV.toString(), oldIdx));
        }
    }

    // One-time conversion at startup: bring every preset (+ LiveState) up to the current format.
    // v<3 flat => nested (via the legacy flat reader); v3 => fold LFO targets into matrix slots.
    // Loads into `a` as scratch, then re-saves. Idempotent (skips current-version files). Call BEFORE
    // the normal LiveState load — the scratch mutation of `a` is then overwritten by that load.
    inline void convertOldPresets(APVTS& a)
    {
        auto files = presetsFolder().findChildFiles(juce::File::findFiles, false, "*.jass");
        files.add(liveStateFile());
        const juce::File backupDir = jassFolder().getChildFile("PresetsBackup");   // safety net
        for (const auto& f : files)
        {
            if (! f.existsAsFile()) continue;
            auto v = juce::JSON::parse(f.loadFileAsString());
            if (! v.isObject()) continue;
            const int ver = (int) v.getProperty("FormatVersion", 1);
            if (ver >= kFormatVersion) continue;   // already current
            backupDir.createDirectory();
            f.copyFileTo(backupDir.getChildFile(f.getFileName()));   // keep the original, just in case
            if (ver < 3) applyVarFlatLegacy(a, v);                   // flat -> apvts (scratch)
            else         applyVar(a, v);                             // nested v3 -> apvts (scratch)
            migrateSlotTargetsToModuleParam(a, v);                   // v4 SlotNTarget -> Module+Param
            migrateV5ParamOrder(a, v);                               // v5 SlotNParam int -> A→Z reorder
            if (ver < 4) migrateLfoTargetsToSlots(a);                // v3→v4 ONLY: v4+ already route LFOs
                                                                     // via the matrix — re-folding doubles them
            const auto name = v.getProperty("Name", f.getFileNameWithoutExtension()).toString();
            const bool mod  = (bool) v.getProperty("Modified", false);
            saveToFile(a, f, name, mod);                             // re-save at the current version
        }
    }

    // Outcome of a preset load. `ok`=false means the file was missing or not a valid JASS
    // preset (caller should report it loudly, not silently fall back to defaults). `migrated`
    // is set when the file was an older FormatVersion and was upgraded in place (a backup was
    // written to jassFolder()/PresetsBackup first). `fileVersion` is the version read from disk.
    struct LoadResult
    {
        bool ok = false;
        int  fileVersion = 0;
        bool migrated = false;
    };

    inline LoadResult loadFromFile(APVTS& a, const juce::File& file)
    {
        LoadResult r;
        if (! file.existsAsFile())
            return r;
        auto v = juce::JSON::parse(file.loadFileAsString());
        if (! v.isObject())
            return r;   // corrupt / not JSON / not an object → ok stays false (loud fail upstream)

        r.fileVersion = (int) v.getProperty("FormatVersion", 1);

        if (r.fileVersion < kFormatVersion)
        {
            // Older format: back up the original BEFORE we rewrite it, then migrate + re-save in
            // place at the current version (same scheme as the startup convertOldPresets). This is
            // the direct-LOAD counterpart to that batch pass, so a hand-picked old file is upgraded
            // dependably instead of silently mis-loading (e.g. a flat file through the nested reader).
            const juce::File backupDir = jassFolder().getChildFile("PresetsBackup");
            backupDir.createDirectory();
            file.copyFileTo(backupDir.getChildFile(file.getFileName()));

            if (r.fileVersion < 3) applyVarFlatLegacy(a, v);   // flat legacy → apvts
            else                   applyVar(a, v);             // nested v3 → apvts
            migrateSlotTargetsToModuleParam(a, v);             // v4 SlotNTarget → Module+Param
            migrateV5ParamOrder(a, v);                         // v5 SlotNParam int → A→Z reorder
            if (r.fileVersion < 4) migrateLfoTargetsToSlots(a);   // v3→v4 ONLY (v4+ re-fold doubles LFOs)

            const auto name = v.getProperty("Name", file.getFileNameWithoutExtension()).toString();
            const bool mod  = (bool) v.getProperty("Modified", false);
            saveToFile(a, file, name, mod);                    // rewrite at the current version
            r.migrated = true;
        }
        else
        {
            applyVar(a, v);
        }

        r.ok = true;
        return r;
    }

    // Reads just the "Name" field of a preset file (empty if absent/unreadable).
    inline juce::String nameFromFile(const juce::File& file)
    {
        if (! file.existsAsFile())
            return {};
        auto v = juce::JSON::parse(file.loadFileAsString());
        if (v.isObject() && v.hasProperty("Name"))
            return v["Name"].toString();
        return {};
    }

    // Reads the "Modified" flag (unsaved working state). False if absent/unreadable.
    inline bool modifiedFromFile(const juce::File& file)
    {
        if (! file.existsAsFile())
            return false;
        auto v = juce::JSON::parse(file.loadFileAsString());
        return v.isObject() && (bool) v.getProperty("Modified", false);
    }
}
