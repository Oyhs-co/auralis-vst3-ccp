// SPDX-License-Identifier: AGPL-3.0-only
// Unit tests for the dynamics slice (CompressorProcessor).

#include "dynamics/CompressorProcessor.h"
#include "DspTypes.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <memory>
#include <vector>

static int failures = 0;

#define CHECK(cond) do { if (! (cond)) { ++failures; std::cerr << "CHECK failed: " #cond \
    " at " << __FILE__ << ':' << __LINE__ << '\n'; } } while (false)
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
    float sample (int c, int i) const { return storage[std::size_t (c) * std::size_t (numFrames) + std::size_t (i)]; }
    float& sample (int c, int i) { return storage[std::size_t (c) * std::size_t (numFrames) + std::size_t (i)]; }
    int numChannels = 0;
    int numFrames = 0;
    std::vector<float> storage;
    float* chans[2] { nullptr, nullptr };
};

static void fillSine (TestBuffer& b, float amp, double freqHz, double sampleRate, int startSample)
{
    for (int c = 0; c < b.numChannels; ++c)
        for (int i = 0; i < b.numFrames; ++i)
            b.sample (c, i) = amp * (float) std::sin (
                2.0 * 3.14159265358979323846 * freqHz * double (startSample + i) / sampleRate);
}

// Steady-state amplitude: largest absolute sample in the buffer.
static float measureAmplitude (const TestBuffer& b)
{
    float peak = 0.0f;
    for (int c = 0; c < b.numChannels; ++c)
        for (int i = 0; i < b.numFrames; ++i)
            peak = std::max (peak, std::fabs (b.sample (c, i)));
    return peak;
}

// (unused processBlocks helper removed — dead code, AGENTS §4)

static void transparentAtDefaults()
{
    auralis::CompressorProcessor c;
    c.prepare ({ 48000.0, 512 });
    c.setThresholdDb (0.0f);
    c.setRatio (1.0f);
    c.setKneeDb (6.0f);
    c.setMakeupDb (0.0f);

    TestBuffer buf (2, 512);
    int pos = 0;
    std::vector<float> input;
    for (int block = 0; block < 6; ++block)
    {
        fillSine (buf, 0.5f, 1000.0, 48000.0, pos);
        pos += buf.numFrames;
        input = buf.storage;
        auto view = buf.view();
        c.process (view);
    }
    for (std::size_t n = 0; n < input.size(); ++n)
        CHECK (std::fabs (buf.storage[n] - input[n]) <= 1e-3f);
}

struct StaticCurveRun
{
    float outputAmplitude = 0.0f;
    float gainReductionDb = 0.0f;
};

// Threshold -20 dB, ratio 4, knee 0, attack 5 ms, release 100 ms, 0.5 sine (-6.02 dBFS).
StaticCurveRun runStaticCurve (int blocks)
{
    auralis::CompressorProcessor c;
    c.prepare ({ 48000.0, 512 });
    c.setThresholdDb (-20.0f);
    c.setRatio (4.0f);
    c.setKneeDb (0.0f);
    c.setAttackMs (5.0f);
    c.setReleaseMs (100.0f);

    TestBuffer buf (2, 512);
    int pos = 0;
    for (int block = 0; block < blocks; ++block)
    {
        fillSine (buf, 0.5f, 1000.0, 48000.0, pos);
        pos += buf.numFrames;
        auto view = buf.view();
        c.process (view);
    }
    return { measureAmplitude (buf), c.getGainReductionDb() };
}

static void staticCurveMatchesMath()
{
    // 13.98 dB overshoot -> 10.49 dB attenuation -> 0.5 * 10^(-10.49/20) ~= 0.1497.
    // Detector ripple with real ballistics is allowed for.
    const auto run = runStaticCurve (60);
    CHECK (std::fabs (run.outputAmplitude - 0.1497f) <= 0.1497f * 0.12f);
}

static void grMeterReportsReduction()
{
    const auto run = runStaticCurve (60);
    CHECK (run.gainReductionDb >= 9.0f);
    CHECK (run.gainReductionDb <= 12.0f);
}

static void makeupAppliesCleanly()
{
    auralis::CompressorProcessor c;
    c.prepare ({ 48000.0, 512 });
    c.setThresholdDb (0.0f);
    c.setRatio (1.0f); // no gain reduction, makeup alone
    c.setMakeupDb (6.0f);

    TestBuffer buf (2, 512);
    int pos = 0;
    for (int block = 0; block < 40; ++block)
    {
        fillSine (buf, 0.5f, 1000.0, 48000.0, pos);
        pos += buf.numFrames;
        auto view = buf.view();
        c.process (view);
    }
    const float amp = measureAmplitude (buf); // settled: ~2x input = 1.0
    CHECK (std::fabs (amp - 1.0f) <= 0.03f);
}

static void belowThresholdNoReduction()
{
    auralis::CompressorProcessor c;
    c.prepare ({ 48000.0, 512 });
    c.setThresholdDb (-10.0f);
    c.setRatio (4.0f);
    c.setKneeDb (0.0f);

    TestBuffer buf (1, 512); // mono path
    int pos = 0;
    for (int block = 0; block < 10; ++block)
    {
        fillSine (buf, 0.1f, 1000.0, 48000.0, pos); // -20 dBFS, 10 dB below threshold
        pos += buf.numFrames;
        auto view = buf.view();
        c.process (view);
    }
    const float amp = measureAmplitude (buf);
    CHECK (std::fabs (amp - 0.1f) <= 0.1f * 0.03f);
}

