// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "IDspProcessor.h"
#include "Smoother.h"

#include <atomic>

namespace auralis
{
// Feed-forward compressor, stereo-linked: one shared gain for all channels,
// detected on the loudest channel (peak, or ~10 ms RMS). Transparent at defaults.
class CompressorProcessor : public IDspProcessor
{
public:
    CompressorProcessor() noexcept
    {
        ratioSmoother.setTarget (1.0f); // defaults: transparent from sample 0
        kneeSmoother.setTarget (6.0f);
    }

    void prepare (const ProcessContext& ctx) override;
    void process (AudioBufferView& buffer) override;
    void reset() override;

    // Control methods: call once per block, never inside the sample loop.
    void setThresholdDb (float db) noexcept;  // -60..0, smoothed 10 ms in dB domain
    void setRatio (float ratio) noexcept;     // 1..20, smoothed 20 ms
    void setAttackMs (float ms) noexcept;     // 1..100
    void setReleaseMs (float ms) noexcept;    // 10..1000
    void setKneeDb (float db) noexcept;       // 0..24, smoothed 20 ms
    void setMakeupDb (float db) noexcept;     // 0..24, smoothed 20 ms in dB domain
    void setRmsMode (bool useRms) noexcept;   // false = peak detector

    // Positive value = amount of gain reduction, updated once per block.
    float getGainReductionDb() const noexcept { return grDb.load (std::memory_order_relaxed); }
    const std::atomic<float>& getGainReductionAtomic() const noexcept { return grDb; }

private:
    void updateCoefficients() noexcept;

    Smoother thresholdSmoother; // 10 ms, dB domain
    Smoother makeupSmoother;    // 20 ms, dB domain
    Smoother ratioSmoother;     // 20 ms; slope is derived per sample (1/ratio - 1, <= 0)
    Smoother kneeSmoother;      // 20 ms
    float sampleRate = 48000.0f;
    float attackMs = 10.0f;
    float releaseMs = 100.0f;
    float coeffAttack = 0.0f;
    float coeffRelease = 0.0f;
    float detCoeff = 0.0f;   // RMS detector one-pole coefficient (~10 ms window)
    float gainDb = 0.0f;     // smoothed applied gain in dB, includes makeup
    float detectorSq = 0.0f; // one-pole state of x^2 (RMS mode)
    bool rmsMode = false;
    std::atomic<float> grDb { 0.0f };
};
} // namespace auralis
