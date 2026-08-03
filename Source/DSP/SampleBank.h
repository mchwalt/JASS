#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>
#include <atomic>
#include <memory>
#include "SampleMapping.h"

// ── SAMPLER sample sets (Story 12.1, zones since Story 12.2) ────────────────────────────────
// A SampleSet is a named, immutable collection of ZONES. A 12.1 single recording is exactly a
// one-zone set (range 0–127, root supplied live by the ROOT knob); a 12.2 multisample set maps
// each zone to a key range with its own root (mapping imported via folder naming convention or
// a minimal .sfz subset — see SampleMapping.h). One code path, no special case.
//
// STEREO files stay stereo (user decision 2026-07-30 — no downmix): the voice renders a stereo
// zone as TWO panned sub-sources (PanSamplerL/R), so each channel gets its own equal-power/
// binaural/HRTF placement; a mono downmix happens only where the engine itself is mono.
//
// The store copies the RT-hardened WavetableBankStore mechanics verbatim (that file documents
// the reasoning): loading on the MESSAGE THREAD only, visibility to the audio thread via an
// atomic count with release/acquire, append-only slots with stable addresses, duplicate-safe by
// name, and NO freeing (a voice caches raw SampleZone pointers across blocks — use-after-free
// hazard). Never-free is acceptable because the store is BOUNDED (Story 12.2 caps):
//   · per FILE/zone:  ≤ 60 s                     (kMaxSeconds, unchanged from 12.1)
//   · per SET total:  ≤ 300 s across all zones   (kMaxSetSeconds — a real multisampled
//     instrument at ~30 zones × a few seconds fits; a runaway folder does not)
//   · GLOBAL budget:  ≤ kMaxStoreBytes, which is EXACTLY the 12.1 worst case of
//     32 × 60 s stereo @ 44.1 kHz float ≈ 646 MiB — so multisampling cannot exceed the RAM
//     envelope the 12.1 never-free decision was based on, no matter how sets are shaped.
struct SampleZone
{
    std::vector<float> data[2];   // [0]=L (or mono), [1]=R (empty ⇒ mono); at the FILE's rate
    double fileSampleRate = 44100.0;
    int rootKey = 60;             // key that plays the file at original speed (mapped sets)
    int loKey = 0, hiKey = 127;   // inclusive key range (mapped sets; 0..127 for single samples)

    bool   isStereo() const            { return ! data[1].empty(); }
    const float* getData (int ch) const { return data[isStereo() && ch != 0 ? 1 : 0].data(); }
    int    getLength() const           { return (int) data[0].size(); }
    size_t bytes() const               { return (data[0].size() + data[1].size()) * sizeof (float); }
};

class SampleSet
{
public:
    SampleSet (juce::String setName, std::vector<SampleZone> zs, bool isMapped)
        : name (std::move (setName)), zones (std::move (zs)), mapped (isMapped) {}

    const juce::String& getName() const { return name; }
    bool isMapped() const               { return mapped; }
    int  getNumZones() const            { return (int) zones.size(); }
    bool isStereo() const               // any-zone stereo: drives the L/R pan-slot spread
    {
        for (const auto& z : zones)
            if (z.isStereo()) return true;
        return false;
    }
    size_t totalBytes() const
    {
        size_t total = 0;
        for (const auto& z : zones) total += z.bytes();
        return total;
    }

    // Zone for a played note. Mapped: the zone whose [lo,hi] contains it; a note no zone covers
    // (possible with imported .sfz gaps) falls back to the NEAREST zone by range distance, so
    // every key always sounds. Unmapped: the single zone. Audio-thread callable: linear scan,
    // no allocation.
    const SampleZone* zoneFor (int midiNote) const
    {
        if (zones.empty()) return nullptr;
        const SampleZone* best = &zones.front();
        int bestDist = 1 << 20;
        for (const auto& z : zones)
        {
            if (midiNote >= z.loKey && midiNote <= z.hiKey)
                return &z;
            const int dist = midiNote < z.loKey ? z.loKey - midiNote : midiNote - z.hiKey;
            if (dist < bestDist) { bestDist = dist; best = &z; }
        }
        return best;
    }

