// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "IDspProcessor.h"
#include "Smoother.h"

namespace auralis
{
// Slice 4 — Bass Enhancer: low-shelf biquad + optional sub-highpass + soft saturation + level compensation.
// shortcut: sub protection is a single 12 dB/oct biquad, upgrade: cascade two stages for 24 dB/oct.
class BassEnhancerProcessor : public IDspProcessor
{
public:
    void prepare (const ProcessContext& ctx) override;
    void process (AudioBufferView& buffer) override;
    void reset() override;

    // Control methods: call once per block, never inside the sample loop.
    void setShelfFreq (float freqHz) noexcept;
    void setShelfGainDb (float gainDb) noexcept;
    void setSaturation (float amount01) noexcept;       // 0..1, smoothed 20 ms
    void setSubHighpassFreq (float freqHz) noexcept;   // 0 = off, else 10..60 Hz

private:
    static constexpr int maxChannels = 2;

    struct Biquad
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
        float x1[maxChannels] = {};
        float x2[maxChannels] = {};
        float y1[maxChannels] = {};
        float y2[maxChannels] = {};
    };

    void updateShelf() noexcept;
    void updateSubHighpass() noexcept;

    Biquad shelf;
    Biquad subHp;

    Smoother shelfB0Smoother;
    Smoother shelfB1Smoother;
    Smoother shelfB2Smoother;
    Smoother shelfA1Smoother;
    Smoother shelfA2Smoother;
    Smoother satSmoother;       // smooths saturation amount 0..1

    float shelfGainDb = 0.0f;   // target dB for compensation formula
    float shelfFreq = 100.0f;
    float subHpFreq = 0.0f;
    double sampleRate = 48000.0;
};
}