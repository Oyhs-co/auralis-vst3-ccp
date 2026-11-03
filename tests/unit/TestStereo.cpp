// SPDX-License-Identifier: AGPL-3.0-only
// Unit tests for the slice 8 stereo enhancement processor (M/S, width, balance, correlation).

#include "stereo/StereoEnhancerProcessor.h"
#include "DspTypes.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

static int failures = 0;

#define CHECK(cond) do { if (! (cond)) { ++failures; std::cerr << "CHECK failed: " #cond \
    " at " << __FILE__ << ':' << __LINE__ << '\n'; } } while (false)
using auralis::StereoEnhancerProcessor;

// Owns sample storage; hands out non-owning views over raw channel pointers (1 or 2 channels).
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

// step 0 turns the ramp into a constant fill (all channels identical).
static void fillRamp (TestBuffer& b, float start, float step)
{
    for (int c = 0; c < b.numChannels; ++c)
        for (int i = 0; i < b.numFrames; ++i)
            b.sample (c, i) = start + step * float (i);
}

static void process (StereoEnhancerProcessor& s, TestBuffer& buf)
{
    auto view = buf.view();
    s.process (view);
}

static void processBlocks (StereoEnhancerProcessor& s, TestBuffer& buf, int blocks)
{
    for (int b = 0; b < blocks; ++b)
        process (s, buf);
}

// Largest absolute sample of one channel.
static float peak (const TestBuffer& b, int c)
{
    float p = 0.0f;
    for (int i = 0; i < b.numFrames; ++i)
        p = std::max (p, std::fabs (b.sample (c, i)));
    return p;
}

// Deterministic uniform noise in [-1, 1).
struct Noise
{
    std::uint32_t state = 0;
    float next()
    {
        state = state * 1664525u + 1013904223u;
        return float (state >> 8) / float (1u << 24) * 2.0f - 1.0f;
    }
};

static void identityAtDefaults()
{
    StereoEnhancerProcessor s;
    s.prepare ({ 48000.0, 512 });
    s.setWidth (1.0f);
    s.setMidGainDb (0.0f);
    s.setSideGainDb (0.0f);
    s.setBalance (0.0f);
    TestBuffer buf (2, 256);
    for (int i = 0; i < buf.numFrames; ++i) // different L and R ramps
    {
        buf.sample (0, i) = -0.4f + 0.004f * float (i);
        buf.sample (1, i) = 0.3f - 0.003f * float (i);
    }
    const auto input = buf.storage;
    processBlocks (s, buf, 2);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < buf.numFrames; ++i)
            CHECK (std::fabs (buf.sample (c, i) - input[std::size_t (c) * std::size_t (buf.numFrames) + std::size_t (i)]) <= 1e-6f);
}

static void zeroWidthIsMonoCompatible()
{
    StereoEnhancerProcessor s;
    s.prepare ({ 48000.0, 512 });
    s.setWidth (0.0f);
    TestBuffer buf (2, 256);
    // w decays exponentially from 1; settle well past 20 ms so the residual
    // side term (2 * side * w) is far below 1e-6.
    for (int block = 0; block < 64; ++block)
    {
        for (int i = 0; i < buf.numFrames; ++i)
        {
            buf.sample (0, i) = ((i * 37 + block * 11) % 128) / 64.0f - 1.0f;
            buf.sample (1, i) = ((i * 53 + block * 7) % 128) / 64.0f - 1.0f;
        }
        process (s, buf);
        if (block == 63)
            for (int i = 0; i < buf.numFrames; ++i)
                CHECK (std::fabs (buf.sample (0, i) - buf.sample (1, i)) <= 1e-6f);
    }
}

static void sideGainShapesImage()
{
    StereoEnhancerProcessor s;
    s.prepare ({ 48000.0, 512 });
    s.setWidth (1.0f);
    s.setMidGainDb (0.0f);
    s.setSideGainDb (6.0f);
    TestBuffer buf (2, 256);
    for (int block = 0; block < 40; ++block)
    {
        fillRamp (buf, 0.25f, 0.0f);
        for (int i = 0; i < buf.numFrames; ++i)
            buf.sample (1, i) = -buf.sample (0, i);
        process (s, buf);
    }
    CHECK (std::fabs (peak (buf, 0) - 0.5f) <= 0.015f);
    CHECK (std::fabs (peak (buf, 1) - 0.5f) <= 0.015f);
}

