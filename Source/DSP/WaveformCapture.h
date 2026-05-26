#pragma once
#include <vector>
#include <atomic>
#include <cstring>

class WaveformCapture
{
public:
    // displayLength = what we show, ringSize = larger buffer to find trigger point
    WaveformCapture(int displayLength = 512)
        : displayLen(displayLength),
          ringSize(displayLength * 3),
          ring(displayLength * 3, 0.0f),
          snapshot(displayLength, 0.0f) {}

    // Called from audio thread
    void writeSample(float sample)
    {
        ring[writePos] = sample;
        writePos = (writePos + 1) % ringSize;
    }

    // Called from GUI thread at ~30fps
    void updateSnapshot()
    {
        // Copy ring buffer to temp
        std::vector<float> temp(ringSize);
        int pos = writePos; // read current write pos (atomic-ish, close enough)
        for (int i = 0; i < ringSize; ++i)
            temp[i] = ring[(pos + i) % ringSize];

        // Find trigger: first positive zero-crossing in the last 2/3 of the buffer
        // This gives us a stable starting point
        int searchStart = ringSize - displayLen * 2;
        int triggerPos = ringSize - displayLen; // fallback

        for (int i = searchStart; i < ringSize - displayLen; ++i)
        {
            if (temp[i] <= 0.0f && temp[i + 1] > 0.0f)
            {
                triggerPos = i + 1;
                break;
            }
        }

        // Copy displayLen samples starting from trigger
        for (int i = 0; i < displayLen; ++i)
            snapshot[i] = temp[triggerPos + i];
    }

    const std::vector<float>& getSnapshot() const { return snapshot; }
    int getLength() const { return displayLen; }

private:
    int displayLen;
    int ringSize;
    std::vector<float> ring;
    std::vector<float> snapshot;
    int writePos = 0;
};
