#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>
#include <atomic>
#include <memory>

// ── SAMPLER sample sets (Story 12.1) ─────────────────────────────────────────────────────────
// A SampleSet is ONE user recording (one-shot / texture / loop), loaded whole into RAM. STEREO
// files stay stereo (user decision 2026-07-30 — no downmix): the voice renders a stereo set as
// TWO panned sub-sources (PanSamplerL/R), so each channel gets its own equal-power/binaural/HRTF
// placement; a mono downmix happens only where the engine itself is mono (Mono / Pseudo-Stereo
// output modes). Mono files hold one channel and play like any other mono generator.
//
// The store copies the RT-hardened WavetableBankStore mechanics verbatim (that file documents the
// reasoning): loading on the MESSAGE THREAD only, visibility to the audio thread via an atomic
// count with release/acquire, append-only slots with stable addresses, duplicate-safe by name,
// and NO freeing (a voice caches the raw pointer for a render block — use-after-free hazard).
// Never-free is acceptable here because v1 is BOUNDED: max ~60 s per file, max 32 sets — the
// decided caps from the story's scope decision (multisample sets would change this math → 12.2).
class SampleSet
{
public:
    SampleSet(juce::String setName, std::vector<float> left, std::vector<float> right,
              double sourceSampleRate)
        : name(std::move(setName)), fileSampleRate(sourceSampleRate)
    {
        data[0] = std::move(left);
        data[1] = std::move(right);   // empty ⇒ mono
    }

    const juce::String& getName() const { return name; }
    bool   isStereo() const             { return ! data[1].empty(); }
    const float* getData(int ch) const  { return data[isStereo() && ch != 0 ? 1 : 0].data(); }
    int    getLength() const            { return (int) data[0].size(); }
    double getFileSampleRate() const    { return fileSampleRate; }

    static constexpr double kMaxSeconds = 60.0;   // per-file cap (story AC3)

    // Load a WAV/AIFF whole (first two channels). nullptr on unreadable/empty/over-cap files.
    static std::unique_ptr<SampleSet> loadFromFile(const juce::File& file)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(file));
        if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0)
            return nullptr;
        if ((double) reader->lengthInSamples / reader->sampleRate > kMaxSeconds)
            return nullptr;   // over the cap — reject outright (no silent truncation)

        const int total = (int) reader->lengthInSamples;
        const int nCh   = juce::jmax(1, (int) reader->numChannels);
        juce::AudioBuffer<float> buf(nCh, total);
        reader->read(&buf, 0, total, 0, true, nCh > 1);

        std::vector<float> left(buf.getReadPointer(0), buf.getReadPointer(0) + total);
        std::vector<float> right;
        if (nCh > 1)
            right.assign(buf.getReadPointer(1), buf.getReadPointer(1) + total);
        return std::make_unique<SampleSet>(file.getFileNameWithoutExtension(), std::move(left),
                                           std::move(right), reader->sampleRate);
    }

private:
    juce::String name;
    std::vector<float> data[2];   // [0]=L (or mono), [1]=R (empty ⇒ mono); at the FILE's rate
    double fileSampleRate;
};

// Shared store — single instance across all voices. Same lock-free contract as WavetableBankStore.
class SampleBankStore
{
public:
    static constexpr int MaxSets = 32;   // set-count cap (story AC3)

    static SampleBankStore& instance()
    {
        static SampleBankStore store;
        return store;
    }

    int getNumSets() const { return count.load(std::memory_order_acquire); }

    const SampleSet* getSet(int index) const
    {
        int n = count.load(std::memory_order_acquire);
        if (n == 0) return nullptr;
        index = juce::jlimit(0, n - 1, index);
        return sets[(size_t) index].get();
    }

    juce::StringArray getNames() const
    {
        juce::StringArray names;
        int n = count.load(std::memory_order_acquire);
        for (int i = 0; i < n; ++i)
            names.add(sets[(size_t) i]->getName());
        return names;
    }

    // Find a loaded set by name (case-insensitive). -1 if absent.
    int indexOf(const juce::String& name) const
    {
        int n = count.load(std::memory_order_acquire);
        for (int i = 0; i < n; ++i)
            if (sets[(size_t) i]->getName().equalsIgnoreCase(name))
                return i;
        return -1;
    }

    // Load a file and append it. Returns the set index, or -1 on failure (unreadable / over the
    // 60 s cap / store full). MESSAGE THREAD only. Duplicate-safe by name: re-loading re-selects.
    int loadFile(const juce::File& file)
    {
        const int existing = indexOf(file.getFileNameWithoutExtension());
        if (existing >= 0) return existing;

        int n = count.load(std::memory_order_acquire);
        if (n >= MaxSets) return -1;
        auto set = SampleSet::loadFromFile(file);
        if (set == nullptr) return -1;
        sets[(size_t) n] = std::move(set);
        count.store(n + 1, std::memory_order_release);
        return n;
    }

private:
    SampleBankStore() = default;

    std::array<std::unique_ptr<SampleSet>, MaxSets> sets;
    std::atomic<int> count{0};
};
