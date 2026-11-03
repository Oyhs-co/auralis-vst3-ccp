// SPDX-License-Identifier: AGPL-3.0-only

#include "LimiterProcessor.h"

#include <algorithm>
#include <cmath>

namespace auralis
{
LimiterProcessor::LimiterProcessor() noexcept
{
    thresholdSmoother.setTarget (1.0f); // 0 dB threshold until configured
}

void LimiterProcessor::prepare (const ProcessContext& ctx)
{
    sampleRate = (float) ctx.sampleRate;
    latency = (int) std::lround (0.005 * ctx.sampleRate); // 5 ms lookahead
    for (auto& ring : delay)
        ring.assign (std::size_t (latency), 0.0f);
    writePos = 0;
    gain = 1.0f;
    thresholdSmoother.prepare (ctx.sampleRate, 5.0f);
    thresholdSmoother.snap();
}

void LimiterProcessor::process (AudioBufferView& buffer)
{
    const int L = latency;
    if (L <= 0)
        return; // not prepared yet

    // shortcut: at most 2 channels (mono/stereo per plan); extras pass through untouched
    const int nch = std::min (buffer.numChannels(), kMaxChannels);
    const int nf = buffer.numFrames();
    // setReleaseMs is plain, so the coefficient is refreshed once per block.
    const float releaseCoeff = 1.0f - std::exp (-1000.0f / (releaseMs * sampleRate));
    float minGain = 1.0f;

    for (int i = 0; i < nf; ++i)
    {
        // shortcut: O(N*L) window peak scan, upgrade to a monotonic deque if profiling shows cost
        // Window = input samples [i-L, i]: the ring holds the last L inputs (the
        // head now being output included) and the incoming sample joins it. A
        // peak therefore holds the gain down from its arrival until it has left
        // the delay line, so instant attack can never under-limit its own output.
        float peak = 0.0f;
        for (int c = 0; c < nch; ++c)
        {
            const float* ring = delay[c].data();
            for (int k = 0; k < L; ++k)
                peak = std::max (peak, std::fabs (ring[k]));
            peak = std::max (peak, std::fabs (buffer.channel (c)[i]));
        }

        const float thresholdLin = thresholdSmoother.next();
        const float targetGain = peak > thresholdLin ? thresholdLin / peak : 1.0f;
        gain = targetGain < gain
                   ? targetGain                                 // instant attack
                   : gain + (targetGain - gain) * releaseCoeff; // one-pole release
        minGain = std::min (minGain, gain);

        for (int c = 0; c < nch; ++c)
        {
            float* ring = delay[c].data();
            float* data = buffer.channel (c);
            const float inputSample = data[i];
            float out = ring[writePos] * gain; // head == input from L samples ago
            if (out > ceilingLin)
                out = ceilingLin; // safety clamp, idle while the threshold governs
            else if (out < -ceilingLin)
                out = -ceilingLin;
            data[i] = out;
            ring[writePos] = inputSample;
        }
        if (++writePos == L)
            writePos = 0;
    }

    gainReductionDb.store (-20.0f * std::log10 (std::max (minGain, 1e-6f)),
                           std::memory_order_relaxed);
}

void LimiterProcessor::reset()
{
    for (auto& ring : delay)
        std::fill (ring.begin(), ring.end(), 0.0f);
    writePos = 0;
    gain = 1.0f;
}

void LimiterProcessor::setThresholdDb (float db) noexcept
{
    thresholdSmoother.setTarget (std::pow (10.0f, std::clamp (db, -24.0f, 0.0f) / 20.0f));
}

void LimiterProcessor::setCeilingDb (float db) noexcept
{
    ceilingLin = std::pow (10.0f, std::clamp (db, -12.0f, 0.0f) / 20.0f);
}

void LimiterProcessor::setReleaseMs (float ms) noexcept
{
    releaseMs = std::clamp (ms, 5.0f, 1000.0f);
}

float LimiterProcessor::getGainReductionDb() const noexcept
{
    return gainReductionDb.load (std::memory_order_relaxed);
}
}
