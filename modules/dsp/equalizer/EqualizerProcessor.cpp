// SPDX-License-Identifier: AGPL-3.0-only

#include "EqualizerProcessor.h"

#include <algorithm>
#include <cmath>

namespace auralis
{
namespace
{
    constexpr double kTwoPi = 6.283185307179586476925286766559;
    constexpr double kSqrtTwo = 1.4142135623730950488016887242097;
}

void EqualizerProcessor::prepare (const ProcessContext& ctx)
{
    sampleRate = ctx.sampleRate;
    for (auto& b : bands)
    {
        std::fill (b.x1, b.x1 + maxChannels, 0.0f);
        std::fill (b.x2, b.x2 + maxChannels, 0.0f);
        std::fill (b.y1, b.y1 + maxChannels, 0.0f);
        std::fill (b.y2, b.y2 + maxChannels, 0.0f);
        b.dirty = true;
    }
}

void EqualizerProcessor::reset()
{
    for (auto& b : bands)
    {
        std::fill (b.x1, b.x1 + maxChannels, 0.0f);
        std::fill (b.x2, b.x2 + maxChannels, 0.0f);
        std::fill (b.y1, b.y1 + maxChannels, 0.0f);
        std::fill (b.y2, b.y2 + maxChannels, 0.0f);
    }
}

void EqualizerProcessor::setBand (int index, float freqHz, float gainDb, float q) noexcept
{
    if (index < 0 || index >= maxBands)
        return;
    auto& b = bands[index];
    b.freq = std::clamp (freqHz, 20.0f, 20000.0f);
    b.gainDb = std::clamp (gainDb, -24.0f, 24.0f);
    b.q = std::clamp (q, 0.1f, 20.0f);
    b.dirty = true;
}

void EqualizerProcessor::setBandEnabled (int index, bool enabled) noexcept
{
    if (index < 0 || index >= maxBands)
        return;
    bands[index].enabled = enabled;
}

void EqualizerProcessor::updateBand (Band& b) noexcept
{
    // RBJ Audio EQ Cookbook, direct form I coefficients (normalized a0 = 1).
    // Shelves use Q = 1/sqrt(2) for Butterworth-like response; peaking uses given Q.
    const double A = std::pow (10.0, b.gainDb / 40.0);
    const double w0 = kTwoPi * b.freq / sampleRate;
    const double cosw0 = std::cos (w0);
    const double sinw0 = std::sin (w0);
    const bool isShelf = (&b == &bands[0] || &b == &bands[3]);
    const double Q = isShelf ? kSqrtTwo : b.q;
    const double alpha = sinw0 / (2.0 * Q);

    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0, a0 = 1.0;

    if (&b == &bands[0]) // low shelf
    {
        const double sqrtA = std::sqrt (A);
        b0 = A * ( (A + 1) - (A - 1) * cosw0 + 2.0 * sqrtA * alpha );
        b1 = 2.0 * A * ( (A - 1) - (A + 1) * cosw0 );
        b2 = A * ( (A + 1) - (A - 1) * cosw0 - 2.0 * sqrtA * alpha );
        a0 = (A + 1) + (A - 1) * cosw0 + 2.0 * sqrtA * alpha;
        a1 = -2.0 * ( (A - 1) + (A + 1) * cosw0 );
        a2 = (A + 1) + (A - 1) * cosw0 - 2.0 * sqrtA * alpha;
    }
    else if (&b == &bands[3]) // high shelf
    {
        const double sqrtA = std::sqrt (A);
        b0 = A * ( (A + 1) + (A - 1) * cosw0 + 2.0 * sqrtA * alpha );
        b1 = -2.0 * A * ( (A - 1) + (A + 1) * cosw0 );
        b2 = A * ( (A + 1) + (A - 1) * cosw0 - 2.0 * sqrtA * alpha );
        a0 = (A + 1) - (A - 1) * cosw0 + 2.0 * sqrtA * alpha;
        a1 = 2.0 * ( (A - 1) - (A + 1) * cosw0 );
        a2 = (A + 1) - (A - 1) * cosw0 - 2.0 * sqrtA * alpha;
    }
    else // peaking (bands 1 and 2)
    {
        b0 = 1.0 + alpha * A;
        b1 = -2.0 * cosw0;
        b2 = 1.0 - alpha * A;
        a0 = 1.0 + alpha / A;
        a1 = -2.0 * cosw0;
        a2 = 1.0 - alpha / A;
    }

    // Normalize
    b.b0 = b0 / a0;
    b.b1 = b1 / a0;
    b.b2 = b2 / a0;
    b.a1 = a1 / a0;
    b.a2 = a2 / a0;
    b.dirty = false;
}

void EqualizerProcessor::process (AudioBufferView& buffer)
{
    const int nch = buffer.numChannels() < maxChannels ? buffer.numChannels() : maxChannels;
    const int nf = buffer.numFrames();

    for (int i = 0; i < nf; ++i)
    {
        for (int c = 0; c < nch; ++c)
        {
            float x = buffer.channel (c)[i];

            for (auto& b : bands)
            {
                if (!b.enabled)
                    continue;

                if (b.dirty)
                    updateBand (b);

                float y = (float) (b.b0 * (double) x + b.b1 * (double) b.x1[c] + b.b2 * (double) b.x2[c]
                                   - b.a1 * (double) b.y1[c] - b.a2 * (double) b.y2[c]);
                b.x2[c] = b.x1[c];
                b.x1[c] = x;
                b.y2[c] = b.y1[c];
                b.y1[c] = y;
                x = y;
            }

            buffer.channel (c)[i] = x;
        }
    }
}
} // namespace auralis