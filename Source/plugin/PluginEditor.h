// SPDX-License-Identifier: AGPL-3.0-only
// Auralis editor: top bar (presets, A/B, intensity, bypass), spectrum view, parameter grid.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "../ui/SpectrumView.h"

class AuralisAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AuralisAudioProcessorEditor (AuralisAudioProcessor& processor);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void buildParamGrid();
    void refreshAbLabel();

    AuralisAudioProcessor& processor;

    juce::ComboBox presetBox;
    juce::TextButton saveBtn { "Save" }, loadBtn { "Load" }, abBtn { "A" }, resetBtn { "Reset" };
    juce::Slider intensitySlider;
    juce::Label intensityLabel { {}, "Intensity" };
    juce::AudioProcessorValueTreeState::SliderAttachment intensityAttachment;
    juce::ToggleButton bypassBtn { "Bypass" };
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment;

    auralis::SpectrumView spectrum;

    juce::Viewport paramViewport;
    juce::Component paramGrid;

    struct Cell
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    struct ToggleCell
    {
        std::unique_ptr<juce::ToggleButton> button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };
    std::vector<Cell> cells;
    std::vector<ToggleCell> toggleCells;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuralisAudioProcessorEditor)
};