static void midGainShapesCenter()
{
    StereoEnhancerProcessor s;
    s.prepare ({ 48000.0, 512 });
    s.setMidGainDb (-6.0f);
    s.setSideGainDb (0.0f);
    TestBuffer buf (2, 256);
    for (int block = 0; block < 40; ++block)
    {
        fillRamp (buf, 0.5f, 0.0f);
        process (s, buf);
    }
    CHECK (std::fabs (peak (buf, 0) - 0.25f) <= 0.0075f);
    CHECK (std::fabs (peak (buf, 1) - 0.25f) <= 0.0075f);
}

static void balanceShiftsChannels()
{
    StereoEnhancerProcessor s;
    s.prepare ({ 48000.0, 512 });
    s.setBalance (-0.5f); // unsmoothed; gL = 0.5, gR = 1
    TestBuffer buf (2, 256);
    for (int block = 0; block < 2; ++block)
    {
        fillRamp (buf, 0.5f, 0.0f); // refill: dual mono 0.5 each block
        process (s, buf);
    }
    CHECK (std::fabs (peak (buf, 0) - 0.25f) <= 0.0075f);
    CHECK (std::fabs (peak (buf, 1) - 0.5f) <= 0.015f);
}

static void correlationValues()
{
    StereoEnhancerProcessor s;
    s.prepare ({ 48000.0, 512 });
    TestBuffer buf (2, 512);

    fillRamp (buf, 0.5f, 0.0f); // dual mono -> +1
    process (s, buf);
    CHECK (std::fabs (s.getCorrelation() - 1.0f) <= 0.01f);

    for (int i = 0; i < buf.numFrames; ++i)
        buf.sample (1, i) = -buf.sample (0, i); // R = -L -> -1
    process (s, buf);
    CHECK (std::fabs (s.getCorrelation() + 1.0f) <= 0.01f);

    Noise nl { 12345u };
    Noise nr { 987654321u }; // uncorrelated pseudo-noise -> near 0
    for (int i = 0; i < buf.numFrames; ++i)
    {
        buf.sample (0, i) = nl.next();
        buf.sample (1, i) = nr.next();
    }
    process (s, buf);
    CHECK (std::fabs (s.getCorrelation()) < 0.3f);
}

static void monoInputPassthrough()
{
    StereoEnhancerProcessor s;
    s.prepare ({ 48000.0, 512 });
    s.setWidth (0.0f);
    s.setMidGainDb (-12.0f);
    TestBuffer buf (1, 128);
    fillRamp (buf, 0.5f, -0.01f);
    const auto input = buf.storage;
    processBlocks (s, buf, 2);
    for (int i = 0; i < buf.numFrames; ++i)
        CHECK (std::fabs (buf.sample (0, i) - input[std::size_t (i)]) <= 1e-6f);
    CHECK (std::fabs (s.getCorrelation() - 1.0f) <= 0.01f);
}

static void noNaN()
{
    StereoEnhancerProcessor s;
    s.prepare ({ 48000.0, 512 });
    s.setWidth (2.0f);
    s.setMidGainDb (12.0f);
    s.setSideGainDb (12.0f);
    TestBuffer buf (2, 256);
    Noise nl { 7u };
    Noise nr { 424242u };
    for (int block = 0; block < 8; ++block)
    {
        for (int i = 0; i < buf.numFrames; ++i)
        {
            buf.sample (0, i) = nl.next();
            buf.sample (1, i) = nr.next();
        }
        process (s, buf);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < buf.numFrames; ++i)
            {
                const float x = buf.sample (c, i);
                CHECK (std::isfinite (x) && std::fabs (x) <= 8.0f);
            }
    }
}

// run_StereoTests in global namespace
int run_StereoTests()
{
    const struct { const char* name; void (*fn)(); } tests[] = {
        { "identityAtDefaults", identityAtDefaults },
        { "zeroWidthIsMonoCompatible", zeroWidthIsMonoCompatible },
        { "sideGainShapesImage", sideGainShapesImage },
        { "midGainShapesCenter", midGainShapesCenter },
        { "balanceShiftsChannels", balanceShiftsChannels },
        { "correlationValues", correlationValues },
        { "monoInputPassthrough", monoInputPassthrough },
        { "noNaN", noNaN },
    };

    for (const auto& t : tests)
    {
        try { t.fn(); }
        catch (const std::exception& e) { ++failures; std::cerr << "EXCEPTION in " << t.name << ": " << e.what() << '\n'; }
        catch (...) { ++failures; std::cerr << "UNKNOWN EXCEPTION in " << t.name << '\n'; }
    }

    if (failures != 0)
        std::cerr << failures << " stereo check(s) failed\n";
    return failures;
}
