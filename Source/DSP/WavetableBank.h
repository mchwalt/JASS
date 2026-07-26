#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>
#include <atomic>
#include <memory>
#include <cmath>

// A wavetable bank holds several "frames" (single-cycle waveforms). The
// Position parameter morphs between frames; phase indexes within a frame.
// Ported from the C# Synthy WavetableBank.
class WavetableBank
{
public:
    WavetableBank(juce::String bankName, std::vector<std::vector<float>> bankFrames)
        : name(std::move(bankName)), frames(std::move(bankFrames))
    {
        frameSize  = frames.empty() ? 0 : (int) frames[0].size();
        frameCount = (int) frames.size();
    }

    const juce::String& getName() const { return name; }

    // Bilinear interpolation: between frames (position) and between samples (phase).
    float getSample(double phase, double position) const
    {
        if (frameCount == 0 || frameSize == 0)
            return 0.0f;

        position = juce::jlimit(0.0, 1.0, position);

        double framePos = position * (frameCount - 1);
        int frameA = (int) framePos;
        int frameB = std::min(frameA + 1, frameCount - 1);
        double frameMix = framePos - frameA;

        double samplePos = std::fmod(phase, 1.0) * frameSize;
        if (samplePos < 0) samplePos += frameSize;
        int idxA = (int) samplePos;
        int idxB = (idxA + 1) % frameSize;
        double sampleMix = samplePos - idxA;

        float a = (float) (frames[(size_t) frameA][(size_t) idxA] * (1 - sampleMix)
                         + frames[(size_t) frameA][(size_t) idxB] * sampleMix);
        float b = (float) (frames[(size_t) frameB][(size_t) idxA] * (1 - sampleMix)
                         + frames[(size_t) frameB][(size_t) idxB] * sampleMix);

        return (float) (a * (1 - frameMix) + b * frameMix);
    }

    // ── Built-in bank generation (mathematically generated) ──
    static constexpr int DefaultFrameSize  = 2048;
    static constexpr int DefaultFrameCount = 16;

    static std::vector<std::unique_ptr<WavetableBank>> generateBuiltIns()
    {
        std::vector<std::unique_ptr<WavetableBank>> banks;
        banks.push_back(generateBasic());
        banks.push_back(generateDigital());
        banks.push_back(generateHarmonic());
        banks.push_back(generateVocal());
        banks.push_back(generatePWM());
        banks.push_back(generateSpectralSweep());
        return banks;
    }

    // Load a WAV file split into equal-sized frames. Returns nullptr on failure.
    static std::unique_ptr<WavetableBank> loadFromWav(const juce::File& file,
                                                      int frameSize = DefaultFrameSize)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(file));
        if (reader == nullptr)
            return nullptr;

        int total = (int) reader->lengthInSamples;
        if (total < frameSize)
            return nullptr;

        juce::AudioBuffer<float> buf(reader->numChannels > 0 ? (int) reader->numChannels : 1, total);
        reader->read(&buf, 0, total, 0, true, reader->numChannels > 1);
        const float* src = buf.getReadPointer(0); // mono / first channel

        int frameCount = std::max(1, total / frameSize);
        frameCount = std::min(frameCount, 256);

        std::vector<std::vector<float>> frames((size_t) frameCount,
                                               std::vector<float>((size_t) frameSize, 0.0f));
        for (int f = 0; f < frameCount; ++f)
        {
            int sourceOffset = f * (total / frameCount);
            float peak = 0.0f;
            for (int i = 0; i < frameSize; ++i)
            {
                int idx = sourceOffset + i;
                float s = (idx < total) ? src[idx] : 0.0f;
                frames[(size_t) f][(size_t) i] = s;
                peak = std::max(peak, std::abs(s));
            }
            if (peak > 0.001f)
                for (int i = 0; i < frameSize; ++i)
                    frames[(size_t) f][(size_t) i] /= peak;
        }

        return std::make_unique<WavetableBank>(file.getFileNameWithoutExtension(), std::move(frames));
    }

