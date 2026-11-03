// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "IDspProcessor.h"

#include <array>

namespace auralis
{
// Slice 3 — Equalizer: 4 fixed RBJ biquad bands (low shelf, peaking, peaking, high shelf).
// Coefficients recomputed per block when dirty; process runs direct form I.
// shortcut: shelf/peaking freq and Q jump without smoothing, mild zipper on automation;
// upgrade: interpolate coefficients.
class EqualizerProcessor : public IDspProcessor
{
public:
    void prepare (const ProcessContext& ctx) override;
    void process (AudioBufferView& buffer) override;
    void reset() override;

    // Control methods: call once per block, never inside the sample loop.
    void setBand (int index, float freqHz, float gainDb, float q) noexcept;
    void setBandEnabled (int index, bool enabled) noexcept;

private:
    static constexpr int maxBands = 4;
    static constexpr int maxChannels = 2;

    struct Band
    {
        float freq = 0.0f;
        float gainDb = 0.0f;
        float q = 0.707f;
        bool enabled = true;
        bool dirty = true;
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
        float x1[maxChannels] = {};
        float x2[maxChannels] = {};
        float y1[maxChannels] = {};
        float y2[maxChannels] = {};
    };

    void updateBand (Band& b) noexcept;

    Band bands[maxBands];
    double sampleRate = 48000.0;
};
}