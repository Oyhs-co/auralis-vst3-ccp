// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "Fft.h"
#include "DspTypes.h"

#include <atomic>
#include <vector>

namespace auralis
{
// Read-only audio observer: meters the mono sum and keeps a log-band spectrum
// snapshot in plain atomics. Never writes samples, never locks, never allocates
// in process(). The GUI pulls the snapshot from any thread.
class SpectrumAnalyzer
{
public:
    static constexpr int numBands = 32;

    void prepare (double sampleRate, int maxBlockSize);
    void process (const AudioBufferView& buffer); // observes only
    void reset();

    // Relaxed-ordering snapshot, safe to read while audio runs.
    std::atomic<float> band[numBands] {};
    std::atomic<float> rmsDb { -100.0f };
    std::atomic<float> peakDb { -100.0f };
    std::atomic<float> peakHoldDb { -100.0f };

private:
    void analyze (const AudioBufferView& buffer);

    Fft fft { 1024 };
    std::vector<float> window;      // Hann, fft.size() samples
    std::vector<float> re, im;      // FFT scratch, fft.size() samples
    int bandEdges[numBands + 1] {}; // FFT bin index of each log edge, 20 Hz..20 kHz
    float bandNorm = 1.0f;          // 1 / (fft.size() / 4)
    int blockCounter = 0;
    float peakHold = 0.0f;
};
}
