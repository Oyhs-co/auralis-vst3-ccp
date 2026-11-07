// SPDX-License-Identifier: AGPL-3.0-only

#include "SpectrumAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace auralis
{
namespace
{
// Peak hold falls by this much per block while no higher peak appears.
constexpr float holdDecayPerBlock = 0.005f;
constexpr double kPi = 3.14159265358979323846;
constexpr float dbFloor = -100.0f;

// NaN-safe: anything not strictly positive (NaN included) lands on the floor.
void storeDb (std::atomic<float>& atom, float value)
{
    const float db = value > 0.0f ? 20.0f * std::log10 (value) : dbFloor;
    atom.store (std::max (dbFloor, db), std::memory_order_relaxed);
}
}

void SpectrumAnalyzer::prepare (double sampleRate, int /*maxBlockSize*/)
{
    const int n = fft.size();
    window.resize ((std::size_t) n);
    ring.resize ((std::size_t) n);
    re.resize ((std::size_t) n);
    im.resize ((std::size_t) n);
    for (int i = 0; i < n; ++i)
        window[(std::size_t) i] = 0.5f - 0.5f * (float) std::cos (2.0 * kPi * (double) i / (double) (n - 1));

    // 33 log-spaced edges from 20 Hz to 20 kHz; bands below the FFT
    // resolution map to an empty bin range and simply read 0.
    for (int j = 0; j <= numBands; ++j)
    {
        const double freq = 20.0 * std::pow (1000.0, (double) j / (double) numBands);
        const double bin = freq * (double) n / sampleRate;
        bandEdges[j] = std::clamp ((int) bin, 0, n / 2);
    }
    bandNorm = 4.0f / (float) n;

    reset();
}

void SpectrumAnalyzer::reset()
{
    blockCounter = 0;
    ringPos = 0;
    peakHold = 0.0f;
    std::fill (ring.begin(), ring.end(), 0.0f);
    for (auto& b : band)
        b.store (0.0f, std::memory_order_relaxed);
    rmsDb.store (dbFloor, std::memory_order_relaxed);
    peakDb.store (dbFloor, std::memory_order_relaxed);
    peakHoldDb.store (dbFloor, std::memory_order_relaxed);
}

void SpectrumAnalyzer::process (const AudioBufferView& buffer)
{
    const int nf = buffer.numFrames();
    if (nf <= 0 || buffer.numChannels() <= 0)
        return;

    // Mono sum, read-only: average the first two channels, single channel as-is.
    const int nch = std::min (buffer.numChannels(), 2);
    const float* ch0 = buffer.channel (0);
    const float* ch1 = nch > 1 ? buffer.channel (1) : nullptr;
    const int n = fft.size();
    const bool haveRing = ! ring.empty(); // before prepare() there is no history to fill

    double sumSquares = 0.0;
    float peak = 0.0f;
    for (int i = 0; i < nf; ++i)
    {
        const float s = ch1 != nullptr ? 0.5f * (ch0[i] + ch1[i]) : ch0[i];
        sumSquares += (double) s * (double) s;
        if (s > peak) peak = s;
        else if (-s > peak) peak = -s;
        if (haveRing)
        {
            ring[(std::size_t) ringPos] = s;
            if (++ringPos == n) ringPos = 0;
        }
    }

    peakHold = std::max (peak, peakHold - holdDecayPerBlock);
    storeDb (rmsDb, (float) std::sqrt (sumSquares / (double) nf));
    storeDb (peakDb, peak);
    storeDb (peakHoldDb, peakHold);

    // FFT on every 2nd block; metering above runs on every block.
    if ((blockCounter++ & 1) == 0)
        analyze();
}

void SpectrumAnalyzer::analyze()
{
    if (re.empty())
        return; // process() called before prepare()

    const int n = fft.size();

    // Window the ring oldest-first so the taper always covers the most recent
    // n samples, whatever the block size.
    for (int i = 0; i < n; ++i)
    {
        re[(std::size_t) i] = ring[(std::size_t) ((ringPos + i) % n)] * window[(std::size_t) i];
        im[(std::size_t) i] = 0.0f;
    }

    fft.transform (re.data(), im.data());

    // Max magnitude per log band; a full-scale sine peaks at ~1.0 (A*N/4).
    // EMA across analyses smooths band jitter for the GUI display.
    for (int b = 0; b < numBands; ++b)
    {
        float mag = 0.0f;
        for (int k = bandEdges[b]; k < bandEdges[b + 1]; ++k)
        {
            const float m = std::sqrt (re[k] * re[k] + im[k] * im[k]);
            if (m > mag) mag = m;
        }
        const float value = std::min (1.0f, mag * bandNorm);
        const float prev = band[b].load (std::memory_order_relaxed);
        band[b].store (0.7f * value + 0.3f * prev, std::memory_order_relaxed);
    }
}
}
