// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "IDspProcessor.h"

namespace auralis
{
// Slice 3 — Equalizer: 4 fixed RBJ biquad bands (low shelf, peaking, peaking, high shelf).
// Target coefficients are recomputed per block only for bands whose values changed;
// current coefficients per-sample smooth toward the targets (15 ms) so automation is
// zipper-free. Process runs direct form I.
class EqualizerProcessor : public IDspProcessor
{
public:
    void prepare (const ProcessContext& ctx) override;
    void process (AudioBufferView& buffer) override;
    void reset() override;

    // Control methods: call once per block, never inside the sample loop.
    void setBand (int index, float freqHz, float gainDb, float q) noexcept;
    void setBandEnabled (int index, bool enabled) noexcept;

    // Message-thread magnitude response (dB) of the enabled bands, evaluated at
    // each requested frequency for the GUI curve. Read-only snapshot: the audio
    // thread may write the band values concurrently; a torn read costs one
    // wrong curve frame at worst.
    void getMagnitudeResponse (const double* freqHz, double* dbOut, int numPoints) const;

private:
    static constexpr int maxBands = 4;
    static constexpr int maxChannels = 2;
    static constexpr double smoothTimeSeconds = 0.005; // coefficient lerp; perceived gain settling ≈ 10× this

    struct Band
    {
        float freq = 0.0f;
        float gainDb = 0.0f;
        float q = 0.707f;
        bool enabled = true;
        bool dirty = true; // coefficients still converging toward target
        // Target coefficients (written by updateBand) and current coefficients (used by the
        // sample loop, smoothed per sample). Current starts as identity: a gain-0 band has
        // mathematically identity targets with b==a bitwise, so no audible ramp occurs.
        double tb0 = 1.0, tb1 = 0.0, tb2 = 0.0, ta1 = 0.0, ta2 = 0.0;
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
        float x1[maxChannels] = {};
        float x2[maxChannels] = {};
        float y1[maxChannels] = {};
        float y2[maxChannels] = {};
    };

    void updateBand (Band& b) noexcept;

    Band bands[maxBands];
    double sampleRate = 48000.0;
    double smoothAlpha = 0.0;
};
}