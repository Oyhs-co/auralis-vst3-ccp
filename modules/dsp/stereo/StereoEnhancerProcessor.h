// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "IDspProcessor.h"
#include "Smoother.h"

#include <atomic>

namespace auralis
{
// Slice 8 stereo stage: M/S decode with width, mid/side gains, balance and an
// output correlation meter.
class StereoEnhancerProcessor : public IDspProcessor
{
public:
    void prepare (const ProcessContext& ctx) override;
    void process (AudioBufferView& buffer) override;
    void reset() override;

    // Control methods: call once per block, never inside the sample loop.
    void setWidth (float w) noexcept;        // 0..2, 1 = normal
    void setMidGainDb (float db) noexcept;   // -12..+12
    void setSideGainDb (float db) noexcept;  // -12..+12
    void setBalance (float b) noexcept;      // -1..+1, unsmoothed; balance < 0 attenuates the left channel

    // Correlation of the last processed stereo block, -1..+1 (+1 is mono-compatible).
    float getCorrelation() const noexcept { return correlation.load (std::memory_order_relaxed); }
    const std::atomic<float>& getCorrelationAtomic() const noexcept { return correlation; }

private:
    Smoother widthSmoother;
    Smoother midGainSmoother;
    Smoother sideGainSmoother;
    float balance = 0.0f;
    std::atomic<float> correlation { 1.0f };
};
}
