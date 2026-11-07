// SPDX-License-Identifier: AGPL-3.0-only
// Unit tests for the slice 3 equalizer (EqualizerProcessor).

#include "equalizer/EqualizerProcessor.h"
#include "DspTypes.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <vector>

namespace
{
int failures = 0;

#define CHECK(cond) do { if (! (cond)) { ++failures; std::cerr << "CHECK failed: " #cond \
    " at " << __FILE__ << ':' << __LINE__ << '\n'; } } while (false)

using auralis::EqualizerProcessor;

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

double sineSample (double amp, double freq, double sr, int t)
{
    return amp * std::sin (2.0 * kPi * freq * double (t) / sr);
}

void fillSine (TestBuffer& b, int blockIdx, int framesPerBlock, double freq, double amp, double sr)
{
    const int offset = blockIdx * framesPerBlock;
    for (int c = 0; c < b.numChannels; ++c)
        for (int i = 0; i < b.numFrames; ++i)
            b.sample (c, i) = (float) sineSample (amp, freq, sr, offset + i);
}

float peakAmplitude (const TestBuffer& b, int c)
{
    float peak = 0.0f;
    for (int i = 0; i < b.numFrames; ++i)
        peak = std::max (peak, std::fabs (b.sample (c, i)));
    return peak;
}

void runBlocks (EqualizerProcessor& eq, TestBuffer& buf, int blocks, double freq, double amp, double sr)
{
    for (int block = 0; block < blocks; ++block)
    {
        fillSine (buf, block, buf.numFrames, freq, amp, sr);
        auto view = buf.view();
        eq.process (view);
    }
}

void flatIsPassthrough()
{
    EqualizerProcessor eq;
    eq.prepare ({ 48000.0, 512 });
    TestBuffer buf (2, 256);
    std::vector<float> input;
    for (int block = 0; block < 3; ++block)
    {
        fillSine (buf, block, 256, 1000.0, 0.25, 48000.0);
        input = buf.storage;
        auto view = buf.view();
        eq.process (view);
    }
    for (std::size_t n = 0; n < input.size(); ++n)
        CHECK (std::fabs (buf.storage[n] - input[n]) <= 1e-4f);
}

void lowShelfBoostsBass()
{
    EqualizerProcessor eq;
    eq.prepare ({ 48000.0, 512 });
    eq.setBand (0, 100.0f, 6.0f, 0.707f);
    TestBuffer buf (2, 256);
    runBlocks (eq, buf, 16, 50.0, 0.25, 48000.0);
    // Well below the corner: close to the full +6 dB (2x). Inequality keeps it robust.
    CHECK (peakAmplitude (buf, 0) > 0.4f);
    CHECK (peakAmplitude (buf, 1) > 0.4f);
    // Far above the corner: nearly untouched.
    runBlocks (eq, buf, 16, 5000.0, 0.25, 48000.0);
    CHECK (std::fabs (peakAmplitude (buf, 0) - 0.25f) <= 0.25f * 0.10f);
    CHECK (std::fabs (peakAmplitude (buf, 1) - 0.25f) <= 0.25f * 0.10f);
}

void highShelfBoostsTreble()
{
    EqualizerProcessor eq;
    eq.prepare ({ 48000.0, 512 });
    eq.setBand (3, 8000.0f, 6.0f, 0.707f);
    TestBuffer buf (2, 256);
    runBlocks (eq, buf, 16, 10000.0, 0.25, 48000.0);
    CHECK (peakAmplitude (buf, 0) > 0.4f);
    CHECK (peakAmplitude (buf, 1) > 0.4f);
    runBlocks (eq, buf, 16, 200.0, 0.25, 48000.0);
    CHECK (std::fabs (peakAmplitude (buf, 0) - 0.25f) <= 0.25f * 0.10f);
    CHECK (std::fabs (peakAmplitude (buf, 1) - 0.25f) <= 0.25f * 0.10f);
}

