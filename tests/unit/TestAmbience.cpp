// SPDX-License-Identifier: AGPL-3.0-only
// Unit tests for the slice 7 ambience processor (AmbienceProcessor).

#include "ambience/AmbienceProcessor.h"
#include "DspTypes.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <vector>

static int failures = 0;

#define CHECK(cond) do { if (! (cond)) { ++failures; std::cerr << "CHECK failed: " #cond \
    " at " << __FILE__ << ":" << __LINE__ << "\n"; } } while (false)

using auralis::AmbienceProcessor;

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

static float blockEnergy (const TestBuffer& b)
{
    float e = 0.0f;
    for (int c = 0; c < b.numChannels; ++c)
        for (int i = 0; i < b.numFrames; ++i)
            e += b.sample (c, i) * b.sample (c, i);
    return e;
}

// (unused runBlocks helper removed — dead code, AGENTS §4)

static void zeroMixIsExactPassthrough()
{
    AmbienceProcessor a;
    a.prepare ({ 48000.0, 512 });
    a.setMix (0.0f);
    TestBuffer buf (2, 256);
    for (int i = 0; i < buf.numFrames; ++i)
    {
        buf.sample (0, i) = -0.4f + 0.004f * float (i);
        buf.sample (1, i) = 0.3f - 0.003f * float (i);
    }
    const auto input = buf.storage;
    for (int block = 0; block < 2; ++block)
    {
        auto view = buf.view();
        a.process (view);
    }
    for (std::size_t n = 0; n < input.size(); ++n)
        CHECK (std::fabs (buf.storage[n] - input[n]) <= 1e-6f);
}

static void wetTailExistsAndDecays()
{
    AmbienceProcessor a;
    a.prepare ({ 48000.0, 512 });
    a.setMix (1.0f);
    a.setDecaySeconds (1.5f);
    a.setDamping (0.3f);
    TestBuffer buf (2, 512);
    // Impulse block
    std::fill (buf.storage.begin(), buf.storage.end(), 0.0f);
    buf.sample (0, 0) = 1.0f;
    buf.sample (1, 0) = 1.0f;
    auto view = buf.view();
    a.process (view);
    // 40 more blocks of silence
    std::fill (buf.storage.begin(), buf.storage.end(), 0.0f);
    std::vector<float> energies;
    for (int block = 1; block <= 40; ++block)
    {
        std::fill (buf.storage.begin(), buf.storage.end(), 0.0f);
        view = buf.view();
        a.process (view);
        energies.push_back (blockEnergy (buf));
    }
    // Tail alive at block 30
    CHECK (energies[29] > 0.0f);
    // Decaying by block 35
    CHECK (energies[34] < energies[9]);
    // All outputs bounded
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < buf.numFrames; ++i)
        {
            const float s = buf.sample (c, i);
            CHECK (std::isfinite (s) && std::fabs (s) <= 2.0f);
        }
}

static void decaySecondsControlsTail()
{
    AmbienceProcessor a1, a2;
    a1.prepare ({ 48000.0, 512 });
    a1.setMix (1.0f);
    a1.setDecaySeconds (0.3f);
    a1.setDamping (0.3f);
    a2.prepare ({ 48000.0, 512 });
    a2.setMix (1.0f);
    a2.setDecaySeconds (3.0f);
    a2.setDamping (0.3f);
    TestBuffer buf1 (2, 512), buf2 (2, 512);
    std::fill (buf1.storage.begin(), buf1.storage.end(), 0.0f);
    buf1.sample (0, 0) = 1.0f;
    buf1.sample (1, 0) = 1.0f;
    std::fill (buf2.storage.begin(), buf2.storage.end(), 0.0f);
    buf2.sample (0, 0) = 1.0f;
    buf2.sample (1, 0) = 1.0f;
    auto view1 = buf1.view();
    a1.process (view1);
    auto view2 = buf2.view();
    a2.process (view2);
    std::fill (buf1.storage.begin(), buf1.storage.end(), 0.0f);
    std::fill (buf2.storage.begin(), buf2.storage.end(), 0.0f);
    for (int block = 1; block <= 25; ++block)
    {
        view1 = buf1.view();
        a1.process (view1);
        view2 = buf2.view();
        a2.process (view2);
    }
    CHECK (blockEnergy (buf1) < blockEnergy (buf2));
}