private:
    juce::String name;
    std::vector<std::vector<float>> frames;
    int frameSize = 0;
    int frameCount = 0;

    static double formantGain(int harmonic, double center, double width)
    {
        double dist = (harmonic - center) / width;
        return std::exp(-0.5 * dist * dist);
    }

    static std::unique_ptr<WavetableBank> generateBasic()
    {
        int frames = DefaultFrameCount;
        std::vector<std::vector<float>> data((size_t) frames, std::vector<float>((size_t) DefaultFrameSize));
        for (int f = 0; f < frames; ++f)
        {
            double t = (double) f / (frames - 1);
            for (int i = 0; i < DefaultFrameSize; ++i)
            {
                double phase = (double) i / DefaultFrameSize;
                double sine = std::sin(2 * juce::MathConstants<double>::pi * phase);
                double triangle = 2 * std::abs(2 * phase - 1) - 1;
                double saw = 2 * phase - 1;
                double square = phase < 0.5 ? 1.0 : -1.0;

                double value;
                if (t < 1.0 / 3.0)        { double m = t * 3;             value = sine * (1 - m) + triangle * m; }
                else if (t < 2.0 / 3.0)   { double m = (t - 1.0 / 3.0) * 3; value = triangle * (1 - m) + saw * m; }
                else                      { double m = (t - 2.0 / 3.0) * 3; value = saw * (1 - m) + square * m; }

                data[(size_t) f][(size_t) i] = (float) value;
            }
        }
        return std::make_unique<WavetableBank>("Basic", std::move(data));
    }

    static std::unique_ptr<WavetableBank> generateDigital()
    {
        int frames = DefaultFrameCount;
        std::vector<std::vector<float>> data((size_t) frames, std::vector<float>((size_t) DefaultFrameSize));
        for (int f = 0; f < frames; ++f)
        {
            double t = (double) f / (frames - 1);
            for (int i = 0; i < DefaultFrameSize; ++i)
            {
                double phase = (double) i / DefaultFrameSize;
                double value = 0;
                int partials = 2 + (int) (t * 14);
                for (int p = 1; p <= partials; ++p)
                {
                    double sign = (p % 2 == 0) ? -1 : 1;
                    double amp = sign / p * (1 + t * (p % 3));
                    value += amp * std::sin(2 * juce::MathConstants<double>::pi * phase * p);
                }
                data[(size_t) f][(size_t) i] = (float) juce::jlimit(-1.0, 1.0, value * 0.5);
            }
        }
        return std::make_unique<WavetableBank>("Digital", std::move(data));
    }

    static std::unique_ptr<WavetableBank> generateHarmonic()
    {
        int frames = DefaultFrameCount;
        std::vector<std::vector<float>> data((size_t) frames, std::vector<float>((size_t) DefaultFrameSize));
        for (int f = 0; f < frames; ++f)
        {
            int maxHarmonic = 1 + (int) ((double) f / (frames - 1) * 31);
            double peak = 0;
            for (int i = 0; i < DefaultFrameSize; ++i)
            {
                double phase = (double) i / DefaultFrameSize;
                double value = 0;
                for (int h = 1; h <= maxHarmonic; ++h)
                    value += std::sin(2 * juce::MathConstants<double>::pi * phase * h) / h;
                data[(size_t) f][(size_t) i] = (float) value;
                peak = std::max(peak, std::abs(value));
            }
            if (peak > 0.001)
                for (int i = 0; i < DefaultFrameSize; ++i)
                    data[(size_t) f][(size_t) i] /= (float) peak;
        }
        return std::make_unique<WavetableBank>("Harmonic", std::move(data));
    }

    static std::unique_ptr<WavetableBank> generateVocal()
    {
        int frames = DefaultFrameCount;
        std::vector<std::vector<float>> data((size_t) frames, std::vector<float>((size_t) DefaultFrameSize));
        const double formants[4][3] = { {1,4,8}, {1,8,14}, {1,3,10}, {1,2,6} };
        const int numVowels = 4;

        for (int f = 0; f < frames; ++f)
        {
            double t = (double) f / (frames - 1) * (numVowels - 1);
            int vowelA = (int) t;
            int vowelB = std::min(vowelA + 1, numVowels - 1);
            double vowelMix = t - vowelA;

            double f1 = formants[vowelA][0] * (1 - vowelMix) + formants[vowelB][0] * vowelMix;
            double f2 = formants[vowelA][1] * (1 - vowelMix) + formants[vowelB][1] * vowelMix;
            double f3 = formants[vowelA][2] * (1 - vowelMix) + formants[vowelB][2] * vowelMix;

            double peak = 0;
            for (int i = 0; i < DefaultFrameSize; ++i)
            {
                double phase = (double) i / DefaultFrameSize;
                double value = 0;
                for (int h = 1; h <= 20; ++h)
                {
                    double env = formantGain(h, f1, 1.0)
                               + formantGain(h, f2, 1.5) * 0.7
                               + formantGain(h, f3, 2.0) * 0.4;
                    value += env * std::sin(2 * juce::MathConstants<double>::pi * phase * h);
                }
                data[(size_t) f][(size_t) i] = (float) value;
                peak = std::max(peak, std::abs(value));
            }
            if (peak > 0.001)
                for (int i = 0; i < DefaultFrameSize; ++i)
                    data[(size_t) f][(size_t) i] /= (float) peak;
        }
        return std::make_unique<WavetableBank>("Vocal", std::move(data));
    }

    static std::unique_ptr<WavetableBank> generatePWM()
    {
        int frames = DefaultFrameCount;
        std::vector<std::vector<float>> data((size_t) frames, std::vector<float>((size_t) DefaultFrameSize));
        for (int f = 0; f < frames; ++f)
        {
            double pulseWidth = 0.5 - (double) f / (frames - 1) * 0.45;
            for (int i = 0; i < DefaultFrameSize; ++i)
            {
                double phase = (double) i / DefaultFrameSize;
                double value = 0;
                for (int h = 1; h <= 32; ++h)
                    value += std::sin(2 * juce::MathConstants<double>::pi * h * phase)
                           - std::sin(2 * juce::MathConstants<double>::pi * h * (phase - pulseWidth));
                data[(size_t) f][(size_t) i] = (float) juce::jlimit(-1.0, 1.0, value / 8.0);
            }
        }
        return std::make_unique<WavetableBank>("PWM", std::move(data));
    }

    static std::unique_ptr<WavetableBank> generateSpectralSweep()
    {
        int frames = DefaultFrameCount;
        std::vector<std::vector<float>> data((size_t) frames, std::vector<float>((size_t) DefaultFrameSize));
        for (int f = 0; f < frames; ++f)
        {
            double centerHarmonic = 1 + (double) f / (frames - 1) * 15;
            double peak = 0;
            for (int i = 0; i < DefaultFrameSize; ++i)
            {
                double phase = (double) i / DefaultFrameSize;
                double value = 0;
                for (int h = 1; h <= 24; ++h)
                {
                    double dist = (h - centerHarmonic) / 2.0;
                    double amp = std::exp(-0.5 * dist * dist);
                    value += amp * std::sin(2 * juce::MathConstants<double>::pi * phase * h);
                }
                data[(size_t) f][(size_t) i] = (float) value;
                peak = std::max(peak, std::abs(value));
            }
            if (peak > 0.001)
                for (int i = 0; i < DefaultFrameSize; ++i)
                    data[(size_t) f][(size_t) i] /= (float) peak;
        }
        return std::make_unique<WavetableBank>("Spectral", std::move(data));
    }
};