void peakingBandShapesOnlyCenter()
{
    EqualizerProcessor eq;
    eq.prepare ({ 48000.0, 512 });
    eq.setBand (1, 1000.0f, 12.0f, 1.0f);
    TestBuffer buf (2, 256);
    runBlocks (eq, buf, 16, 1000.0, 0.125, 48000.0);
    // At the center frequency the peaking gain is exact: +12 dB = 4x.
    CHECK (std::fabs (peakAmplitude (buf, 0) - 0.5f) <= 0.5f * 0.10f);
    CHECK (std::fabs (peakAmplitude (buf, 1) - 0.5f) <= 0.5f * 0.10f);
    runBlocks (eq, buf, 16, 100.0, 0.25, 48000.0);
    CHECK (std::fabs (peakAmplitude (buf, 0) - 0.25f) <= 0.25f * 0.15f);
    runBlocks (eq, buf, 16, 10000.0, 0.25, 48000.0);
    CHECK (std::fabs (peakAmplitude (buf, 0) - 0.25f) <= 0.25f * 0.15f);
}

void disabledBandIsSkipped()
{
    EqualizerProcessor eq;
    eq.prepare ({ 48000.0, 512 });
    eq.setBand (0, 100.0f, 12.0f, 0.707f);
    eq.setBandEnabled (0, false);
    TestBuffer buf (2, 256);
    std::vector<float> input;
    for (int block = 0; block < 3; ++block)
    {
        fillSine (buf, block, 256, 50.0, 0.25, 48000.0);
        input = buf.storage;
        auto view = buf.view();
        eq.process (view);
    }
    for (std::size_t n = 0; n < input.size(); ++n)
        CHECK (std::fabs (buf.storage[n] - input[n]) <= 1e-4f);
}

void stabilityAndNoNaN()
{
    EqualizerProcessor eq;
    eq.prepare ({ 48000.0, 512 });
    eq.setBand (0, 50.0f, 12.0f, 0.707f);
    eq.setBand (1, 500.0f, 12.0f, 1.0f);
    eq.setBand (2, 2000.0f, 12.0f, 1.0f);
    eq.setBand (3, 10000.0f, 12.0f, 0.707f);
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
        eq.process (view);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < buf.numFrames; ++i)
            {
                const float s = buf.sample (c, i);
                CHECK (std::isfinite (s));
            }
    }
}

// S-03/U-02: automating band 1 freq over 24 blocks must stay smooth — no block-boundary
// jumps (zipper) and no NaN/overflow while the coefficients ramp per sample.
void smoothAutomationHasNoZipper()
{
    EqualizerProcessor eq;
    eq.prepare ({ 48000.0, 512 });
    TestBuffer buf (2, 256);
    constexpr int blocks = 24;
    float maxStep = 0.0f;
    float prev[2] = { 0.0f, 0.0f };
    bool havePrev = false;
    for (int block = 0; block < blocks; ++block)
    {
        const float freq = 200.0f + (2000.0f - 200.0f) * float (block) / float (blocks - 1);
        eq.setBand (1, freq, 0.0f, 1.0f);
        fillSine (buf, block, buf.numFrames, 500.0, 0.25, 48000.0);
        auto view = buf.view();
        eq.process (view);
        for (int c = 0; c < buf.numChannels; ++c)
            for (int i = 0; i < buf.numFrames; ++i)
            {
                const float s = buf.sample (c, i);
                CHECK (std::isfinite (s));
                CHECK (std::fabs (s) <= 1.0f);
                if (havePrev)
                    maxStep = std::max (maxStep, std::fabs (s - prev[c]));
                prev[c] = s;
                havePrev = true;
            }
    }
    // 500 Hz sine at 0.25 alone steps up to 2*pi*500*0.25/48000 ~= 0.0164 per sample.
    CHECK (maxStep < 0.03f);
}

} // namespace

int run_EqualizerTests()
{
    flatIsPassthrough();
    lowShelfBoostsBass();
    highShelfBoostsTreble();
    peakingBandShapesOnlyCenter();
    disabledBandIsSkipped();
    stabilityAndNoNaN();
    smoothAutomationHasNoZipper();
    return failures;
}