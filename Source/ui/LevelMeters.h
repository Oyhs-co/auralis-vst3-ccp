// SPDX-License-Identifier: AGPL-3.0-only
// Vertical LED-style meters: input/output RMS with peak caps and compressor /
// limiter gain reduction. Reads analyzer atomics from paint() on the message
// thread. Never touches audio.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "SpectrumAnalyzer.h"

namespace auralis
{
class LevelMeters : public juce::Component
{
public:
    LevelMeters (SpectrumAnalyzer& inputAnalyzer, SpectrumAnalyzer& outputAnalyzer,
                 const std::atomic<float>& compressorGrDb, const std::atomic<float>& limiterGrDb);

    void paint (juce::Graphics&) override;

private:
    SpectrumAnalyzer& analyzerIn;
    SpectrumAnalyzer& analyzerOut;
    const std::atomic<float>& compressorGr; // dB, <= 0 (negative = reduction)
    const std::atomic<float>& limiterGr;
};
}
