// SPDX-License-Identifier: AGPL-3.0-only
// Unit tests for the slice 4 bass enhancer (BassEnhancerProcessor).

#include "bass/BassEnhancerProcessor.h"
#include "DspTypes.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <vector>

static int failures = 0;

#define CHECK(cond) do { if (! (cond)) { ++failures; std::cerr << "CHECK failed: " #cond \
    " at " << __FILE__ << ":" << __LINE__ << "\n"; } } while (false)

using auralis::BassEnhancerProcessor;

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
    float sample (int c, int i) const { return storage[std::size_t (c) * std::size_t (numFrames) + std::size_t (i)]; }
    float& sample (int c, int i) { return storage[std::size_t (c) * std::size_t (numFrames) + std::size_t (i)]; }
    int numChannels = 0;
    int numFrames = 0;
    std::vector<float> storage;
    float* chans[2] { nullptr, nullptr };
};

constexpr double kPi = 3.14159265358979323846;

static double sineSample (double amp, double freq, double sr, int t)
{
    return amp * std::sin (2.0 * kPi * freq * double (t) / sr);
}

static void fillSine (TestBuffer& b, int blockIdx, int framesPerBlock, double freq, double amp, double sr)
{
    const int offset = blockIdx * framesPerBlock;
    for (int c = 0; c < b.numChannels; ++c)
        for (int i = 0; i < b.numFrames; ++i)
            b.sample (c, i) = (float) sineSample (amp, freq, sr, offset + i);
}

static float peakAmplitude (const TestBuffer& b, int c)
{
    float peak = 0.0f;
    for (int i = 0; i < b.numFrames; ++i)
        peak = std::max (peak, std::fabs (b.sample (c, i)));
    return peak;
}

static void runBlocks (BassEnhancerProcessor& p, TestBuffer& buf, int blocks, double freq, double amp, double sr)
{
    for (int block = 0; block < blocks; ++block)
    {
        fillSine (buf, block, buf.numFrames, freq, amp, sr);
        auto view = buf.view();
        p.process (view);
    }
}

static void defaultsArePassthrough()
{
    BassEnhancerProcessor p;
    p.prepare ({ 48000.0, 512 });
    TestBuffer buf (2, 256);
    std::vector<float> input;
    for (int block = 0; block < 3; ++block)
    {
        fillSine (buf, block, 256, 500.0, 0.5, 48000.0);
        input = buf.storage;
        auto view = buf.view();
        p.process (view);
    }
    for (std::size_t n = 0; n < input.size(); ++n)
        CHECK (std::fabs (buf.storage[n] - input[n]) <= 1e-4f);
}

static void subProtectionAttenuatesSubs()
{
    BassEnhancerProcessor p;
    p.prepare ({ 48000.0, 512 });
    p.setSubHighpassFreq (40.0f);
    TestBuffer buf (2, 256);
    runBlocks (p, buf, 16, 25.0, 0.5, 48000.0);
    CHECK (peakAmplitude (buf, 0) < 0.1f);
    runBlocks (p, buf, 8, 200.0, 0.5, 48000.0);
    CHECK (peakAmplitude (buf, 0) > 0.4f);
}

static void shelfBoostsBassOnly()
{
    BassEnhancerProcessor p;
    p.prepare ({ 48000.0, 512 });
    p.setShelfFreq (100.0f);
    p.setShelfGainDb (6.0f);
    TestBuffer buf (2, 256);
    runBlocks (p, buf, 8, 80.0, 0.25, 48000.0);
    CHECK (peakAmplitude (buf, 0) > 0.25f * 1.3f);
    runBlocks (p, buf, 8, 5000.0, 0.25, 48000.0);
    CHECK (std::fabs (peakAmplitude (buf, 0) - 0.25f) <= 0.25f * 0.10f);
}

static void saturationBoundedAndHarmonic()
{
    BassEnhancerProcessor p;
    p.prepare ({ 48000.0, 512 });
    p.setSaturation (1.0f);
    TestBuffer buf (2, 256);
    runBlocks (p, buf, 8, 1000.0, 1.2, 48000.0);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < buf.numFrames; ++i)
        {
            const float s = buf.sample (c, i);
            CHECK (std::isfinite (s) && std::fabs (s) <= 1.05f);
        }
    // With saturation, peak should compress compared to sat=0
    BassEnhancerProcessor p0;
    p0.prepare ({ 48000.0, 512 });
    TestBuffer buf0 (2, 256);
    runBlocks (p0, buf0, 8, 1000.0, 1.2, 48000.0);
    float peakSat = peakAmplitude (buf, 0);
    float peakClean = peakAmplitude (buf0, 0);
    CHECK (peakSat <= peakClean + 0.05f);
}

static void compensationReducesLevel()
{
    BassEnhancerProcessor pSat;
    pSat.prepare ({ 48000.0, 512 });
    pSat.setShelfGainDb (12.0f);
    pSat.setSaturation (1.0f);
    BassEnhancerProcessor pClean;
    pClean.prepare ({ 48000.0, 512 });
    pClean.setShelfGainDb (12.0f);
    pClean.setSaturation (0.0f);
    TestBuffer bufSat (2, 256), bufClean (2, 256);
    runBlocks (pSat, bufSat, 8, 100.0, 0.5, 48000.0);
    runBlocks (pClean, bufClean, 8, 100.0, 0.5, 48000.0);
    CHECK (peakAmplitude (bufSat, 0) <= peakAmplitude (bufClean, 0) + 0.05f);
}

static void noNaNAndStable()
{
    BassEnhancerProcessor p;
    p.prepare ({ 48000.0, 512 });
    p.setShelfGainDb (12.0f);
    p.setSaturation (1.0f);
    p.setSubHighpassFreq (30.0f);
    TestBuffer buf (2, 256);
    unsigned int seed = 12345u;
    for (int block = 0; block < 8; ++block)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < buf.numFrames; ++i)
            {
                seed = seed * 1103515245u + 12345u;
                buf.sample (c, i) = (float) (seed >> 16) / 32768.0f - 1.0f;
            }
        auto view = buf.view();
        p.process (view);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < buf.numFrames; ++i)
            {
                const float s = buf.sample (c, i);
                CHECK (std::isfinite (s) && std::fabs (s) <= 4.0f);
            }
    }
}

// run_BassTests in global namespace so TestMain.cpp can see it
int run_BassTests()
{
    defaultsArePassthrough();
    subProtectionAttenuatesSubs();
    shelfBoostsBassOnly();
    saturationBoundedAndHarmonic();
    compensationReducesLevel();
    noNaNAndStable();
    return failures;
}
