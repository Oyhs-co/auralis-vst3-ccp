// SPDX-License-Identifier: AGPL-3.0-only
// Spectrum + EQ curve view: reads analyzer atomics and the EQ magnitude response
// from paint() on the message thread. Never touches audio.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "SpectrumAnalyzer.h"
#include "equalizer/EqualizerProcessor.h"

namespace auralis
{
class SpectrumView : public juce::Component
{
public:
    SpectrumView (SpectrumAnalyzer& inputAnalyzer, SpectrumAnalyzer& outputAnalyzer,
                  const std::atomic<float>& correlationRef,
                  const std::atomic<float>& compressorGrDb, const std::atomic<float>& limiterGrDb,
                  const auralis::EqualizerProcessor* eqProcessor); // null = sin curva EQ

    void setEqCurveVisible (bool show);   // default true
    void paint (juce::Graphics&) override;

private:
    SpectrumAnalyzer& analyzerIn;
    SpectrumAnalyzer& analyzerOut;
    const std::atomic<float>& correlation;
    const auralis::EqualizerProcessor* eq;
    bool eqCurveVisible = true;
};
}
