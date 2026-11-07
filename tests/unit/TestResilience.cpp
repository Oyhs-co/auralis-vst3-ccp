// SPDX-License-Identifier: AGPL-3.0-only
// U-16: the full default chain must stay finite and bounded when a host feeds
// NaN/inf garbage or tiny blocks. run_ResilienceTests() returns the failure count.

#include "DspPipeline.h"
#include "DspTypes.h"
#include "gain/GainProcessor.h"
#include "equalizer/EqualizerProcessor.h"
#include "bass/BassEnhancerProcessor.h"
#include "clarity/ClarityProcessor.h"
#include "dynamics/CompressorProcessor.h"
#include "ambience/AmbienceProcessor.h"
#include "stereo/StereoEnhancerProcessor.h"
#include "limiter/LimiterProcessor.h"

#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

namespace
{
int failures = 0;

#define CHECK(cond) do { if (! (cond)) { ++failures; std::cerr << "CHECK failed: " #cond \
    " at " << __FILE__ << ':' << __LINE__ << '\n'; } } while (false)

constexpr double kPi = 3.14159265358979323846;

// Owns sample storage; hands out non-owning views over raw channel pointers.
struct TestBuffer
{
    TestBuffer (int channels, int frames)
        : numChannels (channels), numFrames (frames),
          storage (std::size_t (channels) * std::size_t (frames))
    {
        for (int c = 0; c < numChannels; ++c)
            chans[c] = storage.data() + std::size_t (c) * std::size_t (numFrames);
    }

    auralis::AudioBufferView view() { return { chans, numChannels, numFrames }; }

    void writeSine (double freq, double amplitude, double sampleRate, int startFrame)
    {
        for (int c = 0; c < numChannels; ++c)
            for (int i = 0; i < numFrames; ++i)
                sample (c, i) = (float) (amplitude
                    * std::sin (2.0 * kPi * freq * (double) (startFrame + i) / sampleRate));
    }

    void writeGarbageCycle (int startFrame) // NaN, +inf, -inf, 1e30 repeating
    {
        const float cycle[4] = { std::numeric_limits<float>::quiet_NaN(),
                                 std::numeric_limits<float>::infinity(),
                                 -std::numeric_limits<float>::infinity(),
                                 1.0e30f };
        for (int c = 0; c < numChannels; ++c)
            for (int i = 0; i < numFrames; ++i)
                sample (c, i) = cycle[(startFrame + i) & 3];
    }

    void fill (float value)
    {
        for (int c = 0; c < numChannels; ++c)
            for (int i = 0; i < numFrames; ++i)
                sample (c, i) = value;
    }

    bool allFinite() const
    {
        for (float s : storage)
            if (! std::isfinite (s))
                return false;
        return true;
    }

    float peakAbs() const
    {
        float p = 0.0f;
        for (float s : storage)
        {
            const float a = std::fabs (s);
            if (a > p) p = a;
        }
        return p;
    }

    float& sample (int c, int i) { return storage[std::size_t (c) * std::size_t (numFrames) + std::size_t (i)]; }

    int numChannels = 0;
    int numFrames = 0;
    std::vector<float> storage;
    float* chans[2] { nullptr, nullptr };
};

// The shipping chain in order; every default parameter is neutral per the
// per-processor test files.
void buildFullChain (auralis::DspPipeline& pipeline)
{
    pipeline.addProcessor (std::make_unique<auralis::GainProcessor>());
    pipeline.addProcessor (std::make_unique<auralis::EqualizerProcessor>());
    pipeline.addProcessor (std::make_unique<auralis::BassEnhancerProcessor>());
    pipeline.addProcessor (std::make_unique<auralis::ClarityProcessor>());
    pipeline.addProcessor (std::make_unique<auralis::CompressorProcessor>());
    pipeline.addProcessor (std::make_unique<auralis::AmbienceProcessor>());
    pipeline.addProcessor (std::make_unique<auralis::StereoEnhancerProcessor>());
    pipeline.addProcessor (std::make_unique<auralis::LimiterProcessor>());
}

void nanInfIsSanitizedAtFourSampleRates()
{
    const double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0 };
    for (double sr : rates)
    {
        auralis::DspPipeline pipeline;
        buildFullChain (pipeline);
        pipeline.prepare ({ sr, 512 });

        TestBuffer buf (2, 512);
        int frame = 0;
        for (int block = 0; block < 8; ++block)
        {
            buf.writeGarbageCycle (frame);
            frame += buf.numFrames;
            auto view = buf.view();
            pipeline.process (view);
            CHECK (buf.allFinite());
        }

        // Clean phase-continuous sine afterwards: proves no poisoned state
        // survives the garbage (the limiter ceiling keeps the output bounded).
        for (int block = 0; block < 10; ++block)
        {
            buf.writeSine (1000.0, 0.5, sr, frame);
            frame += buf.numFrames;
            auto view = buf.view();
            pipeline.process (view);
            CHECK (buf.allFinite());
        }
        CHECK (buf.peakAbs() <= 2.0f);
    }
}

void tinyBlocksWithGarbage()
{
    auralis::DspPipeline pipeline;
    buildFullChain (pipeline);
    pipeline.prepare ({ 48000.0, 512 });

    TestBuffer buf (2, 1); // 1-sample blocks against a 512 max block size
    const float garbage[2] = { std::numeric_limits<float>::quiet_NaN(),
                               std::numeric_limits<float>::infinity() };
    for (int i = 0; i < 64; ++i)
    {
        buf.fill (garbage[i & 1]);
        auto view = buf.view();
        pipeline.process (view);
        CHECK (buf.allFinite());
    }
}

void mixedRealGarbage()
{
    auralis::DspPipeline pipeline;
    buildFullChain (pipeline);
    pipeline.prepare ({ 48000.0, 512 });

    TestBuffer buf (2, 512);
    int frame = 0;
    int cleanBlocksSeen = 0;
    for (int block = 0; block < 12; ++block)
    {
        const bool clean = (block % 3) != 1; // clean, all-NaN, clean, ...
        if (clean)
        {
            buf.writeSine (1000.0, 0.5, 48000.0, frame);
            ++cleanBlocksSeen;
        }
        else
        {
            buf.fill (std::numeric_limits<float>::quiet_NaN());
        }
        frame += buf.numFrames;

        auto view = buf.view();
        pipeline.process (view);
        CHECK (buf.allFinite());
        // Defaults are neutral, so clean blocks come back at ~0.5 peak; the
        // first one is skipped because smoothers may still be settling.
        if (clean && cleanBlocksSeen > 1)
            CHECK (buf.peakAbs() <= 0.6f);
    }
}

} // namespace

// run_ResilienceTests in global namespace
int run_ResilienceTests()
{
    nanInfIsSanitizedAtFourSampleRates();
    tinyBlocksWithGarbage();
    mixedRealGarbage();
    return failures;
}
