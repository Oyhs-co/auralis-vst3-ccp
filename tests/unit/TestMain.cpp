// SPDX-License-Identifier: AGPL-3.0-only
// Host-side unit tests for the Auralis DSP pipeline and all effect processors.

#include "DspTypes.h"
#include "IDspProcessor.h"
#include "gain/GainProcessor.h"
#include "DspPipeline.h"

#include <cmath>
#include <cstddef>
#include <iostream>
#include <memory>
#include <vector>

// External test entry points (defined in respective Test*.cpp files)
int run_EqualizerTests();
int run_BassTests();
int run_ClarityTests();
int run_DynamicsTests();
int run_AmbienceTests();
int run_StereoTests();
int run_LimiterTests();
int run_FftTests();
int run_AnalyzerTests();

using auralis::DspPipeline;

static int failures = 0;

#define CHECK(cond) do { if (! (cond)) { ++failures; std::cerr << "CHECK failed: " #cond \
    " at " << __FILE__ << ':' << __LINE__ << '\n'; } } while (false)

namespace
{
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

// step 0 turns the ramp into a constant fill.
void fillRamp (TestBuffer& b, float start, float step)
{
    for (int c = 0; c < b.numChannels; ++c)
        for (int i = 0; i < b.numFrames; ++i)
            b.sample (c, i) = start + step * float (i);
}

void process (DspPipeline& pipeline, TestBuffer& buf)
{
    auto view = buf.view();
    pipeline.process (view);
}

void checkPassthrough (const TestBuffer& b, const std::vector<float>& input, float tolerance)
{
    for (int c = 0; c < b.numChannels; ++c)
        for (int i = 0; i < b.numFrames; ++i)
            CHECK (std::fabs (b.sample (c, i) - input[std::size_t (c) * std::size_t (b.numFrames) + std::size_t (i)]) <= tolerance);
}

void processAndCheckPassthrough (DspPipeline& pipeline, TestBuffer& buf, float tolerance)
{
    const auto input = buf.storage;
    process (pipeline, buf);
    checkPassthrough (buf, input, tolerance);
}

auralis::GainProcessor* addGain (DspPipeline& pipeline, auralis::ProcessContext ctx, float db)
{
    auto* gain = static_cast<auralis::GainProcessor*> (
        pipeline.addProcessor (std::make_unique<auralis::GainProcessor>()));
    pipeline.prepare (ctx);
    gain->setGainDb (db);
    return gain;
}

void emptyPipelineIsPassthrough()
{
    DspPipeline pipeline;
    pipeline.prepare ({ 48000.0, 512 });
    TestBuffer buf (2, 128);
    fillRamp (buf, -0.4f, 0.005f);
    processAndCheckPassthrough (pipeline, buf, 0.0f);
}

void unityGainIsPassthrough()
{
    DspPipeline pipeline;
    auto* gain = addGain (pipeline, { 44100.0, 512 }, 0.0f);
    TestBuffer buf (2, 256);
    fillRamp (buf, -0.3f, 0.002f);
    for (int block = 0; block < 3; ++block) processAndCheckPassthrough (pipeline, buf, 1e-6f);
}

void gain6dBIncreasesAmplitude()
{
    DspPipeline pipeline;
    auto* gain = addGain (pipeline, { 48000.0, 512 }, 6.0f);
    TestBuffer buf (2, 256);
    for (int block = 0; block < 8; ++block)
    {
        fillRamp (buf, 0.25f, 0.0f);
        process (pipeline, buf);
    }
    for (int c = 0; c < buf.numChannels; ++c) // within 2% of 0.5 once settled
        for (int i = 0; i < buf.numFrames; ++i)
            CHECK (std::fabs (buf.sample (c, i) - 0.5f) <= 0.01f);
}

void bypassIsPassthrough()
{
    DspPipeline pipeline;
    auto* gain = addGain (pipeline, { 48000.0, 512 }, 6.0f);
    gain->setBypass (true);
    TestBuffer buf (2, 256);
    fillRamp (buf, 0.5f, -0.004f);
    processAndCheckPassthrough (pipeline, buf, 0.0f);
}

void disabledProcessorIsPassthrough()
{
    DspPipeline pipeline;
    auto* gain = addGain (pipeline, { 48000.0, 512 }, 6.0f);
    pipeline.setEnabled (0, false);
    TestBuffer buf (2, 256);
    fillRamp (buf, -0.2f, 0.003f);
    processAndCheckPassthrough (pipeline, buf, 0.0f);
}

void variableBlockSizes()
{
    DspPipeline pipeline;
    auto* gain = addGain (pipeline, { 48000.0, 1024 }, 6.0f);
    TestBuffer buf (2, 1024);
    const int sizes[] = { 1, 7, 64, 1024, 33 };
    for (int n : sizes)
    {
        fillRamp (buf, 0.5f, 0.0f);
        auralis::AudioBufferView view (buf.chans, 2, n);
        pipeline.process (view);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float s = buf.sample (c, i);
                CHECK (std::isfinite (s) && std::fabs (s) <= 1.0f);
            }
    }
    CHECK (pipeline.size() == 1);
}