static void releaseRecoversToUnity()
{
    auralis::CompressorProcessor c;
    c.prepare ({ 48000.0, 512 });
    c.setThresholdDb (-30.0f);
    c.setRatio (10.0f);
    c.setAttackMs (5.0f);
    c.setReleaseMs (100.0f);

    TestBuffer buf (2, 512);
    int pos = 0;
    for (int block = 0; block < 6; ++block) // compress hard first
    {
        fillSine (buf, 0.5f, 1000.0, 48000.0, pos);
        pos += buf.numFrames;
        auto view = buf.view();
        c.process (view);
    }
    for (int block = 0; block < 40; ++block) // quiet input: release must return to ~unity
    {
        fillSine (buf, 0.01f, 1000.0, 48000.0, pos);
        pos += buf.numFrames;
        auto view = buf.view();
        c.process (view);
    }
    const float amp = measureAmplitude (buf);
    CHECK (std::fabs (amp - 0.01f) <= 0.01f * 0.10f);
}

static void noNaNWithExtremeSettings()
{
    auralis::CompressorProcessor c;
    c.prepare ({ 48000.0, 512 });
    c.setThresholdDb (-40.0f);
    c.setRatio (20.0f);

    TestBuffer buf (2, 512);
    unsigned int seed = 12345u;
    for (int block = 0; block < 8; ++block)
    {
        for (int ch = 0; ch < buf.numChannels; ++ch)
            for (int i = 0; i < buf.numFrames; ++i)
            {
                seed = seed * 1103515245u + 12345u;
                buf.sample (ch, i) = (float) (seed >> 16) / 32768.0f - 1.0f; // pseudo-noise in [-1, 1)
            }
        auto view = buf.view();
        c.process (view);
        for (int ch = 0; ch < buf.numChannels; ++ch)
            for (int i = 0; i < buf.numFrames; ++i)
            {
                const float s = buf.sample (ch, i);
                CHECK (std::isfinite (s) && std::fabs (s) <= 8.0f);
            }
    }
}

static void rmsModeReducesGain()
{
    auralis::CompressorProcessor c;
    c.prepare ({ 48000.0, 512 });
    c.setThresholdDb (-20.0f);
    c.setRatio (4.0f);
    c.setKneeDb (0.0f);
    c.setAttackMs (5.0f);
    c.setReleaseMs (100.0f);
    c.setRmsMode (true);

    TestBuffer buf (2, 512);
    int pos = 0;
    for (int block = 0; block < 10; ++block)
    {
        fillSine (buf, 0.5f, 1000.0, 48000.0, pos);
        pos += buf.numFrames;
        auto view = buf.view();
        c.process (view);
    }
    // RMS of a 0.5 sine sits ~3 dB under its peak: less reduction than peak mode,
    // but clearly active and finite.
    const float amp = measureAmplitude (buf);
    CHECK (std::isfinite (amp));
    CHECK (amp < 0.45f);
    CHECK (c.getGainReductionDb() > 0.0f);
    CHECK (c.getGainReductionDb() <= 24.0f);
}

// Automate ratio 1 -> 8 while compressing: smoother + gain ballistics must keep
// every transition finite and free of zipper noise. A 100 Hz carrier keeps the
// signal's own sample-to-sample delta (~0.0065) far below the continuity bound.
static void ratioKneeAutomationIsContinuous()
{
    auralis::CompressorProcessor c;
    c.prepare ({ 48000.0, 256 });
    c.setThresholdDb (-20.0f);
    c.setAttackMs (5.0f);
    c.setReleaseMs (100.0f);
    c.setKneeDb (6.0f);

    TestBuffer buf (2, 256);
    int pos = 0;
    float prev[2] = { 0.0f, 0.0f };
    bool hasPrev = false;
    float maxDiff = 0.0f;
    for (int block = 0; block < 14; ++block)
    {
        c.setRatio (1.0f + 0.5f * float (block + 1)); // 1.5 .. 8, one change per block
        fillSine (buf, 0.5f, 100.0, 48000.0, pos);
        pos += buf.numFrames;
        auto view = buf.view();
        c.process (view);
        for (int ch = 0; ch < buf.numChannels; ++ch)
            for (int i = 0; i < buf.numFrames; ++i)
            {
                const float s = buf.sample (ch, i);
                CHECK (std::isfinite (s) && std::fabs (s) <= 2.0f);
                if (hasPrev)
                    maxDiff = std::max (maxDiff, std::fabs (s - prev[ch]));
                prev[ch] = s;
                hasPrev = true;
            }
    }
    CHECK (maxDiff < 0.02f);
}

// run_DynamicsTests in global namespace
int run_DynamicsTests()
{
    transparentAtDefaults();
    staticCurveMatchesMath();
    makeupAppliesCleanly();
    belowThresholdNoReduction();
    releaseRecoversToUnity();
    grMeterReportsReduction();
    noNaNWithExtremeSettings();
    rmsModeReducesGain();
    ratioKneeAutomationIsContinuous();
    return failures;
}
