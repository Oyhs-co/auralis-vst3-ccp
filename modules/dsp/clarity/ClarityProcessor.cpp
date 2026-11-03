// SPDX-License-Identifier: AGPL-3.0-only

#include "ClarityProcessor.h"

#include <cmath>

namespace auralis
{
namespace
{
constexpr double kQ = 0.7071067811865476;               // Butterworth Q
constexpr double kInvTanh2 = 1.0 / 0.9640275800758169;  // 1/tanh(2): softClip unity at full scale
constexpr double kTwoPi = 6.283185307179586476925286766559;
}

void ClarityProcessor::prepare (const ProcessContext& ctx)
{
    sampleRate = ctx.sampleRate;
    amount.prepare (ctx.sampleRate, 30.0f);
    presence.prepare (ctx.sampleRate, 20.0f);
    updateCoefficients();
    reset();
}

void ClarityProcessor::process (AudioBufferView& buffer)
{
    // shortcut: only the first two channels are excited; the plugin is mono/stereo today
    const int nch = buffer.numChannels() < maxChannels ? buffer.numChannels() : maxChannels;
    const int nf = buffer.numFrames();

    for (int c = 0; c < nch; ++c)
    {
        float* data = buffer.channel (c);
        double hx1 = x1[c], hx2 = x2[c], hy1 = y1[c], hy2 = y2[c];

        for (int i = 0; i < nf; ++i)
        {
            const float a = amount.next();
            const float g = presence.next();
            if (a == 0.0f)
                continue; // amount fully down: exact dry passthrough, exciter path skipped

            const float x = data[i];
            const double y = b0 * (double) x + b1 * hx1 + b2 * hx2 - a1 * hy1 - a2 * hy2;
            hx2 = hx1; hx1 = (double) x; hy2 = hy1; hy1 = y;

            const float clipped = (float) (std::tanh (2.0 * y) * kInvTanh2);
            const float comp = 1.0f / (1.0f + 0.15f * a);
            data[i] = comp * (x + a * g * clipped);
        }

        x1[c] = hx1; x2[c] = hx2; y1[c] = hy1; y2[c] = hy2;
    }
}

void ClarityProcessor::reset()
{
    for (int c = 0; c < maxChannels; ++c)
        x1[c] = x2[c] = y1[c] = y2[c] = 0.0;
    amount.snap();    // no zipper when playback restarts, as in GainProcessor
    presence.snap();
}

void ClarityProcessor::setAmount (float amount01) noexcept
{
    amount.setTarget (amount01 < 0.0f ? 0.0f : (amount01 > 1.0f ? 1.0f : amount01));
}

void ClarityProcessor::setCenterFreq (float freqHz) noexcept
{
    centerFreq = freqHz < 1000.0f ? 1000.0 : (freqHz > 8000.0f ? 8000.0 : (double) freqHz);
    updateCoefficients(); // control-rate: once per block at most
}

void ClarityProcessor::setPresenceDb (float db) noexcept
{
    const float clamped = db < -6.0f ? -6.0f : (db > 6.0f ? 6.0f : db);
    presence.setTarget (std::pow (10.0f, clamped / 20.0f));
}

void ClarityProcessor::updateCoefficients() noexcept
{
    // RBJ cookbook highpass; coefficients in double for stability at low f0/Fs ratios.
    const double w0 = kTwoPi * centerFreq / sampleRate;
    const double cosw0 = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * kQ);
    const double a0 = 1.0 + alpha;

    b0 = (1.0 + cosw0) / 2.0 / a0;
    b1 = -(1.0 + cosw0) / a0;
    b2 = (1.0 + cosw0) / 2.0 / a0;
    a1 = -2.0 * cosw0 / a0;
    a2 = (1.0 - alpha) / a0;
}
}
