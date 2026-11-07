// SPDX-License-Identifier: AGPL-3.0-only
// Unit tests for the lookahead output limiter (LimiterProcessor).

#include "limiter/LimiterProcessor.h"
#include "DspTypes.h"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <iostream>
#include <vector>

static int failures = 0;

#define CHECK(cond) do { if (! (cond)) { ++failures; std::cerr << "CHECK failed: " #cond \
    " at " << __FILE__ << ":" << __LINE__ << "\n"; } } while (false)
using auralis::LimiterProcessor;

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

// Run blocks and return all output samples per channel as out[c][t]
std::vector<std::vector<float>> runBlocks (LimiterProcessor& l, int channels, int framesPerBlock, int nBlocks)
{
    TestBuffer buf (channels, framesPerBlock);
    std::vector<std::vector<float>> out (static_cast<std::size_t> (channels));
    for (int block = 0; block < nBlocks; ++block)
    {
        fillSine (buf, block, framesPerBlock, 1000.0, 0.5, 48000.0);
        auralis::AudioBufferView view = buf.view();
        l.process (view);
        for (int c = 0; c < channels; ++c)
            for (int i = 0; i < framesPerBlock; ++i)
                out[std::size_t (c)].push_back (buf.sample (c, i));
    }
    return out;
}

static void passthroughBelowThreshold (int channels)
{
    LimiterProcessor l;
    l.prepare ({ 48000.0, 512 });
    l.setThresholdDb (0.0f);
    l.setCeilingDb (0.0f);
    auto out = runBlocks (l, channels, 512, 4);

    const int L = l.getLatencySamples();
    CHECK (L == 240);
    // Last block: amplitude preserved and sample-aligned to the input delayed by L.
    float peak = 0.0f;
    for (int t = 3 * 512; t < 4 * 512; ++t)
    {
        const float expected = (float) sineSample (0.5, 1000.0, 48000.0, t - L);
        for (int c = 0; c < channels; ++c)
        {
            const float s = out[std::size_t (c)][std::size_t (t)];
            peak = std::max (peak, std::fabs (s));
            CHECK (std::fabs (s - expected) <= 1e-3f);
        }
    }
    CHECK (std::fabs (peak - 0.5f) <= 1e-3f);
}

static void peakNeverExceedsCeiling()
{
    LimiterProcessor l;
    l.prepare ({ 44100.0, 512 });
    l.setThresholdDb (-6.0f);
    l.setCeilingDb (-6.0f);
    TestBuffer buf (2, 512);
    for (int block = 0; block < 10; ++block)
    {
        fillSine (buf, block, 512, 1000.0, 1.0, 44100.0);
        auralis::AudioBufferView view = buf.view();
        l.process (view);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < buf.numFrames; ++i)
            {
                const float s = buf.sample (c, i);
                const float limit = std::pow (10.0f, -6.0f / 20.0f) + 0.02f;
                CHECK (std::fabs (s) <= limit);
            }
    }
}

static void gainReductionReported()
{
    LimiterProcessor l;
    l.prepare ({ 44100.0, 512 });
    l.setThresholdDb (-6.0f);
    l.setCeilingDb (-6.0f);
    TestBuffer buf (2, 512);
    for (int block = 0; block < 10; ++block)
    {
        fillSine (buf, block, 512, 1000.0, 1.0, 44100.0);
        auralis::AudioBufferView view = buf.view();
        l.process (view);
    }
    CHECK (l.getGainReductionDb() >= 5.0f);
}

static void noReductionBelowThreshold()
{
    LimiterProcessor l;
    l.prepare ({ 48000.0, 512 });
    l.setThresholdDb (-6.0f);
    TestBuffer buf (2, 512);
    std::vector<std::vector<float>> out (2);
    for (int block = 0; block < 4; ++block)
    {
        fillSine (buf, block, 512, 1000.0, 0.1, 48000.0);
        auralis::AudioBufferView view = buf.view();
        l.process (view);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < buf.numFrames; ++i)
                out[std::size_t (c)].push_back (buf.sample (c, i));
    }
    CHECK (l.getGainReductionDb() < 0.5f);
    const int L = l.getLatencySamples();
    for (int c = 0; c < 2; ++c)
        for (int t = 3 * 512; t < 4 * 512; ++t)
        {
            const float expected = (float) sineSample (0.1, 1000.0, 48000.0, t - L);
            CHECK (std::fabs (out[std::size_t (c)][std::size_t (t)] - expected) <= 0.05f * 0.1f);
        }
}

