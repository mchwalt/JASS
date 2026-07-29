#pragma once
#include <vector>
#include <atomic>
#include <cmath>

// Stereo capture for the OSCILLOSCOPE + SPECTRUM displays (Story 10.6). Captures the L/R bus
// AFTER the master-bus stages (compressor, widener, Binaural/Kunstkopf, ROOM reflections) so the
// displays show what actually reaches the headphones — before, the tap sat ahead of the whole
// master section and everything it did was invisible. Channel pairs are written together and
// share one write index, so the two snapshots stay sample-aligned.
class WaveformCapture
{
public:
    // displayLength = snapshot capacity in samples: it must cover the LONGEST scope window
    // (the WaveformDisplay shows only the first `windowMs/1000 * sampleRate` of it), so pick
    // 50 ms × the highest expected sample rate. ringSize = 3× that, to find a stable trigger.
    WaveformCapture(int displayLength = 512)
        : displayLen(displayLength),
          ringSize(displayLength * 3)
    {
        for (int c = 0; c < 2; ++c)
        {
            ring[c].assign((size_t) ringSize, 0.0f);
            temp[c].assign((size_t) ringSize, 0.0f);
            snapshot[c].assign((size_t) displayLen, 0.0f);
        }
    }

    // Called from audio thread — one L/R pair per sample, single shared index (aligned channels).
    void writeSample(float l, float r)
    {
        const int p = writePos.load(std::memory_order_relaxed);
        ring[0][(size_t) p] = l;
        ring[1][(size_t) p] = r;
        writePos.store((p + 1) % ringSize, std::memory_order_release);   // publish (paired acquire in updateSnapshot)
    }

    // Called from GUI thread at ~30fps
    void updateSnapshot()
    {
        const int pos = writePos.load(std::memory_order_acquire);   // paired with the release in writeSample
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < ringSize; ++i)
                temp[c][(size_t) i] = ring[c][(size_t) ((pos + i) % ringSize)];

        // Mono content (Mono mode, or every generator centred) => the displays collapse to a
        // single trace instead of painting two identical lines over each other. Detected on the
        // ALIGNED ring copies (same index = same instant), BEFORE the per-channel trigger cuts.
        {
            float maxDiff = 0.0f;
            for (int i = ringSize - displayLen; i < ringSize; ++i)
                maxDiff = std::max(maxDiff, std::fabs(temp[0][(size_t) i] - temp[1][(size_t) i]));
            stereoContent = maxDiff > 1.0e-3f;
        }

        // PER-CHANNEL trigger (like a two-channel scope in alternate-trigger mode): each channel
        // is cut at its own trigger point, so L and R each stand still even when they carry
        // UNRELATED signals (e.g. sine left, saw right — a shared sum trigger has no stable
        // period there and the traces jumped). The two snapshots are therefore NOT mutually
        // time-aligned; for unrelated signals no meaningful common alignment exists anyway.
        // HYSTERESIS per trigger (arm below −thr, fire above +thr, thr = 5% of the channel's
        // search-region peak): the post-FX signal (reflections, compressor) carries small ripple
        // zero-crossings that a plain zero-cross trigger jumped on ("nervous" trace).
        const int searchStart = ringSize - displayLen * 2;
        for (int c = 0; c < 2; ++c)
        {
            int triggerPos = ringSize - displayLen; // fallback

            float peak = 0.0f;
            for (int i = searchStart; i < ringSize; ++i)
                peak = std::max(peak, std::fabs(temp[c][(size_t) i]));
            const float thr = peak * 0.05f;

            bool armed = false;
            for (int i = searchStart; i < ringSize - displayLen; ++i)
            {
                const float s = temp[c][(size_t) i];
                if (! armed)
                {
                    if (s <= -thr) armed = true;
                }
                else if (s > thr)
                {
                    triggerPos = i;
                    break;
                }
            }

            for (int i = 0; i < displayLen; ++i)
                snapshot[c][(size_t) i] = temp[c][(size_t) (triggerPos + i)];
        }
    }

    const std::vector<float>& getSnapshot(int channel = 0) const { return snapshot[channel == 0 ? 0 : 1]; }
    bool  isStereoContent() const { return stereoContent; }   // valid after updateSnapshot (GUI thread)
    int   getLength() const { return displayLen; }

    // Sample rate for the display side (scope ms-window, spectrum bin→Hz). Set from
    // prepareToPlay; read by the displays. Plain atomic — no audio-behaviour impact.
    void setSampleRate(double sr) { sampleRate.store(sr); }
    double getSampleRate() const { return sampleRate.load(); }

private:
    std::atomic<double> sampleRate { 44100.0 };
    int displayLen;
    int ringSize;
    std::vector<float> ring[2];
    std::vector<float> temp[2];       // GUI-thread scratch (member: no per-frame allocation)
    std::vector<float> snapshot[2];
    bool stereoContent = false;       // GUI thread only (written+read in/after updateSnapshot)
    std::atomic<int> writePos { 0 };  // audio writes (release), GUI reads (acquire) — no torn index
};