void smoothingIsContinuous()
{
    DspPipeline pipeline;
    auto* gain = addGain (pipeline, { 48000.0, 512 }, 12.0f);
    TestBuffer buf (2, 16);
    fillRamp (buf, 0.5f, 0.0f);
    process (pipeline, buf);
    const float out0 = buf.sample (0, 0);
    CHECK (std::isfinite (out0));
    CHECK (out0 > 0.5f);  // smoothing already moving toward ~1.995
    CHECK (out0 < 1.5f);  // no instant jump to the final factor
}

void noNaNWithAlternatingSignal()
{
    DspPipeline pipeline;
    auto* gain = addGain (pipeline, { 96000.0, 512 }, 12.0f);
    TestBuffer buf (2, 128);
    for (int block = 0; block < 4; ++block)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < buf.numFrames; ++i)
                buf.sample (c, i) = (i % 2 == 0) ? 0.5f : -0.5f;
        process (pipeline, buf);
    }
    for (int c = 0; c < buf.numChannels; ++c)
        for (int i = 0; i < buf.numFrames; ++i)
            CHECK (std::isfinite (buf.sample (c, i)));
}

void monoPipeline()
{
    DspPipeline pipeline;
    auto* gain = addGain (pipeline, { 48000.0, 512 }, 0.0f);
    TestBuffer buf (1, 64);
    fillRamp (buf, 0.5f, -0.01f);
    processAndCheckPassthrough (pipeline, buf, 1e-6f);
}

} // namespace

int main()
{
    const struct { const char* name; void (*fn)(); } pipelineTests[] = {
        { "emptyPipelineIsPassthrough", emptyPipelineIsPassthrough },
        { "unityGainIsPassthrough", unityGainIsPassthrough },
        { "gain6dBIncreasesAmplitude", gain6dBIncreasesAmplitude },
        { "bypassIsPassthrough", bypassIsPassthrough },
        { "disabledProcessorIsPassthrough", disabledProcessorIsPassthrough },
        { "variableBlockSizes", variableBlockSizes },
        { "smoothingIsContinuous", smoothingIsContinuous },
        { "noNaNWithAlternatingSignal", noNaNWithAlternatingSignal },
        { "monoPipeline", monoPipeline },
    };

    // Run pipeline tests first
    for (const auto& t : pipelineTests)
    {
        try { t.fn(); }
        catch (const std::exception& e) { ++failures; std::cerr << "EXCEPTION in " << t.name << ": " << e.what() << '\n'; }
        catch (...) { ++failures; std::cerr << "UNKNOWN EXCEPTION in " << t.name << '\n'; }
    }

    // Run all processor tests (each returns its own failure count)
    failures += run_EqualizerTests();
    failures += run_BassTests();
    failures += run_ClarityTests();
    failures += run_DynamicsTests();
    failures += run_AmbienceTests();
    failures += run_StereoTests();
    failures += run_LimiterTests();
    failures += run_FftTests();
    failures += run_AnalyzerTests();

    if (failures == 0)
    {
        std::cout << "ALL TESTS PASSED\n";
        return 0;
    }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}