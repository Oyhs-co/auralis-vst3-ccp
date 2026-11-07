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
    delay.assign (std::size_t (ctx.numChannels),
                  std::vector<float> (std::size_t (latency), 0.0f));
    window.assign (std::size_t (ctx.numChannels), SlidingMax {});
    for (auto& w : window)
    {
        w.val.assign (std::size_t (latency) + 1, 0.0f);
        w.idx.assign (std::size_t (latency) + 1, 0);
    }
    writePos = 0;
    seq = 0;
    gain = 1.0f;
    thresholdSmoother.prepare (ctx.sampleRate, 5.0f);
    thresholdSmoother.snap();
}

void LimiterProcessor::process (AudioBufferView& buffer)
{
    const int L = latency;
    if (L <= 0)
        return; // not prepared yet

    // shortcut: channels beyond prepare()'s numChannels pass dry, upgrade: re-prepare on layout change
    const int prepared = (int) window.size();
    const int nch = buffer.numChannels() < prepared ? buffer.numChannels() : prepared;
    const int nf = buffer.numFrames();
    // setReleaseMs is plain, so the coefficient is refreshed once per block.
    const float releaseCoeff = 1.0f - std::exp (-1000.0f / (releaseMs * sampleRate));
    const int cap = L + 1; // window = last L inputs plus the current one
    float minGain = 1.0f;

    for (int i = 0; i < nf; ++i)
    {
        // Window = input samples [seq-L, seq] per channel: the ring holds the
        // last L inputs (the head now being output included) and the incoming
        // sample joins it. A peak therefore holds the gain down from its
        // arrival until it has left the delay line, so instant attack can
        // never under-limit its own output. Exact maximum via a monotonic
        // non-increasing queue, amortized O(1) per sample.
        float peak = 0.0f;
        for (int c = 0; c < nch; ++c)
        {
            auto& w = window[std::size_t (c)];
            // Expire before pushing: keeps at most L live entries so the
            // circular write never collides with the front.
            while (w.count > 0 && w.idx[std::size_t (w.head)] < seq - L)
            {
                w.head = (w.head + 1) % cap;
                --w.count;
            }
            const float v = std::fabs (buffer.channel (c)[i]);
            while (w.count > 0 && w.val[std::size_t ((w.head + w.count - 1) % cap)] <= v)
                --w.count;
            const int pos = (w.head + w.count) % cap;
            w.val[std::size_t (pos)] = v;
            w.idx[std::size_t (pos)] = seq;
            ++w.count;
            peak = std::max (peak, w.val[std::size_t (w.head)]);
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
        ++seq;
    }

    gainReductionDb.store (-20.0f * std::log10 (std::max (minGain, 1e-6f)),
                           std::memory_order_relaxed);
}

void LimiterProcessor::reset()
{
    for (auto& ring : delay)
        std::fill (ring.begin(), ring.end(), 0.0f);
    for (auto& w : window)
    {
        w.head = 0;
        w.count = 0;
    }
    seq = 0;
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
