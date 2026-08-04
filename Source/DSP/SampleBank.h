#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>
#include <atomic>
#include <memory>
#include <limits>
#include <cmath>
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
// hazard). Never-free is acceptable because the store is BOUNDED (12.2 caps, 12.5 numbers):
//   · per FILE/zone:  ≤ 60 s                      (kMaxSeconds, unchanged from 12.1)
//   · per SET total:  ≤ 3600 s across all zones   (kMaxSetSeconds — a 4-velocity-layer piano
//     is ~1700–2100 s; a runaway folder still fails early with a friendly message)
//   · GLOBAL budget:  ≤ kMaxStoreBytes = 4 GiB (12.5 user decision D2) — the actual RAM
//     protector, sized for two 4-layer grand pianos plus headroom.
// Global RAM budget — the hard protector of the never-free store. History: 12.1/12.2 pinned
// it to the old worst case (≈646 MiB); story 12.5 raised it to 4 GiB (user decision D2,
// 2026-08-04) so VELOCITY-LAYERED pianos fit (Splendid ×4 ≈ 557 MB + Salamander ×4 ≈ 690 MB
// decoded, plus headroom for bigger libraries). Namespace scope so loadZone can bound a
// single allocation against it BEFORE allocating. (size_t) first ⇒ 64-bit multiply.
inline constexpr size_t kMaxSampleStoreBytes = (size_t) 4 * 1024 * 1024 * 1024;

struct SampleZone
{
    std::vector<float> data[2];   // [0]=L (or mono), [1]=R (empty ⇒ mono); at the FILE's rate
    double fileSampleRate = 44100.0;
    int rootKey = 60;             // key that plays the file at original speed (mapped sets)
    int loKey = 0, hiKey = 127;   // inclusive key range (mapped sets; 0..127 for single samples)
    float releaseSeconds = -1.0f; // sfz ampeg_release — note-off fade (12.4); <0 ⇒ REL knob decides
    int   loVel = 0, hiVel = 127; // inclusive velocity layer (12.5; full range for single samples)
    float veltrack = 0.0f;        // 12.5 amp_veltrack as 0..1 — how much velocity scales the gain
    float gainLin  = 1.0f;        // 12.5 sfz volume= as a linear factor (layer balancing)
    double tuneRatio = 1.0;       // 12.5 sfz tune= as a rate factor 2^(cents/1200)

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

    // Zone for a played note (12.5: and its VELOCITY — layers are a zone dimension). Mapped:
    // the zone containing key AND velocity; anything not exactly covered (imported .sfz gaps)
    // falls back to the NEAREST zone — key distance dominates, velocity breaks ties — so every
    // key always sounds. Unmapped: the single zone. Audio-thread callable: linear scan, no
    // allocation (a 4-layer piano is ~230 zones, scanned once per note-on).
    const SampleZone* zoneFor (int midiNote, int velocity = 127) const
    {
        if (zones.empty()) return nullptr;
        const SampleZone* best = &zones.front();
        int bestDist = 1 << 24;
        for (const auto& z : zones)
        {
            const int kd = midiNote < z.loKey ? z.loKey - midiNote
                                              : (midiNote > z.hiKey ? midiNote - z.hiKey : 0);
            const int vd = velocity < z.loVel ? z.loVel - velocity
                                              : (velocity > z.hiVel ? velocity - z.hiVel : 0);
            const int dist = kd * 256 + vd;
            if (dist == 0)
                return &z;
            if (dist < bestDist) { bestDist = dist; best = &z; }
        }
        return best;
    }

    static constexpr double kMaxSeconds    = 60.0;    // per-file/zone cap (12.1 AC3, kept)
    // Per-set total cap. 300 → 900 s (single-layer chromatic pianos) → 3600 s (12.5: a 4-layer
    // piano is ~1700–2100 s); the GLOBAL byte budget (kMaxSampleStoreBytes) remains the hard
    // RAM protector — this cap only catches runaway folders early with a friendlier message.
    static constexpr double kMaxSetSeconds = 3600.0;

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
        // Corrupt-header guard (review 2026-08-04): a bogus 32-bit sampleRate (GHz range) can
        // pass the SECONDS cap with a huge lengthInSamples — bound the actual allocation in
        // 64-bit BEFORE it happens (int overflow / multi-GB alloc on the message thread).
        const juce::int64 len64  = reader->lengthInSamples;
        const juce::int64 bytes  = len64 * (reader->numChannels > 1 ? 2 : 1) * (juce::int64) sizeof (float);
        if (len64 > (juce::int64) std::numeric_limits<int>::max() / 8
            || bytes > (juce::int64) kMaxSampleStoreBytes)
            return false;

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
            // sfz offset=: drop pre-attack frames the library wants skipped (applied at load
            // time so playback code stays offset-free).
            if (e.offsetFrames > 0 && e.offsetFrames < z.getLength() - 4)
                for (auto& chan : z.data)
                    if (! chan.empty())
                        chan.erase (chan.begin(), chan.begin() + e.offsetFrames);
            z.rootKey = e.rootKey;
            z.loKey   = e.loKey;
            z.hiKey   = e.hiKey;
            z.releaseSeconds = e.releaseSeconds;
            z.loVel   = e.loVel;
            z.hiVel   = e.hiVel;
            z.veltrack  = juce::jlimit (0.0f, 100.0f, e.veltrack) / 100.0f;
            z.gainLin   = juce::Decibels::decibelsToGain (e.volumeDb);
            z.tuneRatio = std::pow (2.0, e.tuneCents / 1200.0);
            totalSeconds += (double) z.getLength() / z.fileSampleRate;
            if (totalSeconds > kMaxSetSeconds)
            {
                error = "the set exceeds 60 minutes of audio in total (reached at \""
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
    static constexpr size_t kMaxStoreBytes = kMaxSampleStoreBytes;   // budget: see namespace const

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
        // Duplicate-safe by name — but only within the SAME kind (review 2026-08-04): silently
        // returning a mapped set for a single-file load (or vice versa) flips a preset's set
        // identity across sessions, because preload order decides who claims the name first.
        const int existing = indexOf (file.getFileNameWithoutExtension());
        if (existing >= 0)
        {
            if (! getSet (existing)->isMapped()) return existing;
            if (error) *error = "a multisample set named \"" + file.getFileNameWithoutExtension()
                              + "\" is already loaded — rename the file";
            return -1;
        }
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
        if (existing >= 0)
        {
            if (getSet (existing)->isMapped()) return existing;
            if (error) *error = "a single sample named \"" + dir.getFileName()
                              + "\" is already loaded — rename the folder";
            return -1;
        }
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
        if (existing >= 0)
        {
            if (getSet (existing)->isMapped()) return existing;
            if (error) *error = "a single sample named \"" + sfzFile.getFileNameWithoutExtension()
                              + "\" is already loaded — rename the .sfz";
            return -1;
        }
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
