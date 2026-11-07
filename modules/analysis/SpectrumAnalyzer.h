// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "Fft.h"
#include "DspTypes.h"

#include <atomic>
#include <vector>

namespace auralis
{
// Read-only audio observer: meters the mono sum and keeps a log-band spectrum
// snapshot in plain atomics. process() (audio thread) only appends to a
// double-buffered ring and flips an index; analyzePending() (message thread)
// runs the window/FFT/band mapping and publishes normalized dB bands. Never
// writes samples, never locks, never allocates after prepare().
class SpectrumAnalyzer
{
public:
    static constexpr int numBands = 32;

    void prepare (double sampleRate, int maxBlockSize);
    void process (const AudioBufferView& buffer); // audio thread: meters + ring append only
    void reset();
    void analyzePending(); // message thread: Hann window + FFT + band publication

    // Relaxed-ordering snapshot, safe to read while audio runs.
    std::atomic<float> band[numBands] {};
    std::atomic<float> rmsDb { -100.0f };
    std::atomic<float> peakDb { -100.0f };
    std::atomic<float> peakHoldDb { -100.0f };

private:
    void publishBands (int ringIndex);

    Fft fft { 1024 };
    std::vector<float> window;      // Hann, fft.size() samples
    std::vector<float> rings[2];    // mono-sum history, double-buffered
    std::atomic<int> publishedRing { 0 };
    std::vector<float> re, im;      // FFT scratch, message thread only
    int bandEdges[numBands + 1] {}; // FFT bin index of each log edge, 20 Hz..20 kHz
    int ringPos[2] { 0, 0 };        // next write index per ring (oldest sample)
    float bandNorm = 1.0f;          // 1 / (fft.size() / 4)
    int blockCounter = 0;
    float peakHold = 0.0f;
};
}
