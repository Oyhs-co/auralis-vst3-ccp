// SPDX-License-Identifier: AGPL-3.0-only

#include "BassEnhancerProcessor.h"

#include <algorithm>
#include <cmath>

namespace auralis
{
namespace
{
    constexpr double kTwoPi = 6.283185307179586476925286766559;
    constexpr double kSqrtTwo = 1.4142135623730950488016887242097;
}

void BassEnhancerProcessor::prepare (const ProcessContext& ctx)
{
    sampleRate = ctx.sampleRate;
    updateShelf();
    shelfB0Smoother.prepare (ctx.sampleRate, 20.0f);
    shelfB1Smoother.prepare (ctx.sampleRate, 20.0f);
    shelfB2Smoother.prepare (ctx.sampleRate, 20.0f);
    shelfA1Smoother.prepare (ctx.sampleRate, 20.0f);
    shelfA2Smoother.prepare (ctx.sampleRate, 20.0f);
    satSmoother.prepare (ctx.sampleRate, 20.0f);
    satSmoother.setTarget (0.0f);
    updateSubHighpass();
    reset();
}

void BassEnhancerProcessor::reset()
{
    for (int c = 0; c < maxChannels; ++c)
    {
        shelf.x1[c] = shelf.x2[c] = shelf.y1[c] = shelf.y2[c] = 0.0f;
        subHp.x1[c] = subHp.x2[c] = subHp.y1[c] = subHp.y2[c] = 0.0f;
    }
    shelfB0Smoother.snap();
    shelfB1Smoother.snap();
    shelfB2Smoother.snap();
    shelfA1Smoother.snap();
    shelfA2Smoother.snap();
    satSmoother.snap();
}

void BassEnhancerProcessor::setShelfFreq (float freqHz) noexcept
{
    shelfFreq = std::clamp (freqHz, 40.0f, 200.0f);
    updateShelf();
}

void BassEnhancerProcessor::setShelfGainDb (float gainDb) noexcept
{
    shelfGainDb = std::clamp (gainDb, -12.0f, 12.0f);
    updateShelf();
}

void BassEnhancerProcessor::setSaturation (float amount01) noexcept
{
    satSmoother.setTarget (amount01 < 0.0f ? 0.0f : (amount01 > 1.0f ? 1.0f : amount01));
}

void BassEnhancerProcessor::setSubHighpassFreq (float freqHz) noexcept
{
    subHpFreq = freqHz <= 0.0f ? 0.0f : std::clamp (freqHz, 10.0f, 60.0f);
    updateSubHighpass();
}

void BassEnhancerProcessor::updateShelf() noexcept
{
    const double A = std::pow (10.0, shelfGainDb / 40.0);
    const double w0 = kTwoPi * shelfFreq / sampleRate;
    const double cosw0 = std::cos (w0);
    const double sinw0 = std::sin (w0);
    const double alpha = sinw0 / (2.0 * kSqrtTwo); // Q = 1/sqrt(2)
    const double sqrtA = std::sqrt (A);

    // RBJ low shelf
    const double b0 = A * ( (A + 1) - (A - 1) * cosw0 + 2.0 * sqrtA * alpha );
    const double b1 = 2.0 * A * ( (A - 1) - (A + 1) * cosw0 );
    const double b2 = A * ( (A + 1) - (A - 1) * cosw0 - 2.0 * sqrtA * alpha );
    const double a0 = (A + 1) + (A - 1) * cosw0 + 2.0 * sqrtA * alpha;
    const double a1 = -2.0 * ( (A - 1) + (A + 1) * cosw0 );
    const double a2 = (A + 1) + (A - 1) * cosw0 - 2.0 * sqrtA * alpha;

    shelf.b0 = b0 / a0;
    shelf.b1 = b1 / a0;
    shelf.b2 = b2 / a0;
    shelf.a1 = a1 / a0;
    shelf.a2 = a2 / a0;
    shelfB0Smoother.setTarget ((float) shelf.b0);
    shelfB1Smoother.setTarget ((float) shelf.b1);
    shelfB2Smoother.setTarget ((float) shelf.b2);
    shelfA1Smoother.setTarget ((float) shelf.a1);
    shelfA2Smoother.setTarget ((float) shelf.a2);
}

void BassEnhancerProcessor::updateSubHighpass() noexcept
{
    if (subHpFreq <= 0.0f)
    {
        subHp.b0 = 1.0; subHp.b1 = 0.0; subHp.b2 = 0.0; subHp.a1 = 0.0; subHp.a2 = 0.0;
        return;
    }
    const double w0 = kTwoPi * subHpFreq / sampleRate;
    const double cosw0 = std::cos (w0);
    const double sinw0 = std::sin (w0);
    const double alpha = sinw0 / (2.0 * kSqrtTwo); // 12 dB/oct Butterworth
    const double b0 = (1.0 + cosw0) * 0.5;
    const double b1 = -(1.0 + cosw0);
    const double b2 = (1.0 + cosw0) * 0.5;
    const double a0 = 1.0 + alpha;
    const double a1 = -2.0 * cosw0;
    const double a2 = 1.0 - alpha;

    subHp.b0 = b0 / a0;
    subHp.b1 = b1 / a0;
    subHp.b2 = b2 / a0;
    subHp.a1 = a1 / a0;
    subHp.a2 = a2 / a0;
}

void BassEnhancerProcessor::process (AudioBufferView& buffer)
{
    const int nch = buffer.numChannels() < maxChannels ? buffer.numChannels() : maxChannels;
    const int nf = buffer.numFrames();

    for (int i = 0; i < nf; ++i)
    {
        const float s = satSmoother.next();
        const double b0 = shelfB0Smoother.next();
        const double b1 = shelfB1Smoother.next();
        const double b2 = shelfB2Smoother.next();
        const double a1 = shelfA1Smoother.next();
        const double a2 = shelfA2Smoother.next();

        for (int c = 0; c < nch; ++c)
        {
            float x = buffer.channel (c)[i];

            // Sub highpass (if enabled)
            if (subHpFreq > 0.0f)
            {
                float y = (float) (subHp.b0 * (double) x + subHp.b1 * (double) subHp.x1[c] + subHp.b2 * (double) subHp.x2[c]
                                   - subHp.a1 * (double) subHp.y1[c] - subHp.a2 * (double) subHp.y2[c]);
                subHp.x2[c] = subHp.x1[c];
                subHp.x1[c] = x;
                subHp.y2[c] = subHp.y1[c];
                subHp.y1[c] = y;
                x = y;
            }

            // Low shelf
            {
                float y = (float) (b0 * (double) x + b1 * (double) shelf.x1[c] + b2 * (double) shelf.x2[c]
                                   - a1 * (double) shelf.y1[c] - a2 * (double) shelf.y2[c]);
                shelf.x2[c] = shelf.x1[c];
                shelf.x1[c] = x;
                shelf.y2[c] = shelf.y1[c];
                shelf.y1[c] = y;
                x = y;
            }

            // Saturation (soft tanh)
            if (s > 0.001f)
            {
                const float k = 1.0f + 9.0f * s; // drive 1..10
                const float clipped = std::tanh (k * x) / std::tanh (k);
                x = (1.0f - s) * x + s * clipped;
            }

            // Level compensation: 1 / (1 + 0.25 * sat * max(0, gainDb/12))
            const float comp = 1.0f / (1.0f + 0.25f * s * std::max (0.0f, shelfGainDb / 12.0f));
            buffer.channel (c)[i] = x * comp;
        }
    }
}
} // namespace auralis