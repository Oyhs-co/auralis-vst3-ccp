// SPDX-License-Identifier: AGPL-3.0-only

#include "LevelMeters.h"

namespace auralis
{
namespace
{
// Studio-dark palette.
const juce::Colour bgColour    { 0xff0d1117 };
const juce::Colour gridColour  { 0xff30363d };
const juce::Colour panelColour { 0xff161b22 };
const juce::Colour textColour  { 0xffc9d1d9 };
const juce::Colour mutedColour { 0xff8b949e };
const juce::Colour tealColour  { 0xff2dd4bf };
const juce::Colour amberColour { 0xffd29922 };
const juce::Colour inputColour { 0xff58a6ff };

constexpr float meterMinDb = -60.0f;
constexpr float meterMaxDb = 0.0f;
constexpr float grMaxDb = 24.0f;   // reduction scale for the GR bars
constexpr float segmentDb = 6.0f;  // LED gap spacing
constexpr float barInset = 5.0f;   // horizontal padding inside each column

// NaN-safe normalized level over the -60..0 dB window.
float levelNorm (float db)
{
    return juce::jlimit (0.0f, 1.0f, (db - meterMinDb) / (meterMaxDb - meterMinDb));
}
} // namespace

LevelMeters::LevelMeters (SpectrumAnalyzer& inputAnalyzer, SpectrumAnalyzer& outputAnalyzer,
                          const std::atomic<float>& compressorGrDb, const std::atomic<float>& limiterGrDb)
    : analyzerIn (inputAnalyzer), analyzerOut (outputAnalyzer),
      compressorGr (compressorGrDb), limiterGr (limiterGrDb)
{
    setOpaque (true);
}

void LevelMeters::paint (juce::Graphics& g)
{
    g.fillAll (bgColour);

    auto valueStrip = getLocalBounds().toFloat().removeFromTop (14.0f);
    auto labelStrip = getLocalBounds().toFloat().removeFromBottom (14.0f);
    auto area = getLocalBounds().toFloat().withTrimmedTop (14.0f).withTrimmedBottom (14.0f);
    const float barTop = area.getY();
    const float barHeight = area.getHeight();
    const float colWidth = area.getWidth() / 4.0f;

    auto levelY = [&] (float db) -> float // -60..0 dB -> y, 0 dB at the top
    {
        return barTop + (1.0f - levelNorm (db)) * barHeight;
    };

    g.setFont (11.0f);

    // Empty tracks behind all four bars.
    g.setColour (panelColour);
    for (int c = 0; c < 4; ++c)
        g.fillRect (area.getX() + colWidth * (float) c + barInset, barTop,
                    colWidth - 2.0f * barInset, barHeight);

    // Scale ticks at -12/-24/-48 across the two level columns.
    g.setColour (gridColour);
    for (float tickDb : { -12.0f, -24.0f, -48.0f })
        g.drawHorizontalLine ((int) levelY (tickDb),
                              area.getX() + barInset, area.getX() + colWidth * 2.0f - barInset);

    auto drawLevelBar = [&] (int col, const SpectrumAnalyzer& a, bool usePeakHold, juce::Colour colour)
    {
        const float x = area.getX() + colWidth * (float) col + barInset;
        const float w = colWidth - 2.0f * barInset;
        const float norm = levelNorm (a.rmsDb.load (std::memory_order_relaxed));
        const float fillTop = barTop + (1.0f - norm) * barHeight;

        g.setColour (colour);
        g.fillRect (x, fillTop, w, norm * barHeight);

        // LED segmentation: darker gaps every 6 dB inside the lit region.
        g.setColour (bgColour);
        for (float edge = meterMinDb + segmentDb; edge < meterMaxDb; edge += segmentDb)
        {
            const float y = levelY (edge);
            if (y > fillTop)
                g.fillRect (x, y - 1.0f, w, 2.0f);
        }

        const float peakDb = usePeakHold ? a.peakHoldDb.load (std::memory_order_relaxed)
                                         : a.peakDb.load (std::memory_order_relaxed);
        const float peakNorm = levelNorm (peakDb);
        if (peakNorm > 0.0f)
        {
            g.setColour (colour);
            g.fillRect (x, barTop + (1.0f - peakNorm) * barHeight, w, 2.0f);
        }
    };
    drawLevelBar (0, analyzerIn, false, inputColour);
    drawLevelBar (1, analyzerOut, true, tealColour);

    // GR bars grow downward from the top: 0 dB reduction at the top.
    auto drawGrBar = [&] (int col, const std::atomic<float>& gr)
    {
        const float x = area.getX() + colWidth * (float) col + barInset;
        const float w = colWidth - 2.0f * barInset;
        // The DSP publishes negative dB; the bar shows the reduction magnitude.
        const float reduction = juce::jlimit (0.0f, grMaxDb, -gr.load (std::memory_order_relaxed));
        const float grHeight = reduction / grMaxDb * barHeight;

        g.setColour (amberColour);
        g.fillRect (x, barTop, w, grHeight);

        g.setColour (bgColour);
        for (float edge = segmentDb; edge < grMaxDb; edge += segmentDb)
        {
            const float y = barTop + edge / grMaxDb * barHeight;
            if (y < barTop + grHeight)
                g.fillRect (x, y - 1.0f, w, 2.0f);
        }
    };
    drawGrBar (2, compressorGr);
    drawGrBar (3, limiterGr);

    // Values above, labels below.
    const float values[4] = { analyzerIn.rmsDb.load (std::memory_order_relaxed),
                              analyzerOut.rmsDb.load (std::memory_order_relaxed),
                              compressorGr.load (std::memory_order_relaxed),
                              limiterGr.load (std::memory_order_relaxed) };
    const char* labels[4] = { "IN", "OUT", "GR c", "GR l" };
    for (int c = 0; c < 4; ++c)
    {
        const auto column = area.withX (area.getX() + colWidth * (float) c).withWidth (colWidth);
        g.setColour (textColour);
        g.drawText (juce::String (values[c], 1), valueStrip.withX (column.getX()).withWidth (column.getWidth()),
                    juce::Justification::centred);
        g.setColour (mutedColour);
        g.drawText (labels[c], labelStrip.withX (column.getX()).withWidth (column.getWidth()),
                    juce::Justification::centred);
    }
}
} // namespace auralis
