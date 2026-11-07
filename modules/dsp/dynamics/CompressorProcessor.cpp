// SPDX-License-Identifier: AGPL-3.0-only

#include "CompressorProcessor.h"

#include <algorithm>
#include <cmath>

namespace auralis
{
namespace
{
// 20 * log10 (2) in dB per unit of log2(); lets log2/exp2 replace log10/pow on
// the sample path. Literal because constexpr std::log10 needs C++23.
constexpr float kDbPerLog2 = 6.020599913279624f;
constexpr float kInvDbPerLog2 = 1.0f / kDbPerLog2;
}

void CompressorProcessor::updateCoefficients() noexcept
{
    coeffAttack = 1.0f - std::exp (-1000.0f / (attackMs * sampleRate));
    coeffRelease = 1.0f - std::exp (-1000.0f / (releaseMs * sampleRate));
    detCoeff = 1.0f - std::exp (-1000.0f / (10.0f * sampleRate));
}

void CompressorProcessor::prepare (const ProcessContext& ctx)
{
    sampleRate = (float) ctx.sampleRate;
    thresholdSmoother.prepare (sampleRate, 10.0f);
    makeupSmoother.prepare (sampleRate, 20.0f);
    ratioSmoother.prepare (sampleRate, 20.0f);
    kneeSmoother.prepare (sampleRate, 20.0f);
    thresholdSmoother.snap();
    makeupSmoother.snap();
    ratioSmoother.snap();
    kneeSmoother.snap();
    updateCoefficients();
    gainDb = 0.0f;
    detectorSq = 0.0f;
    grDb.store (0.0f, std::memory_order_relaxed);
}

void CompressorProcessor::reset()
{
    gainDb = 0.0f;
    detectorSq = 0.0f;
    grDb.store (0.0f, std::memory_order_relaxed);
    thresholdSmoother.snap();
    makeupSmoother.snap();
    ratioSmoother.snap();
    kneeSmoother.snap();
}

void CompressorProcessor::process (AudioBufferView& buffer)
{
    const int nch = buffer.numChannels();
    const int nf = buffer.numFrames();
    float sumReductionDb = 0.0f;

    for (int i = 0; i < nf; ++i)
    {
        // Stereo link: the whole buffer shares one gain, detected on the loudest channel.
        float peak = 0.0f;
        for (int c = 0; c < nch; ++c)
            peak = std::max (peak, std::fabs (buffer.channel (c)[i]));

        float env = peak;
        if (rmsMode)
        {
            detectorSq += detCoeff * (peak * peak + 1e-30f - detectorSq); // floor avoids denormals
            env = std::sqrt (detectorSq);
        }

        const float levelDb = std::log2 (env < 1e-6f ? 1e-6f : env) * kDbPerLog2;
        const float overshoot = levelDb - thresholdSmoother.next();

        // Smoothed ratio/knee; clamping ratio >= 1 guarantees no boost while it converges.
        const float ratio = std::max (ratioSmoother.next(), 1.0f);
        const float slope = 1.0f / ratio - 1.0f;
        const float knee = kneeSmoother.next();
        const float halfKnee = 0.5f * knee;

        // Soft knee: quadratic between -halfKnee..+halfKnee, linear above, 0 below.
        // slope <= 0, so attenuationDb <= 0 by construction.
        float attenuationDb = 0.0f;
        if (overshoot >= halfKnee)
            attenuationDb = overshoot * slope;
        else if (overshoot > -halfKnee)
            attenuationDb = slope * (overshoot + halfKnee) * (overshoot + halfKnee) / (2.0f * knee);

        const float makeupDb = makeupSmoother.next();
        const float targetDb = attenuationDb + makeupDb;
        gainDb += (targetDb - gainDb) * (targetDb < gainDb ? coeffAttack : coeffRelease);

        sumReductionDb += gainDb - makeupDb; // applied compression, excluding makeup
        const float gainLinear = std::exp2 (gainDb * kInvDbPerLog2);
        for (int c = 0; c < nch; ++c)
            buffer.channel (c)[i] *= gainLinear;
    }

    if (nf > 0)
        grDb.store (-sumReductionDb / (float) nf, std::memory_order_relaxed);
}

void CompressorProcessor::setThresholdDb (float db) noexcept
{
    thresholdSmoother.setTarget (std::clamp (db, -60.0f, 0.0f));
}

void CompressorProcessor::setRatio (float ratio) noexcept
{
    ratioSmoother.setTarget (std::clamp (ratio, 1.0f, 20.0f));
}

void CompressorProcessor::setAttackMs (float ms) noexcept
{
    attackMs = std::clamp (ms, 1.0f, 100.0f);
    updateCoefficients();
}

void CompressorProcessor::setReleaseMs (float ms) noexcept
{
    releaseMs = std::clamp (ms, 10.0f, 1000.0f);
    updateCoefficients();
}

void CompressorProcessor::setKneeDb (float db) noexcept
{
    kneeSmoother.setTarget (std::clamp (db, 0.0f, 24.0f));
}

void CompressorProcessor::setMakeupDb (float db) noexcept
{
    makeupSmoother.setTarget (std::clamp (db, 0.0f, 24.0f));
}

void CompressorProcessor::setRmsMode (bool useRms) noexcept { rmsMode = useRms; }
} // namespace auralis
