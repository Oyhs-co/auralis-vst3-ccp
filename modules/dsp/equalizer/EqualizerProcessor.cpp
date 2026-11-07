// SPDX-License-Identifier: AGPL-3.0-only

#include "EqualizerProcessor.h"

#include <algorithm>
#include <cmath>

namespace auralis
{
namespace
{
    constexpr double kTwoPi = 6.283185307179586476925286766559;
    constexpr double kInvSqrtTwo = 0.7071067811865476; // Butterworth Q
    // Coefficients closer than this to target are snapped; inaudible by orders of magnitude.
    constexpr double kCoeffEpsilon = 1e-9;

    struct Rbj
    {
        double b0, b1, b2, a0, a1, a2; // unnormalized (a0 != 1)
    };

    // RBJ Audio EQ Cookbook coefficients, shared by the audio path (updateBand)
    // and the GUI response curve (getMagnitudeResponse) so the drawn curve always
    // matches what the filter computes. Shelves use Butterworth Q = 1/sqrt(2).
    Rbj rbjRaw (double freqHz, double gainDb, double q, int bandIndex, double sampleRate) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = kTwoPi * freqHz / sampleRate;
        const double cosw0 = std::cos (w0);
        const double sinw0 = std::sin (w0);
        const bool isShelf = bandIndex == 0 || bandIndex == 3;
        const double alpha = isShelf ? sinw0 * kInvSqrtTwo : sinw0 / (2.0 * q);

        Rbj c { 1.0, 0.0, 0.0, 1.0, 0.0, 0.0 };

        if (bandIndex == 0) // low shelf
        {
            const double sqrtA = std::sqrt (A);
            c.b0 = A * ( (A + 1) - (A - 1) * cosw0 + 2.0 * sqrtA * alpha );
            c.b1 = 2.0 * A * ( (A - 1) - (A + 1) * cosw0 );
            c.b2 = A * ( (A + 1) - (A - 1) * cosw0 - 2.0 * sqrtA * alpha );
            c.a0 = (A + 1) + (A - 1) * cosw0 + 2.0 * sqrtA * alpha;
            c.a1 = -2.0 * ( (A - 1) + (A + 1) * cosw0 );
            c.a2 = (A + 1) + (A - 1) * cosw0 - 2.0 * sqrtA * alpha;
        }
        else if (bandIndex == 3) // high shelf
        {
            const double sqrtA = std::sqrt (A);
            c.b0 = A * ( (A + 1) + (A - 1) * cosw0 + 2.0 * sqrtA * alpha );
            c.b1 = -2.0 * A * ( (A - 1) + (A + 1) * cosw0 );
            c.b2 = A * ( (A + 1) + (A - 1) * cosw0 - 2.0 * sqrtA * alpha );
            c.a0 = (A + 1) - (A - 1) * cosw0 + 2.0 * sqrtA * alpha;
            c.a1 = 2.0 * ( (A - 1) - (A + 1) * cosw0 );
            c.a2 = (A + 1) - (A - 1) * cosw0 - 2.0 * sqrtA * alpha;
        }
        else // peaking (bands 1 and 2)
        {
            c.b0 = 1.0 + alpha * A;
            c.b1 = -2.0 * cosw0;
            c.b2 = 1.0 - alpha * A;
            c.a0 = 1.0 + alpha / A;
            c.a1 = -2.0 * cosw0;
            c.a2 = 1.0 - alpha / A;
        }

        return c;
    }
}

void EqualizerProcessor::prepare (const ProcessContext& ctx)
{
    sampleRate = ctx.sampleRate;
    smoothAlpha = 1.0 - std::exp (-1.0 / (smoothTimeSeconds * sampleRate));
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
    const float f = std::clamp (freqHz, 20.0f, 20000.0f);
    const float g = std::clamp (gainDb, -24.0f, 24.0f);
    const float qq = std::clamp (q, 0.1f, 20.0f);
    // Hosts resend unchanged values every block; exact float compare is intended.
    if (f == b.freq && g == b.gainDb && qq == b.q)
        return;
    b.freq = f;
    b.gainDb = g;
    b.q = qq;
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
    const Rbj c = rbjRaw (b.freq, b.gainDb, b.q, (int) (&b - bands), sampleRate);

    // Normalize
    b.tb0 = c.b0 / c.a0;
    b.tb1 = c.b1 / c.a0;
    b.tb2 = c.b2 / c.a0;
    b.ta1 = c.a1 / c.a0;
    b.ta2 = c.a2 / c.a0;
}

