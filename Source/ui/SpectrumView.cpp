// SPDX-License-Identifier: AGPL-3.0-only

#include "SpectrumView.h"

#include <cmath>

namespace auralis
{
namespace
{
// dB to 0..1 panel height, -60 dB floor.
float dbToHeight (float db)
{
    return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
}
} // namespace

SpectrumView::SpectrumView (SpectrumAnalyzer& inputAnalyzer, SpectrumAnalyzer& outputAnalyzer,
                            const std::atomic<float>& correlationRef,
                            const std::atomic<float>& compressorGrDb, const std::atomic<float>& limiterGrDb)
    : analyzerIn (inputAnalyzer), analyzerOut (outputAnalyzer), correlation (correlationRef),
      compressorGr (compressorGrDb), limiterGr (limiterGrDb)
{
    setOpaque (true);
}

void SpectrumView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff14181d));

    auto area = getLocalBounds().toFloat().reduced (6.0f);
    const int numBands = SpectrumAnalyzer::numBands;
    const float bandWidth = area.getWidth() / (float) numBands;
    const float meterTop = area.getBottom() - 18.0f;
    auto spectrumArea = area.withBottom (meterTop);

    for (int b = 0; b < numBands; ++b)
    {
        const float x = spectrumArea.getX() + bandWidth * (float) b;
        const float w = bandWidth - 2.0f;

        const float in = juce::jlimit (0.0f, 1.0f, analyzerIn.band[b].load (std::memory_order_relaxed));
        const float out = juce::jlimit (0.0f, 1.0f, analyzerOut.band[b].load (std::memory_order_relaxed));

        g.setColour (bandColourIn);
        g.fillRect (x, spectrumArea.getBottom() - in * spectrumArea.getHeight(), w, in * spectrumArea.getHeight());
        g.setColour (bandColourOut);
        g.fillRect (x + w * 0.25f, spectrumArea.getBottom() - out * spectrumArea.getHeight(),
                    w * 0.75f, out * spectrumArea.getHeight());
    }

    // Peak hold trace over the output bands.
    g.setColour (peakColour);
    const float hold = dbToHeight (analyzerOut.peakHoldDb.load (std::memory_order_relaxed));
    if (hold > 0.0f)
        g.fillRect (spectrumArea.getX(), spectrumArea.getBottom() - hold * spectrumArea.getHeight(),
                    spectrumArea.getWidth(), 1.5f);

    // Metering line: in/out RMS, correlation and gain reduction.
    const float inRms = analyzerIn.rmsDb.load (std::memory_order_relaxed);
    const float outRms = analyzerOut.rmsDb.load (std::memory_order_relaxed);
    const float corr = juce::jlimit (-1.0f, 1.0f, correlation.load (std::memory_order_relaxed));
    const float compGr = compressorGr.load (std::memory_order_relaxed);
    const float limGr = limiterGr.load (std::memory_order_relaxed);

    g.setColour (juce::Colours::grey);
    g.setFont (13.0f);
    auto meterLine = area.withTop (meterTop);
    g.drawText (juce::String::formatted ("In %.1f dB", inRms), meterLine.removeFromLeft (area.getWidth() / 5.0f),
                juce::Justification::centredLeft);
    g.drawText (juce::String::formatted ("Out %.1f dB", outRms), meterLine.removeFromLeft (area.getWidth() / 5.0f),
                juce::Justification::centredLeft);
    g.drawText (juce::String::formatted ("Corr %+.2f", corr), meterLine.removeFromLeft (area.getWidth() / 5.0f),
                juce::Justification::centredLeft);
    g.drawText (juce::String::formatted ("GR c %.1f", compGr), meterLine.removeFromLeft (area.getWidth() / 5.0f),
                juce::Justification::centredLeft);
    g.drawText (juce::String::formatted ("GR l %.1f", limGr), meterLine, juce::Justification::centredLeft);
}
} // namespace auralis