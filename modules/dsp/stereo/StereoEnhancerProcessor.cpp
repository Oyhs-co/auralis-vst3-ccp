// SPDX-License-Identifier: AGPL-3.0-only

#include "StereoEnhancerProcessor.h"

#include <algorithm>
#include <cmath>

namespace auralis
{
namespace
{
constexpr float smoothTimeMs = 20.0f;
}

void StereoEnhancerProcessor::prepare (const ProcessContext& ctx)
{
    // Smoother::prepare also snaps to the current target: no zipper on reload.
    widthSmoother.setTarget (1.0f);
    midGainSmoother.setTarget (1.0f);
    sideGainSmoother.setTarget (1.0f);
    widthSmoother.prepare (ctx.sampleRate, smoothTimeMs);
    midGainSmoother.prepare (ctx.sampleRate, smoothTimeMs);
    sideGainSmoother.prepare (ctx.sampleRate, smoothTimeMs);
    correlation.store (1.0f, std::memory_order_relaxed);
}

void StereoEnhancerProcessor::process (AudioBufferView& buffer)
{
    const int nch = buffer.numChannels();
    const int nf = buffer.numFrames();
    if (nch < 2 || nf <= 0)
    {
        // Mono: exact passthrough, no M/S math; a mono signal is perfectly correlated.
        correlation.store (1.0f, std::memory_order_relaxed);
        return;
    }

    float* left = buffer.channel (0);
    float* right = buffer.channel (1);

    // balance < 0 attenuates the left channel, balance > 0 the right one.
    const float gainL = balance <= 0.0f ? 1.0f + balance : 1.0f;
    const float gainR = balance >= 0.0f ? 1.0f - balance : 1.0f;

    float sumLR = 0.0f;
    float sumLL = 0.0f;
    float sumRR = 0.0f;

    // Identity at defaults is ulp-exact: mid + side recomposes L up to float32
    // rounding only (~1e-7 for |x| <= 1), never more.
    for (int i = 0; i < nf; ++i)
    {
        const float mid = 0.5f * (left[i] + right[i]);
        const float side = 0.5f * (left[i] - right[i]);
        const float center = mid * midGainSmoother.next();
        const float edge = side * sideGainSmoother.next() * widthSmoother.next();
        left[i] = (center + edge) * gainL;
        right[i] = (center - edge) * gainR;
        sumLR += left[i] * right[i];
        sumLL += left[i] * left[i];
        sumRR += right[i] * right[i];
    }

    const float corr = std::clamp (sumLR / std::sqrt (sumLL * sumRR + 1e-12f), -1.0f, 1.0f);
    correlation.store (corr, std::memory_order_relaxed);
}

void StereoEnhancerProcessor::reset()
{
    widthSmoother.snap();
    midGainSmoother.snap();
    sideGainSmoother.snap();
    correlation.store (1.0f, std::memory_order_relaxed);
}

void StereoEnhancerProcessor::setWidth (float w) noexcept { widthSmoother.setTarget (w); }

void StereoEnhancerProcessor::setMidGainDb (float db) noexcept
{
    midGainSmoother.setTarget (std::pow (10.0f, db / 20.0f));
}

void StereoEnhancerProcessor::setSideGainDb (float db) noexcept
{
    sideGainSmoother.setTarget (std::pow (10.0f, db / 20.0f));
}

void StereoEnhancerProcessor::setBalance (float b) noexcept { balance = b; }
}