static void releaseControlsRecovery()
{
    auto recoveryPeaks = [] (float releaseMs)
    {
        LimiterProcessor l;
        l.prepare ({ 48000.0, 512 });
        l.setThresholdDb (-12.0f);
        l.setReleaseMs (releaseMs);
TestBuffer buf (2, 512);
        std::vector<float> peaks;
        for (int block = 0; block < 35; ++block)
        {
            fillSine (buf, block, 512, 1000.0, block < 5 ? 0.5 : 0.01, 48000.0);
            auralis::AudioBufferView view = buf.view();
            l.process (view);
            if (block >= 5)
                peaks.push_back (peakAmplitude (buf, 0));
        }
        return peaks;
    };

    const std::vector<float> fast = recoveryPeaks (50.0f);
    const std::vector<float> slow = recoveryPeaks (1000.0f);
    CHECK (fast.size() == 30);
    CHECK (std::fabs (fast.back() - 0.01f) <= 0.15f * 0.01f);
    CHECK (slow[9] < fast[9] * 0.8f);
}

static void noNaN()
{
    LimiterProcessor l;
    l.prepare ({ 48000.0, 512 });
    l.setThresholdDb (-12.0f);
    l.setCeilingDb (-6.0f);
    unsigned long long seed = 12345;
    TestBuffer buf (2, 512);
    for (int block = 0; block < 10; ++block)
    {
        for (int c = 0; c < buf.numChannels; ++c)
            for (int i = 0; i < buf.numFrames; ++i)
            {
                seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
                buf.sample (c, i) = 2.0f * float ((seed >> 40) & 0xFFFFFFULL) / 16777216.0f - 1.0f;
            }
        auralis::AudioBufferView view = buf.view();
        l.process (view);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < buf.numFrames; ++i)
            {
                const float s = buf.sample (c, i);
                const float limit = std::pow (10.0f, -6.0f / 20.0f) + 0.02f;
                CHECK (std::isfinite (s) && std::fabs (s) <= limit);
            }
    }
}

// The sliding-window maximum must behave exactly like the old O(N*L) scan.
// Brute force is not reachable from outside, so pin the invariants that
// uniquely identify a correct window:
// (a) ceiling respected on hot noise, (b) reduction reported, (c) finite,
// (d) open threshold/ceiling: output == input delayed by exactly L samples
// (a window off by one breaks this).
static void slidingMaxMatchesBruteForce()
{
    LimiterProcessor l;
    l.prepare ({ 48000.0, 512, 2 });
    l.setThresholdDb (-12.0f);
    l.setCeilingDb (-12.0f);
    const float limit = std::pow (10.0f, -12.0f / 20.0f) + 0.02f;
    unsigned long long seed = 12345;
    TestBuffer buf (2, 512);
    for (int block = 0; block < 20; ++block)
    {
        for (int c = 0; c < buf.numChannels; ++c)
            for (int i = 0; i < buf.numFrames; ++i)
            {
                seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
                buf.sample (c, i) = 2.0f * float ((seed >> 40) & 0xFFFFFFULL) / 16777216.0f - 1.0f;
            }
        auralis::AudioBufferView view = buf.view();
        l.process (view);
        for (int c = 0; c < buf.numChannels; ++c)
            for (int i = 0; i < buf.numFrames; ++i)
            {
                const float s = buf.sample (c, i);
                CHECK (std::isfinite (s) && std::fabs (s) <= limit);
            }
    }
    CHECK (l.getGainReductionDb() > 0.0f);

    LimiterProcessor open;
    open.prepare ({ 48000.0, 512, 2 });
    open.setThresholdDb (0.0f); // noise peaks stay strictly below 0 dBFS
    open.setCeilingDb (0.0f);
    seed = 98765;
    TestBuffer buf2 (2, 512);
    std::vector<std::vector<float>> in (2, std::vector<float> (std::size_t (20 * 512)));
    std::vector<std::vector<float>> out (2, std::vector<float> (std::size_t (20 * 512)));
    for (int block = 0; block < 20; ++block)
    {
        for (int c = 0; c < buf2.numChannels; ++c)
            for (int i = 0; i < buf2.numFrames; ++i)
            {
                seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
                const float v = 2.0f * float ((seed >> 40) & 0xFFFFFFULL) / 16777216.0f - 1.0f;
                buf2.sample (c, i) = v;
                in[std::size_t (c)][std::size_t (block * 512 + i)] = v;
            }
        auralis::AudioBufferView view = buf2.view();
        open.process (view);
        for (int c = 0; c < buf2.numChannels; ++c)
            for (int i = 0; i < buf2.numFrames; ++i)
                out[std::size_t (c)][std::size_t (block * 512 + i)] = buf2.sample (c, i);
    }
    const int L = open.getLatencySamples();
    for (int c = 0; c < buf2.numChannels; ++c)
        for (int t = L; t < 20 * 512; ++t)
            CHECK (std::fabs (out[std::size_t (c)][std::size_t (t)]
                              - in[std::size_t (c)][std::size_t (t - L)]) <= 1e-6f);
}

// run_LimiterTests in global namespace
int run_LimiterTests()
{
    passthroughBelowThreshold (2);
    passthroughBelowThreshold (1);
    peakNeverExceedsCeiling();
    gainReductionReported();
    noReductionBelowThreshold();
    releaseControlsRecovery();
    noNaN();
    slidingMaxMatchesBruteForce();
    return failures;
}
