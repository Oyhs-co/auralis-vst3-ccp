// SPDX-License-Identifier: AGPL-3.0-only

#include "SpectrumView.h"

#include <cmath>

namespace auralis
{
namespace
{
// Studio-dark palette.
const juce::Colour bgColour    { 0xff0d1117 };
const juce::Colour gridColour  { 0xff30363d };
const juce::Colour textColour  { 0xffc9d1d9 };
const juce::Colour mutedColour { 0xff8b949e };
const juce::Colour tealColour  { 0xff2dd4bf };
const juce::Colour amberColour { 0xffd29922 };
const juce::Colour inputColour { 0xff58a6ff };

constexpr int eqCurvePoints = 64;
} // namespace

SpectrumView::SpectrumView (SpectrumAnalyzer& inputAnalyzer, SpectrumAnalyzer& outputAnalyzer,
                            const std::atomic<float>& correlationRef,
                            const std::atomic<float>& /*compressorGrDb*/,
                            const std::atomic<float>& /*limiterGrDb*/,
                            const auralis::EqualizerProcessor* eqProcessor)
    : analyzerIn (inputAnalyzer), analyzerOut (outputAnalyzer), correlation (correlationRef),
      eq (eqProcessor)
{
    // Gain reduction lives in LevelMeters; the refs stay in the signature so the
    // editor wiring is stable.
    setOpaque (true);
}

void SpectrumView::setEqCurveVisible (bool show)
{
    eqCurveVisible = show;
}

void SpectrumView::paint (juce::Graphics& g)
{
    g.fillAll (bgColour);

    // Top strip for the correlation readout, bottom strip for frequency labels.
    auto topStrip = getLocalBounds().toFloat().removeFromTop (16.0f);
    auto labelStrip = getLocalBounds().toFloat().removeFromBottom (14.0f);
    auto area = getLocalBounds().toFloat().withTrimmedTop (16.0f).withTrimmedBottom (14.0f);
    const float height = area.getHeight();
    const float bottom = area.getBottom();
    const float log20 = std::log10 (20.0f);
    const float logSpan = 4.0f - log20; // 20 Hz..20 kHz

    auto bandX = [&] (int b) -> float
    {
        const float f = 20.0f * std::pow (1000.0f, (float) b / (float) SpectrumAnalyzer::numBands);
        return area.getX() + (std::log10 (f) - log20) / logSpan * area.getWidth();
    };
    auto yForNorm = [&] (float norm) -> float
    {
        return bottom - juce::jlimit (0.0f, 1.0f, norm) * height;
    };

    // Horizontal grid: 0/-12/-24/-36/-48 dB, labels on the right.
    g.setFont (11.0f);
    for (int db = 0; db >= -48; db -= 12)
    {
        const float y = yForNorm ((float) (db + 60) / 60.0f);
        g.setColour (gridColour);
        g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
        g.setColour (mutedColour);
        g.drawText (juce::String (db),
                    juce::Rectangle<float> (area.getRight() - 34.0f, y + 1.0f, 32.0f, 12.0f),
                    juce::Justification::centredRight);
    }

    // Vertical grid: 100 Hz / 1 kHz / 10 kHz.
    for (int hz : { 100, 1000, 10000 })
    {
        const float x = area.getX() + (std::log10 ((float) hz) - log20) / logSpan * area.getWidth();
        g.setColour (gridColour);
        g.drawVerticalLine ((int) x, area.getY(), bottom);
        g.setColour (mutedColour);
        auto label = hz >= 1000 ? juce::String (hz / 1000) + " kHz" : juce::String (hz) + " Hz";
        g.drawText (label,
                    juce::Rectangle<float> (x - 30.0f, labelStrip.getY(), 60.0f, labelStrip.getHeight()),
                    juce::Justification::centred);
    }

    // Input (blue) under output (teal): filled path + thin line, bands are
    // log-spaced 20 Hz..20 kHz and already normalized over a 60 dB window.
    auto drawSpectrum = [&] (const SpectrumAnalyzer& a, juce::Colour fill, juce::Colour line, float lineWidth)
    {
        juce::Path p;
        p.startNewSubPath (bandX (0), yForNorm (a.band[0].load (std::memory_order_relaxed)));
        for (int b = 1; b < SpectrumAnalyzer::numBands; ++b)
            p.lineTo (bandX (b), yForNorm (a.band[b].load (std::memory_order_relaxed)));

        juce::Path fillPath (p);
        fillPath.lineTo (bandX (SpectrumAnalyzer::numBands - 1), bottom);
        fillPath.lineTo (bandX (0), bottom);
        fillPath.closeSubPath();

        g.setColour (fill);
        g.fillPath (fillPath);
        g.setColour (line);
        g.strokePath (p, juce::PathStrokeType (lineWidth));
    };
    drawSpectrum (analyzerIn, juce::Colour (0x5958a6ff), inputColour, 1.0f);
    drawSpectrum (analyzerOut, juce::Colour (0x4d2dd4bf), tealColour, 1.5f);

    // Output peak hold, amber horizontal trace.
    const float holdNorm = (analyzerOut.peakHoldDb.load (std::memory_order_relaxed) + 60.0f) / 60.0f;
    if (holdNorm > 0.0f)
    {
        g.setColour (amberColour);
        g.drawHorizontalLine ((int) yForNorm (holdNorm), area.getX(), area.getRight());
    }

    // EQ curve: same 60 dB pixel range but ±15 dB mapped over it (x2) so a
    // ±15 dB EQ reads clearly.
    if (eqCurveVisible && eq != nullptr)
    {
        double freqs[eqCurvePoints];
        double dbs[eqCurvePoints];
        for (int i = 0; i < eqCurvePoints; ++i)
            freqs[i] = 20.0 * std::pow (1000.0, (double) i / (double) (eqCurvePoints - 1));
        eq->getMagnitudeResponse (freqs, dbs, eqCurvePoints);

        juce::Path eqPath;
        for (int i = 0; i < eqCurvePoints; ++i)
        {
            const float x = area.getX()
                + (float) ((std::log10 (freqs[i]) - (double) log20) / (double) logSpan) * area.getWidth();
            const float y = yForNorm ((float) ((dbs[i] + 15.0) / 30.0));
            i == 0 ? eqPath.startNewSubPath (x, y) : eqPath.lineTo (x, y);
        }

        juce::Path eqFill (eqPath);
        eqFill.lineTo (area.getRight(), bottom);
        eqFill.lineTo (area.getX(), bottom);
        eqFill.closeSubPath();
        g.setColour (juce::Colour (0x18c9d1d9));
        g.fillPath (eqFill);
        g.setColour (textColour);
        g.strokePath (eqPath, juce::PathStrokeType (2.0f));
    }

    // Stereo correlation readout, top-right.
    const float corr = juce::jlimit (-1.0f, 1.0f, correlation.load (std::memory_order_relaxed));
    g.setColour (mutedColour);
    g.setFont (12.0f);
    g.drawText (juce::String::formatted ("Corr %+.2f", corr), topStrip.removeFromRight (90.0f).reduced (6.0f, 0.0f),
                juce::Justification::centredRight);
}
} // namespace auralis
