// SPDX-License-Identifier: AGPL-3.0-only

#include "CompressorProcessor.h"

#include <algorithm>
#include <cmath>

namespace auralis
{
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
    thresholdSmoother.snap();
    makeupSmoother.snap();
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

        const float levelDb = 20.0f * std::log10 (env < 1e-6f ? 1e-6f : env);
        const float overshoot = levelDb - thresholdSmoother.next();

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
        const float gainLinear = std::pow (10.0f, gainDb * (1.0f / 20.0f));
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
    slope = 1.0f / std::clamp (ratio, 1.0f, 20.0f) - 1.0f;
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
    knee = std::clamp (db, 0.0f, 24.0f);
    halfKnee = 0.5f * knee;
}

void CompressorProcessor::setMakeupDb (float db) noexcept
{
    makeupSmoother.setTarget (std::clamp (db, 0.0f, 24.0f));
}

void CompressorProcessor::setRmsMode (bool useRms) noexcept { rmsMode = useRms; }
} // namespace auralis
