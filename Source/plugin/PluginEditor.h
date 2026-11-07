// SPDX-License-Identifier: AGPL-3.0-only
// Auralis editor: studio-dark console — top bar, spectrum + meters, macro/module panels.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "../ui/SpectrumView.h"
#include "../ui/LevelMeters.h"

class AuralisAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AuralisAudioProcessorEditor (AuralisAudioProcessor& processor);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void browseForPreset (bool saving);

    struct Cell
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    struct HeaderToggle
    {
        std::unique_ptr<juce::ToggleButton> button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };
    // One rounded panel per module: title + enable in the header, knob grid below.
    struct ModulePanel : juce::Component
    {
        juce::Label title;
        juce::ToggleButton enableBtn;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttachment;
        std::vector<HeaderToggle> headerToggles;
        std::vector<Cell> cells;

        void paint (juce::Graphics&) override;
    };

    void buildModules();
    void relayoutModules();
    int layoutPanel (ModulePanel& panel, int width, int cellW, int ch);
    Cell makeKnob (const juce::String& id, juce::Component& parent);
    HeaderToggle makeHeaderToggle (const juce::String& id, const juce::String& label, juce::Component& parent);
    void takeBaseline();
    bool baselineMatches() const;

    AuralisAudioProcessor& processor;

    juce::ComboBox presetBox;
    juce::TextButton saveBtn { "Save" }, loadBtn { "Load" }, abBtn { "A" }, resetBtn { "Reset" }, viewBtn { "Simple" };
    juce::Slider intensitySlider;
    juce::Label intensityLabel { {}, "Intensity" };
    juce::AudioProcessorValueTreeState::SliderAttachment intensityAttachment;
    juce::ToggleButton bypassBtn { "Bypass" };
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment;

    auralis::SpectrumView spectrum;
    auralis::LevelMeters meters;

    juce::Viewport paramViewport;
    juce::Component paramHost;
    ModulePanel macrosPanel;
    std::vector<std::unique_ptr<ModulePanel>> modules;

    bool advancedView = false;
    bool presetModified = false;
    int tickCount = 0;
    juce::ValueTree baselineSnapshot;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuralisAudioProcessorEditor)
};