// Shared store of wavetable banks (built-ins + user-loaded WAVs).
// Single instance shared across all polyphonic voices. Lock-free for the
// audio thread: banks live in a fixed array with stable addresses, and an
// atomic count guards visibility of newly loaded banks.
class WavetableBankStore
{
public:
    static constexpr int MaxBanks = 64;

    static WavetableBankStore& instance()
    {
        static WavetableBankStore store;
        return store;
    }

    int getNumBanks() const { return count.load(std::memory_order_acquire); }

    const WavetableBank* getBank(int index) const
    {
        int n = count.load(std::memory_order_acquire);
        if (n == 0) return nullptr;
        index = juce::jlimit(0, n - 1, index);
        return banks[(size_t) index].get();
    }

    juce::StringArray getNames() const
    {
        juce::StringArray names;
        int n = count.load(std::memory_order_acquire);
        for (int i = 0; i < n; ++i)
            names.add(banks[(size_t) i]->getName());
        return names;
    }

    // Load a WAV and append it. Returns the bank index, or -1 on failure. Call from the message
    // thread only. Duplicate-safe: if a bank with the same name (the file's stem) is already
    // present, that existing index is returned instead of appending a copy — so repeatedly loading
    // the same file just re-selects it rather than growing the list. (No in-place replace, so the
    // audio thread never sees a slot mutated under it.)
    int loadWav(const juce::File& file)
    {
        const juce::String name = file.getFileNameWithoutExtension();
        int n = count.load(std::memory_order_acquire);
        for (int i = 0; i < n; ++i)
            if (banks[(size_t) i] != nullptr && banks[(size_t) i]->getName().equalsIgnoreCase(name))
                return i;   // already loaded → re-select, don't duplicate

        if (n >= MaxBanks) return -1;
        auto bank = WavetableBank::loadFromWav(file);
        if (bank == nullptr) return -1;
        banks[(size_t) n] = std::move(bank);
        count.store(n + 1, std::memory_order_release);
        return n;
    }

    // Drop every user-LOADED bank from the VISIBLE list, keeping only the built-ins (WAVETABLE reset).
    // Message-thread only. We ONLY lower the visible count here — we do NOT free the user banks.
    // A voice caches the raw WavetableBank* returned by getBank() for the whole render block, so
    // freeing a slot while the audio thread may still be dereferencing it is a use-after-free (the
    // atomic count doesn't protect an already-cached pointer). Lowering count (release) makes getBank
    // clamp back into the built-in range within one block, so no voice points past builtInCount
    // afterwards; the stale unique_ptrs stay alive and are reclaimed only when a later loadWav
    // overwrites their slot — provably unreferenced by then (deferred reclamation). Bounded by
    // MaxBanks, so this cannot grow without limit.
    void resetToBuiltIns()
    {
        count.store(builtInCount, std::memory_order_release);
    }

private:
    WavetableBankStore()
    {
        auto builtIns = WavetableBank::generateBuiltIns();
        int n = 0;
        for (auto& b : builtIns)
            banks[(size_t) n++] = std::move(b);
        builtInCount = n;
        count.store(n, std::memory_order_release);
    }
    int builtInCount = 0;   // number of shipped built-in banks (the list resetToBuiltIns restores)

    std::array<std::unique_ptr<WavetableBank>, MaxBanks> banks;
    std::atomic<int> count{0};
};
