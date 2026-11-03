// SPDX-License-Identifier: AGPL-3.0-only
// Self-contained checks for auralis::SpectrumAnalyzer; run_AnalyzerTests() returns the failure count.

#include "SpectrumAnalyzer.h"
#include "DspTypes.h"

#include <cmath>
#include <cstddef>
#include <iostream>
#include <vector>

using auralis::SpectrumAnalyzer;

static int failures = 0;

#define CHECK(cond) do { if (! (cond)) { ++failures; std::cerr << "CHECK failed: " #cond \
    " at " << __FILE__ << ':' << __LINE__ << '\n'; } } while (false)

constexpr double kPi = 3.14159265358979323846;

// Owns sample storage, hands out views to the analyzer and keeps a pristine
// copy of the input so tests can prove the buffer was never touched.
class ObservedBuffer
{
public:
    ObservedBuffer (int channels, int frames)
        : numCh (channels), nf (frames),
          storage ((std::size_t) channels * (std::size_t) frames)
    {
        for (int c = 0; c < numCh; ++c)
            chans[c] = storage.data() + (std::size_t) c * (std::size_t) nf;
    }

    auralis::AudioBufferView view() { return { chans, numCh, nf }; }

    void fillRamp (float start, float step)
    {
        for (int c = 0; c < numCh; ++c)
            for (int i = 0; i < nf; ++i)
                sample (c, i) = start + step * (float) i;
        pristine = storage;
    }

    void fillSine (double freq, double amplitude, double sampleRate, int startFrame)
    {
        for (int c = 0; c < numCh; ++c)
            for (int i = 0; i < nf; ++i)
                sample (c, i) = (float) (amplitude
                    * std::sin (2.0 * kPi * freq * (double) (startFrame + i) / sampleRate));
        pristine = storage;
    }

    void checkUnmodified()
    {
        for (std::size_t i = 0; i < storage.size(); ++i)
            CHECK (storage[i] == pristine[i]);
    }

private:
    float& sample (int c, int i) { return storage[(std::size_t) c * (std::size_t) nf + (std::size_t) i]; }

    int numCh = 0;
    int nf = 0;
    std::vector<float> storage;
    std::vector<float> pristine;
    float* chans[2] { nullptr, nullptr };
};

static void neverModifiesBuffer()
{
    SpectrumAnalyzer analyzer;
    analyzer.prepare (48000.0, 512);
    ObservedBuffer buf (2, 512);
    buf.fillRamp (-0.4f, 0.005f);
    for (int block = 0; block < 3; ++block)
        analyzer.process (buf.view());
    buf.checkUnmodified();
}

static void rmsOfKnownSine()
{
    SpectrumAnalyzer analyzer;
    analyzer.prepare (48000.0, 512);
    ObservedBuffer buf (1, 512);
    buf.fillSine (1000.0, 0.7071, 48000.0, 0);
    analyzer.process (buf.view());
    buf.checkUnmodified();

    // RMS of a sine is A/sqrt(2) = 0.5, i.e. -6.02 dB.
    const double expectedDb = 20.0 * std::log10 (0.5);
    CHECK (std::fabs (analyzer.rmsDb.load() - expectedDb) <= 0.5);
}

static void peakHoldWorks()
{
    SpectrumAnalyzer analyzer;
    analyzer.prepare (48000.0, 512);
    ObservedBuffer buf (1, 512);

    buf.fillRamp (0.9f, 0.0f);
    analyzer.process (buf.view());
    CHECK (std::fabs (analyzer.peakHoldDb.load() - 20.0 * std::log10 (0.9)) <= 0.5);

    buf.fillRamp (0.0f, 0.0f);
    for (int block = 0; block < 20; ++block)
        analyzer.process (buf.view());

    // 20 silent blocks decay the hold by only 0.1 linear (0.005 per block).
    CHECK (analyzer.peakHoldDb.load() > 20.0 * std::log10 (0.9) - 20.0);
}

static void sineEnergyInRightBand()
{
    SpectrumAnalyzer analyzer;
    analyzer.prepare (48000.0, 1024);
    ObservedBuffer buf (2, 1024);
    for (int block = 0; block < 4; ++block)
    {
        buf.fillSine (1000.0, 0.5, 48000.0, block * 1024);
        analyzer.process (buf.view());
    }

    int maxBand = 0;
    for (int b = 1; b < SpectrumAnalyzer::numBands; ++b)
        if (analyzer.band[b].load() > analyzer.band[maxBand].load())
            maxBand = b;

    CHECK (analyzer.band[maxBand].load() > 0.3f);
    int hotBands = 0;
    for (int b = 0; b < SpectrumAnalyzer::numBands; ++b)
        if (analyzer.band[b].load() > 0.3f)
            ++hotBands;
    CHECK (hotBands == 1);

    for (int b = 0; b < SpectrumAnalyzer::numBands; ++b)
        if (b < maxBand - 4 || b > maxBand + 4)
            CHECK (analyzer.band[b].load() < 0.05f);
}

static void noNaN()
{
    SpectrumAnalyzer analyzer;
    analyzer.prepare (48000.0, 512);
    ObservedBuffer buf (2, 512);
    buf.fillRamp (0.0f, 0.0f);
    for (int block = 0; block < 4; ++block)
        analyzer.process (buf.view());

    CHECK (analyzer.rmsDb.load() == -100.0f);
    CHECK (analyzer.peakDb.load() == -100.0f);
    CHECK (analyzer.peakHoldDb.load() == -100.0f);
    CHECK (std::isfinite (analyzer.rmsDb.load()));
    CHECK (std::isfinite (analyzer.peakDb.load()));
    CHECK (std::isfinite (analyzer.peakHoldDb.load()));
    for (int b = 0; b < SpectrumAnalyzer::numBands; ++b)
    {
        const float v = analyzer.band[b].load();
        CHECK (std::isfinite (v));
        CHECK (v < 0.001f);
    }
}

// run_AnalyzerTests in global namespace
int run_AnalyzerTests()
{
    neverModifiesBuffer();
    rmsOfKnownSine();
    peakHoldWorks();
    sineEnergyInRightBand();
    noNaN();
    return failures;
}
