// SPDX-License-Identifier: AGPL-3.0-only

#include "GainProcessor.h"

#include <cmath>

namespace auralis
{
void GainProcessor::prepare (const ProcessContext& ctx)
{
    coeff = 1.0f - std::exp (-1.0f / (0.02f * (float) ctx.sampleRate));
    current = target; // no zipper on reload
}

void GainProcessor::process (AudioBufferView& buffer)
{
    if (bypass)
        return; // buffer already holds the input; passthrough

    const int nch = buffer.numChannels();
    const int nf = buffer.numFrames();

    for (int c = 0; c < nch; ++c)
    {
        float* data = buffer.channel (c);
        for (int i = 0; i < nf; ++i)
        {
            current += coeff * (target - current);
            data[i] *= current;
        }
    }
}

void GainProcessor::reset() { current = target; }

void GainProcessor::setGainDb (float db) noexcept
{
    target = std::pow (10.0f, db / 20.0f);
}

void GainProcessor::setBypass (bool b) noexcept { bypass = b; }
}
