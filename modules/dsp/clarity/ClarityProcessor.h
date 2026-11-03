// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "IDspProcessor.h"
#include "Smoother.h"

namespace auralis
{
// Slice 5 — Clarity/Fidelity: parallel harmonic exciter.
// out = comp * (dry + amount * presenceGain * softClip(highpass(dry))),
// softClip(y) = tanh(2y)/tanh(2), comp = 1/(1 + 0.15*amount).
class ClarityProcessor : public IDspProcessor
{
public:
    void prepare (const ProcessContext& ctx) override;
    void process (AudioBufferView& buffer) override;
    void reset() override;

    // Control methods: call once per block, never inside the sample loop.
    void setAmount (float amount01) noexcept;    // 0..1, smoothed 30 ms in the linear domain
    void setCenterFreq (float freqHz) noexcept;  // 1000..8000 Hz, biquad recomputed immediately
    void setPresenceDb (float db) noexcept;      // -6..+6 dB, linear target smoothed 20 ms

private:
    void updateCoefficients() noexcept;

    static constexpr int maxChannels = 2;

    Smoother amount;    // exciter drive 0..1
    Smoother presence;  // linear gain 10^(dB/20)
    double centerFreq = 3000.0;
    double sampleRate = 48000.0;
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0; // RBJ highpass, direct form I
    double x1[maxChannels] = {}, x2[maxChannels] = {};
    double y1[maxChannels] = {}, y2[maxChannels] = {};
};
}
