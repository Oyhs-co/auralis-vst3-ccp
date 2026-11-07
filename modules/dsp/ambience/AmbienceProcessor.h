// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "IDspProcessor.h"
#include "Smoother.h"

#include <vector>

namespace auralis
{
// Slice 7 ambience: 8-line feedback delay network reverb. Mono-summed input
// runs through two fixed one-sample allpass diffusers and a predelay into the
// network; Householder feedback with per-line t60 gains and one-pole damping
// shapes the tail; stereo wet buses get width control and dry/wet mixing.
class AmbienceProcessor : public IDspProcessor
{
public:
    AmbienceProcessor() noexcept; // width target 1.0: wet path passes original stereo until configured

    void prepare (const ProcessContext& ctx) override;
    void process (AudioBufferView& buffer) override;
    void reset() override;

    // Control methods: call once per block, never inside the sample loop.
    void setMix (float mix01) noexcept;            // 0..1, smoothed over 30 ms
    void setPredelayMs (float ms) noexcept;        // 0..200
    void setDecaySeconds (float seconds) noexcept; // 0.1..10 (t60)
    void setDamping (float damping01) noexcept;    // 0..1 (18 kHz .. 1.8 kHz), coeff smoothed over 50 ms
    void setWidth (float width01) noexcept;        // 0..1 (mono .. original), smoothed over 20 ms

    float getDecaySeconds() const noexcept { return decaySeconds; } // host tail length

private:
    static constexpr int numLines = 8;
    static constexpr int baseLength[numLines] = { 1543, 2011, 2663, 3413, 4127, 5003, 5741, 6719 };

    void updatePredelaySamples() noexcept;
    void updateDampCoeffTarget() noexcept;

    std::vector<float> line[numLines];
    int lineLength[numLines] = {};
    int linePos[numLines] = {};
    float dampState[numLines] = {};
    float gain[numLines] = {}; // per-block, derived from decay

    std::vector<float> predelay;
    int predelayPos = 0;
    int predelaySamples = 0;

    float ap1X = 0.0f; float ap1Y = 0.0f; // diffuser allpass states (1 sample each)
    float ap2X = 0.0f; float ap2Y = 0.0f;

    Smoother mixSmoother;
    Smoother widthSmoother; // 20 ms
    Smoother dampSmoother;  // applied damping coefficient, 50 ms — reverb changes are slow
    float decaySeconds = 2.0f;
    float damping = 0.0f;
    float predelayMs = 0.0f;
    double sampleRate = 48000.0;
};
}