    static constexpr double kMaxSeconds    = 60.0;    // per-file/zone cap (12.1 AC3, kept)
    static constexpr double kMaxSetSeconds = 300.0;   // per-set total cap (12.2 AC4)

    // Load one WAV/AIFF whole (first two channels) as a zone. Empty vectors on unreadable /
    // zero-length / over-cap files (no silent truncation).
    static bool loadZone (const juce::File& file, SampleZone& out)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
        if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0)
            return false;
        if ((double) reader->lengthInSamples / reader->sampleRate > kMaxSeconds)
            return false;   // over the per-file cap — reject outright

        const int total = (int) reader->lengthInSamples;
        const int nCh   = juce::jmax (1, (int) reader->numChannels);
        juce::AudioBuffer<float> buf (nCh, total);
        reader->read (&buf, 0, total, 0, true, nCh > 1);

        out.data[0].assign (buf.getReadPointer (0), buf.getReadPointer (0) + total);
        out.data[1].clear();
        if (nCh > 1)
            out.data[1].assign (buf.getReadPointer (1), buf.getReadPointer (1) + total);
        out.fileSampleRate = reader->sampleRate;
        return true;
    }

    // 12.1 single-file set: one unmapped zone, full key range (ROOT knob supplies the root).
    static std::unique_ptr<SampleSet> loadFromFile (const juce::File& file)
    {
        SampleZone z;
        if (! loadZone (file, z))
            return nullptr;
        std::vector<SampleZone> zs;
        zs.push_back (std::move (z));
        return std::make_unique<SampleSet> (file.getFileNameWithoutExtension(), std::move (zs), false);
    }

    // 12.2 mapped set from a list of (file, root, lo, hi) entries. nullptr when no entry loads
    // or the set total exceeds kMaxSetSeconds — the WHOLE set is rejected (no partial sets).
    // `error` names the offending FILE and reason (user request 2026-08-03: no guessing).
    static std::unique_ptr<SampleSet> loadFromEntries (const juce::String& setName,
                                                       const std::vector<SampleMapping::Entry>& entries,
                                                       juce::String& error)
    {
        if (entries.empty())
        {
            error = "no mappable samples found (files must be named like \"Name_C3.wav\", "
                    "or the .sfz must contain <region> sample=... entries)";
            return nullptr;
        }
        std::vector<SampleZone> zs;
        double totalSeconds = 0.0;
        for (const auto& e : entries)
        {
            SampleZone z;
            if (! loadZone (e.file, z))
            {   // one bad/over-cap file rejects the set — no truncation surprises
                error = "\"" + e.file.getFileName() + "\" is missing, unreadable, or longer than 60 s"
                      + (e.file.existsAsFile() ? juce::String() : juce::String(" (not found at "
                            + e.file.getFullPathName() + ")"));
                return nullptr;
            }
            z.rootKey = e.rootKey;
            z.loKey   = e.loKey;
            z.hiKey   = e.hiKey;
            totalSeconds += (double) z.getLength() / z.fileSampleRate;
            if (totalSeconds > kMaxSetSeconds)
            {
                error = "the set exceeds 5 minutes of audio in total (reached at \""
                      + e.file.getFileName() + "\")";
                return nullptr;
            }
            zs.push_back (std::move (z));
        }
        return std::make_unique<SampleSet> (setName, std::move (zs), true);
    }

private:
    juce::String name;
    std::vector<SampleZone> zones;   // immutable after construction; addresses stable (never-free store)
    bool mapped = false;
};

// Shared store — single instance across all voices. Same lock-free contract as WavetableBankStore.
class SampleBankStore
{
public:
    static constexpr int MaxSets = 32;   // set-count cap (12.1 AC3, kept)
    // Global RAM budget = the 12.1 worst case (32 × 60 s stereo @ 44.1 kHz float). Multisample
    // sets reshape how the budget is spent but can never exceed it (Story 12.2 AC4).
    static constexpr size_t kMaxStoreBytes = (size_t) MaxSets * 60 * 44100 * 2 * sizeof (float);

