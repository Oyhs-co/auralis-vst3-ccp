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

private:
    static constexpr int kMaxChannels = 2;

    std::vector<float> delay[kMaxChannels]; // per-channel ring, size == latency
    Smoother thresholdSmoother;             // smoothed threshold target, linear
    float sampleRate = 48000.0f;
    float releaseMs = 100.0f;
    float ceilingLin = 1.0f;
    float gain = 1.0f;                      // shared per-sample envelope
    int latency = 0;                        // lookahead L in samples
    int writePos = 0;                       // ring write position == oldest sample
    std::atomic<float> gainReductionDb { 0.0f };
};
}
