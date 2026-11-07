// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "IDspProcessor.h"
#include "Smoother.h"

#include <atomic>
#include <cstddef>
#include <vector>

namespace auralis
{
// Lookahead peak limiter: fixed 5 ms delay line per channel and one shared gain
// envelope. The lookahead exposes peaks before they reach the output, so attack
// can be instant; recovery is a one-pole release. The ceiling is a hard clamp
// that stays idle while the threshold does the work.
class LimiterProcessor : public IDspProcessor
{
public:
    LimiterProcessor() noexcept;

    void prepare (const ProcessContext& ctx) override;
    void process (AudioBufferView& buffer) override;
    void reset() override;

    // Control methods: call once per block, never inside the sample loop.
    void setThresholdDb (float db) noexcept; // -24..0 dB, smoothed over 5 ms
    void setCeilingDb (float db) noexcept;   // -12..0 dB, hard output clamp
    void setReleaseMs (float ms) noexcept;   // 5..1000 ms, recovery speed

    // Lookahead delay in samples; the plugin layer reports it to the host.
    int getLatencySamples() const noexcept { return latency; }
    // Positive dB of gain reduction applied during the last processed block.
    float getGainReductionDb() const noexcept;
    const std::atomic<float>& getGainReductionAtomic() const noexcept { return gainReductionDb; }

private:
    // Exact sliding-window maximum over one channel's last L+1 inputs, kept as
    // a monotonic non-increasing queue in a circular buffer (amortized O(1)
    // per sample). Signed indices: seq - L must not wrap before the window fills.
    struct SlidingMax
    {
        std::vector<float> val;      // queue values, capacity == L + 1
        std::vector<long long> idx;  // sequence index of each queued value
        int head = 0;                // front position == current window maximum
        int count = 0;               // live elements
    };

    std::vector<std::vector<float>> delay; // per-channel ring, size == latency
    std::vector<SlidingMax> window;        // per-channel sliding maximum state
    Smoother thresholdSmoother;             // smoothed threshold target, linear
    float sampleRate = 48000.0f;
    float releaseMs = 100.0f;
    float ceilingLin = 1.0f;
    float gain = 1.0f;                      // shared per-sample envelope
    int latency = 0;                        // lookahead L in samples
    int writePos = 0;                       // ring write position == oldest sample
    long long seq = 0;                      // input sample counter for window expiry
    std::atomic<float> gainReductionDb { 0.0f };
};
}