static void dampingDarkens()
{
    AmbienceProcessor a1, a2;
    a1.prepare ({ 48000.0, 512 });
    a1.setMix (1.0f);
    a1.setDecaySeconds (2.0f);
    a1.setDamping (0.9f);
    a2.prepare ({ 48000.0, 512 });
    a2.setMix (1.0f);
    a2.setDecaySeconds (2.0f);
    a2.setDamping (0.1f);
    TestBuffer buf1 (2, 512), buf2 (2, 512);
    std::fill (buf1.storage.begin(), buf1.storage.end(), 0.0f);
    buf1.sample (0, 0) = 1.0f;
    buf1.sample (1, 0) = 1.0f;
    std::fill (buf2.storage.begin(), buf2.storage.end(), 0.0f);
    buf2.sample (0, 0) = 1.0f;
    buf2.sample (1, 0) = 1.0f;
    auto view1 = buf1.view();
    a1.process (view1);
    auto view2 = buf2.view();
    a2.process (view2);
    std::fill (buf1.storage.begin(), buf1.storage.end(), 0.0f);
    std::fill (buf2.storage.begin(), buf2.storage.end(), 0.0f);
    std::vector<float> e1, e2;
    for (int block = 1; block <= 40; ++block)
    {
        view1 = buf1.view();
        a1.process (view1);
        view2 = buf2.view();
        a2.process (view2);
        e1.push_back (blockEnergy (buf1));
        e2.push_back (blockEnergy (buf2));
    }
    // Second half of window: high damping tail energy < low damping
    float sum1 = 0.0f, sum2 = 0.0f;
    for (int k = 20; k < 40; ++k)
    {
        sum1 += e1[k];
        sum2 += e2[k];
    }
    CHECK (sum1 < sum2);
}

static void predelayOffsetsWet()
{
    AmbienceProcessor a;
    a.prepare ({ 48000.0, 512 });
    a.setMix (1.0f);
    a.setPredelayMs (100.0f); // 4800 samples at 48k
    TestBuffer buf (2, 512);
    std::fill (buf.storage.begin(), buf.storage.end(), 0.0f);
    buf.sample (0, 0) = 1.0f;
    buf.sample (1, 0) = 1.0f;
    auto view = buf.view();
    a.process (view);
    // The impulse and eight silent blocks total 4608 samples, before the 4800-sample delay.
    for (int block = 0; block < 8; ++block)
    {
        std::fill (buf.storage.begin(), buf.storage.end(), 0.0f);
        view = buf.view();
        a.process (view);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < buf.numFrames; ++i)
                CHECK (std::fabs (buf.sample (c, i)) < 1e-9f);
    }
    // The predelay is followed by the shortest room-line delay.
    for (int block = 0; block < 4; ++block)
    {
        std::fill (buf.storage.begin(), buf.storage.end(), 0.0f);
        view = buf.view();
        a.process (view);
    }
    bool nonZero = false;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < buf.numFrames; ++i)
            if (std::fabs (buf.sample (c, i)) > 1e-9f)
                nonZero = true;
    CHECK (nonZero);
}

static void stabilityNoNaN()
{
    AmbienceProcessor a;
    a.prepare ({ 48000.0, 512 });
    a.setMix (1.0f);
    a.setDecaySeconds (10.0f);
    a.setDamping (0.0f);
    TestBuffer buf (2, 512);
    unsigned long long seed = 12345;
    for (int block = 0; block < 16; ++block)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < buf.numFrames; ++i)
            {
                seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
                buf.sample (c, i) = 2.0f * float ((seed >> 40) & 0xFFFFFFULL) / 16777216.0f - 1.0f;
            }
        auto view = buf.view();
        a.process (view);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < buf.numFrames; ++i)
            {
                const float s = buf.sample (c, i);
                CHECK (std::isfinite (s) && std::fabs (s) <= 2.0f);
            }
    }
}

static void widthAndDampingAutomationIsContinuous()
{
    AmbienceProcessor a;
    a.prepare ({ 48000.0, 512 });
    a.setMix (1.0f);
    a.setDecaySeconds (2.0f);
    TestBuffer buf (2, 512);
    float prev[2] = { 0.0f, 0.0f };
    bool hasPrev = false;
    for (int block = 0; block < 10; ++block)
    {
        const float t = float (block) / 9.0f; // 0..1 across 10 blocks
        a.setWidth (1.0f - t);                // width 1 -> 0
        a.setDamping (0.1f + 0.8f * t);       // damping 0.1 -> 0.9
        for (int c = 0; c < buf.numChannels; ++c)
            for (int i = 0; i < buf.numFrames; ++i)
                buf.sample (c, i) = 0.1f; // continuous quiet signal
        auto view = buf.view();
        a.process (view);
        for (int c = 0; c < buf.numChannels; ++c)
            for (int i = 0; i < buf.numFrames; ++i)
            {
                const float s = buf.sample (c, i);
                CHECK (std::isfinite (s));
                if (hasPrev)
                    CHECK (std::fabs (s - prev[c]) < 0.03f); // reverb changes are slow by design
                prev[c] = s;
            }
        hasPrev = true;
    }
}

// run_AmbienceTests in global namespace
int run_AmbienceTests()
{
    zeroMixIsExactPassthrough();
    wetTailExistsAndDecays();
    decaySecondsControlsTail();
    dampingDarkens();
    predelayOffsetsWet();
    stabilityNoNaN();
    widthAndDampingAutomationIsContinuous();
    return failures;
}
