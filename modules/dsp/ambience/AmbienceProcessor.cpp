// SPDX-License-Identifier: AGPL-3.0-only

#include "AmbienceProcessor.h"

#include <algorithm>
#include <cmath>

namespace auralis
{
namespace
{
    constexpr float twoPi = 6.283185307179586f;
    constexpr float inputGain = 1.0f / 8.0f; // per-line injection; keeps summed buses in headroom
    constexpr float busGain = 0.5f;          // 4 lines summed per wet bus
    constexpr float mixSilent = 1e-4f;       // below this the output is the exact dry signal
    constexpr float widthSmoothMs = 20.0f;
    constexpr float dampSmoothMs = 50.0f;    // reverb character changes are slow by design
}

AmbienceProcessor::AmbienceProcessor() noexcept
{
    // Current default width member was 1.0: keep the wet path at original stereo until configured.
    widthSmoother.setTarget (1.0f);
}

void AmbienceProcessor::prepare (const ProcessContext& ctx)
{
    sampleRate = ctx.sampleRate > 0.0 ? ctx.sampleRate : 48000.0;
    const float scale = (float) (sampleRate / 48000.0);
    for (int i = 0; i < numLines; ++i)
    {
        int len = (int) std::lround ((float) baseLength[i] * scale);
        if (len < 2)
            len = 2;
        if ((len & 1) == 0)
            ++len; // odd lengths keep the mode pattern non-degenerate
        lineLength[i] = len;
        line[i].assign ((size_t) len, 0.0f);
        linePos[i] = 0;
        dampState[i] = 0.0f;
    }
    const int maxPredelay = (int) std::lround (0.2 * sampleRate); // 200 ms
    predelay.assign ((size_t) maxPredelay + 1, 0.0f);
    predelayPos = 0;
    updatePredelaySamples();
    ap1X = ap1Y = ap2X = ap2Y = 0.0f;
    updateDampCoeffTarget(); // recompute at this sample rate before the snap
    dampSmoother.prepare (sampleRate, dampSmoothMs);
    widthSmoother.prepare (sampleRate, widthSmoothMs); // also snaps: no zipper on reload
    mixSmoother.prepare (sampleRate, 30.0f);
}

void AmbienceProcessor::process (AudioBufferView& buffer)
{
    const int nf = buffer.numFrames();
    const int nch = buffer.numChannels();
    if (nf <= 0 || nch <= 0 || predelay.empty() || line[0].empty())
        return; // unprepared: buffer already holds the input; passthrough

    float* ch0 = buffer.channel (0);
    float* ch1 = nch > 1 ? buffer.channel (1) : nullptr;

    // shortcut: decay gains recompute per block (8 pow), level steps are masked by the tail; upgrade: smooth gains per sample or crossfade
    const float sr = (float) sampleRate;
    for (int i = 0; i < numLines; ++i)
        gain[i] = std::pow (10.0f, -3.0f * ((float) lineLength[i] / sr) / decaySeconds);

    const int predelayLen = (int) predelay.size();

    for (int n = 0; n < nf; ++n)
    {
        const float dryL = ch0[n];
        const float dryR = ch1 != nullptr ? ch1[n] : 0.0f;

        // Mono sum -> fixed 1-sample allpass diffusers -> predelay -> FDN.
        const float in = 0.5f * (dryL + dryR);
        const float ap1Out = -0.5f * in + ap1X + 0.5f * ap1Y;
        ap1X = in;
        ap1Y = ap1Out;
        float d = -0.375f * ap1Out + ap2X + 0.375f * ap2Y;
        ap2X = ap1Out;
        ap2Y = d;

        // Predelay ring: write then read so a 0 ms offset is exactly 0 samples.
        // shortcut: predelay position is integer-sample and block-quantized, audible only when automating predelay; upgrade: fractional delay interpolation
        predelay[predelayPos] = d;
        d = predelay[predelayPos - predelaySamples
                     + (predelayPos < predelaySamples ? predelayLen : 0)];
        predelayPos = predelayPos + 1 == predelayLen ? 0 : predelayPos + 1;

        const float dampCoeff = dampSmoother.next(); // one lerp per sample, shared by all 8 lines
        float s[numLines];
        float sum = 0.0f;
        for (int i = 0; i < numLines; ++i)
        {
            s[i] = line[i][linePos[i]];
            sum += s[i];
        }

        float wetL = 0.0f;
        float wetR = 0.0f;
        for (int i = 0; i < numLines; ++i)
        {
            const float f = s[i] - 0.25f * sum;              // Householder row: H = I - J/4
            dampState[i] += dampCoeff * (f - dampState[i]);  // per-line one-pole damping
            line[i][linePos[i]] = inputGain * d + gain[i] * dampState[i];
            linePos[i] = linePos[i] + 1 == lineLength[i] ? 0 : linePos[i] + 1;
            if (i < 4)
                wetL += s[i]; // wet taps the delay-line reads
            else
                wetR += s[i];
        }
        wetL *= busGain;
        wetR *= busGain;

        const float mid = 0.5f * (wetL + wetR);
        const float side = 0.5f * (wetL - wetR);
        const float w = widthSmoother.next();
        wetL = mid + side * w;
        wetR = mid - side * w;

        const float m = mixSmoother.next();
        if (m < mixSilent)
        {
            ch0[n] = dryL; // exact passthrough; the network still runs so the tail stays warm
            if (ch1 != nullptr)
                ch1[n] = dryR;
        }
        else if (ch1 != nullptr)
        {
            ch0[n] = dryL * (1.0f - m) + wetL * m;
            ch1[n] = dryR * (1.0f - m) + wetR * m;
        }
        else
        {
            ch0[n] = dryL * (1.0f - m) + mid * m; // mono buffer folds to mid, width cancels
        }
    }
}

void AmbienceProcessor::reset()
{
    for (int i = 0; i < numLines; ++i)
    {
        std::fill (line[i].begin(), line[i].end(), 0.0f);
        linePos[i] = 0;
        dampState[i] = 0.0f;
    }
    std::fill (predelay.begin(), predelay.end(), 0.0f);
    predelayPos = 0;
    ap1X = ap1Y = ap2X = ap2Y = 0.0f;
    widthSmoother.snap();
    dampSmoother.snap();
}

void AmbienceProcessor::setMix (float mix01) noexcept
{
    mixSmoother.setTarget (mix01 < 0.0f ? 0.0f : (mix01 > 1.0f ? 1.0f : mix01));
}

void AmbienceProcessor::setPredelayMs (float ms) noexcept
{
    predelayMs = ms < 0.0f ? 0.0f : (ms > 200.0f ? 200.0f : ms);
    updatePredelaySamples();
}

void AmbienceProcessor::setDecaySeconds (float seconds) noexcept
{
    decaySeconds = seconds < 0.1f ? 0.1f : (seconds > 10.0f ? 10.0f : seconds);
}

void AmbienceProcessor::setDamping (float damping01) noexcept
{
    damping = damping01 < 0.0f ? 0.0f : (damping01 > 1.0f ? 1.0f : damping01);
    updateDampCoeffTarget();
}

void AmbienceProcessor::setWidth (float width01) noexcept
{
    widthSmoother.setTarget (width01 < 0.0f ? 0.0f : (width01 > 1.0f ? 1.0f : width01));
}

void AmbienceProcessor::updateDampCoeffTarget() noexcept
{
    // fc = 18 kHz..1.8 kHz over damping 0..1, as the one-pole feedback coefficient.
    const float fcHz = 18000.0f * std::pow (10.0f, -damping);
    dampSmoother.setTarget (1.0f - std::exp (-twoPi * fcHz / (float) sampleRate));
}

void AmbienceProcessor::updatePredelaySamples() noexcept
{
    if (predelay.empty())
    {
        predelaySamples = 0;
        return;
    }
    const int maxPredelay = (int) predelay.size() - 1;
    const int samples = (int) std::lround (predelayMs * 0.001f * (float) sampleRate);
    predelaySamples = samples < 0 ? 0 : (samples > maxPredelay ? maxPredelay : samples);
}
}