void EqualizerProcessor::getMagnitudeResponse (const double* freqHz, double* dbOut, int numPoints) const
{
    if (numPoints <= 0 || freqHz == nullptr || dbOut == nullptr)
        return;

    for (int i = 0; i < numPoints; ++i)
        dbOut[i] = 0.0;

    for (int i = 0; i < numPoints; ++i)
    {
        const double w = kTwoPi * std::clamp (freqHz[i], 1.0, 0.5 * sampleRate) / sampleRate;
        const double cosW = std::cos (w);
        const double sinW = std::sin (w);
        const double cos2W = std::cos (2.0 * w);
        const double sin2W = std::sin (2.0 * w);

        for (int bandIndex = 0; bandIndex < maxBands; ++bandIndex)
        {
            const Band& b = bands[bandIndex];
            if (! b.enabled)
                continue;

            // Fresh target coefficients from the band's cached values (same
            // math as updateBand), evaluated at w. A 0 dB band normalizes to
            // b == a bitwise, so its contribution is exactly 0.
            const Rbj c = rbjRaw (b.freq, b.gainDb, b.q, bandIndex, sampleRate);
            const double b0 = c.b0 / c.a0;
            const double b1 = c.b1 / c.a0;
            const double b2 = c.b2 / c.a0;
            const double a1 = c.a1 / c.a0;
            const double a2 = c.a2 / c.a0;

            // |H(e^jw)|^2 with H = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2)
            const double nr = b0 + b1 * cosW + b2 * cos2W;
            const double ni = -b1 * sinW - b2 * sin2W;
            const double dr = 1.0 + a1 * cosW + a2 * cos2W;
            const double di = -a1 * sinW - a2 * sin2W;
            const double den = dr * dr + di * di;
            if (den > 0.0)
                dbOut[i] += 10.0 * std::log10 ((nr * nr + ni * ni) / den);
        }
    }
}

void EqualizerProcessor::process (AudioBufferView& buffer)
{
    const int nch = buffer.numChannels() < maxChannels ? buffer.numChannels() : maxChannels;
    const int nf = buffer.numFrames();

    // Block prologue: compute target coefficients once per enabled dirty band.
    for (auto& b : bands)
        if (b.enabled && b.dirty)
            updateBand (b);

    for (int i = 0; i < nf; ++i)
    {
        // Per-sample coefficient smoothing (shared by all channels, so both channels
        // always run on identical coefficients).
        for (auto& b : bands)
        {
            if (! (b.enabled && b.dirty))
                continue;

            b.b0 += smoothAlpha * (b.tb0 - b.b0);
            b.b1 += smoothAlpha * (b.tb1 - b.b1);
            b.b2 += smoothAlpha * (b.tb2 - b.b2);
            b.a1 += smoothAlpha * (b.ta1 - b.a1);
            b.a2 += smoothAlpha * (b.ta2 - b.a2);

            if (std::fabs (b.tb0 - b.b0) < kCoeffEpsilon
             && std::fabs (b.tb1 - b.b1) < kCoeffEpsilon
             && std::fabs (b.tb2 - b.b2) < kCoeffEpsilon
             && std::fabs (b.ta1 - b.a1) < kCoeffEpsilon
             && std::fabs (b.ta2 - b.a2) < kCoeffEpsilon)
            {
                b.b0 = b.tb0;
                b.b1 = b.tb1;
                b.b2 = b.tb2;
                b.a1 = b.ta1;
                b.a2 = b.ta2;
                b.dirty = false;
            }
        }

        for (int c = 0; c < nch; ++c)
        {
            float x = buffer.channel (c)[i];

            for (auto& b : bands)
            {
                if (!b.enabled)
                    continue;

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