    static SampleBankStore& instance()
    {
        static SampleBankStore store;
        return store;
    }

    int getNumSets() const { return count.load (std::memory_order_acquire); }

    const SampleSet* getSet (int index) const
    {
        int n = count.load (std::memory_order_acquire);
        if (n == 0) return nullptr;
        index = juce::jlimit (0, n - 1, index);
        return sets[(size_t) index].get();
    }

    juce::StringArray getNames() const
    {
        juce::StringArray names;
        int n = count.load (std::memory_order_acquire);
        for (int i = 0; i < n; ++i)
            names.add (sets[(size_t) i]->getName());
        return names;
    }

    // Find a loaded set by name (case-insensitive). -1 if absent.
    int indexOf (const juce::String& name) const
    {
        int n = count.load (std::memory_order_acquire);
        for (int i = 0; i < n; ++i)
            if (sets[(size_t) i]->getName().equalsIgnoreCase (name))
                return i;
        return -1;
    }

    // Load a single audio file and append it. Returns the set index, or -1 on failure
    // (unreadable / over the caps / store full — `error`, when given, says which and why).
    // MESSAGE THREAD only. Duplicate-safe by name.
    int loadFile (const juce::File& file, juce::String* error = nullptr)
    {
        const int existing = indexOf (file.getFileNameWithoutExtension());
        if (existing >= 0) return existing;
        auto set = SampleSet::loadFromFile (file);
        if (set == nullptr)
        {
            if (error) *error = "\"" + file.getFileName() + "\" is unreadable or longer than 60 s";
            return -1;
        }
        return append (std::move (set), error);
    }

    // 12.2: load a folder of "<anything>_<note>.wav" files as ONE mapped set named after the
    // folder. Files without a note suffix are skipped; no parsable file ⇒ -1.
    int loadFolder (const juce::File& dir, juce::String* error = nullptr)
    {
        const int existing = indexOf (dir.getFileName());
        if (existing >= 0) return existing;
        juce::String err;
        auto set = SampleSet::loadFromEntries (dir.getFileName(),
                                               SampleMapping::entriesFromFolder (dir), err);
        if (set == nullptr) { if (error) *error = err; return -1; }
        return append (std::move (set), error);
    }

    // 12.2: import a minimal .sfz as ONE mapped set named after the file.
    int loadSfz (const juce::File& sfzFile, juce::String* error = nullptr)
    {
        const int existing = indexOf (sfzFile.getFileNameWithoutExtension());
        if (existing >= 0) return existing;
        juce::String err;
        auto set = SampleSet::loadFromEntries (sfzFile.getFileNameWithoutExtension(),
                                               SampleMapping::entriesFromSfz (sfzFile), err);
        if (set == nullptr) { if (error) *error = err; return -1; }
        return append (std::move (set), error);
    }

private:
    SampleBankStore() = default;

    // Publish a finished set (or reject it). Single choke point for the count/budget caps, so
    // every load path enforces them identically.
    int append (std::unique_ptr<SampleSet> set, juce::String* error = nullptr)
    {
        if (set == nullptr) return -1;
        int n = count.load (std::memory_order_acquire);
        if (n >= MaxSets)
        {
            if (error) *error = "the sample list is full (32 sets)";
            return -1;
        }
        if (bytesLoaded + set->totalBytes() > kMaxStoreBytes)
        {
            if (error) *error = "the sample memory budget is exhausted (unload is not supported "
                                "in this session — restart with fewer/smaller sets)";
            return -1;
        }
        bytesLoaded += set->totalBytes();
        sets[(size_t) n] = std::move (set);
        count.store (n + 1, std::memory_order_release);
        return n;
    }

    std::array<std::unique_ptr<SampleSet>, MaxSets> sets;
    std::atomic<int> count { 0 };
    size_t bytesLoaded = 0;   // message-thread only (all loads are)
};
