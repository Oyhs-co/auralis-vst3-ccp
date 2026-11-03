// SPDX-License-Identifier: AGPL-3.0-only
// Spectrum + meter view: reads analyzer atomics, repaints from a Timer. Never touches audio.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "SpectrumAnalyzer.h"

namespace auralis
{
class SpectrumView : public juce::Component
{
public:
    SpectrumView (SpectrumAnalyzer& inputAnalyzer, SpectrumAnalyzer& outputAnalyzer,
                  const std::atomic<float>& correlationRef);

    void paint (juce::Graphics&) override;

private:
    SpectrumAnalyzer& analyzerIn;
    SpectrumAnalyzer& analyzerOut;
    const std::atomic<float>& correlation;

    juce::Colour bandColourIn { 0xff5a6472 };
    juce::Colour bandColourOut { 0xff4fb3a6 };
    juce::Colour peakColour { 0xffe8b84f };
};
}