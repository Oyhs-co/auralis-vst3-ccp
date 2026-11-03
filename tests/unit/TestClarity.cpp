// SPDX-License-Identifier: AGPL-3.0-only
// Unit tests for the slice 5 clarity processor (ClarityProcessor).

#include "clarity/ClarityProcessor.h"
#include "DspTypes.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <vector>

static int failures = 0;

#define CHECK(cond) do { if (! (cond)) { ++failures; std::cerr << "CHECK failed: " #cond \
    " at " << __FILE__ << ":" << __LINE__ << "\n"; } } while (false)

using auralis::ClarityProcessor;

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

static void runBlocks (ClarityProcessor& p, TestBuffer& buf, int blocks, double freq, double amp, double sr)
{
    for (int block = 0; block < blocks; ++block)
    {
        fillSine (buf, block, buf.numFrames, freq, amp, sr);
        auto view = buf.view();
        p.process (view);
    }
}

static void zeroAmountIsExactPassthrough()
{
    ClarityProcessor p;
    p.prepare ({ 48000.0, 512 });
    p.setAmount (0.0f);
    p.setPresenceDb (6.0f);
    p.setCenterFreq (3000.0f);
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
        CHECK (std::fabs (buf.storage[n] - input[n]) <= 1e-6f);
}

static void exciterAddsHighs()
{
    ClarityProcessor p;
    p.prepare ({ 48000.0, 512 });
    p.setAmount (1.0f);
    p.setPresenceDb (6.0f);
    p.setCenterFreq (3000.0f);
    TestBuffer buf (2, 256);
    runBlocks (p, buf, 12, 6000.0, 0.25, 48000.0);
    CHECK (peakAmplitude (buf, 0) > 0.5f); // excited + presence > input*2
    // 100 Hz nearly untouched: passes through HPF then gain*comp ~ 1/(1+0.15) = 0.87
    runBlocks (p, buf, 12, 100.0, 0.25, 48000.0);
    CHECK (std::fabs (peakAmplitude (buf, 0) - 0.25f * 0.87f) <= 0.25f * 0.12f);
}

static void negativePresenceAttenuatesHighs()
{
    auto peakWithPresence = [] (float presenceDb)
    {
        ClarityProcessor p;
        p.prepare ({ 48000.0, 512 });
        p.setAmount (1.0f);
        p.setPresenceDb (presenceDb);
        p.setCenterFreq (3000.0f);
        TestBuffer buf (2, 256);
        runBlocks (p, buf, 12, 6000.0, 0.25, 48000.0);
        return peakAmplitude (buf, 0);
    };

    CHECK (peakWithPresence (-6.0f) < peakWithPresence (0.0f));
}

static void compensationKeepsLevelBounded()
{
    ClarityProcessor p;
    p.prepare ({ 48000.0, 512 });
    p.setAmount (1.0f);
    p.setPresenceDb (0.0f);
    TestBuffer buf (2, 256);
    unsigned int seed = 12345u;
    for (int block = 0; block < 8; ++block)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < buf.numFrames; ++i)
            {
                seed = seed * 1103515245u + 12345u;
                buf.sample (c, i) = 2.0f * float ((seed >> 16) & 0xFFFFu) / 65535.0f - 1.0f;
            }
        auto view = buf.view();
        p.process (view);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < buf.numFrames; ++i)
            {
                const float s = buf.sample (c, i);
                CHECK (std::isfinite (s) && std::fabs (s) <= 2.0f);
            }
    }
}

static void noNaNLoud()
{
    ClarityProcessor p;
    p.prepare ({ 48000.0, 512 });
    p.setAmount (1.0f);
    p.setPresenceDb (6.0f);
    p.setCenterFreq (3000.0f);
    TestBuffer buf (2, 256);
    unsigned int seed = 98765u;
    for (int block = 0; block < 8; ++block)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < buf.numFrames; ++i)
            {
                buf.sample (c, i) = (i % 2 == 0) ? 1.0f : -1.0f;
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

// run_ClarityTests in global namespace
int run_ClarityTests()
{
    zeroAmountIsExactPassthrough();
    exciterAddsHighs();
    negativePresenceAttenuatesHighs();
    compensationKeepsLevelBounded();
    noNaNLoud();
    return failures;
}
