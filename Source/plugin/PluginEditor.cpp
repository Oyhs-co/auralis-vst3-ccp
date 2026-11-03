// SPDX-License-Identifier: AGPL-3.0-only

#include "PluginEditor.h"

namespace
{
constexpr int topBarHeight = 44;
constexpr int spectrumMinHeight = 140;
constexpr int cellWidth = 108;
constexpr int cellHeight = 92;
constexpr int gridColumns = 6;

bool isTopBarParam (const juce::String& id)
{
    return id == "gain" || id == "intensity" || id == "bypass";
}
} // namespace

AuralisAudioProcessorEditor::AuralisAudioProcessorEditor (AuralisAudioProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      intensityAttachment (p.apvts, "intensity", intensitySlider),
      bypassAttachment (p.apvts, "bypass", bypassBtn),
      spectrum (p.getInputAnalyzer(), p.getOutputAnalyzer(), p.getStereoCorrelationAtomic())
{
    for (auto& factory : auralis::PresetManager::factoryPresets())
        presetBox.addItem (factory.name, presetBox.getNumItems() + 1);
    presetBox.setTextWhenNothingSelected ("Preset");
    presetBox.onChange = [this]
    { processor.applyPreset (auralis::PresetManager::factoryPresets()[(int) presetBox.getSelectedId() - 1]); };

    saveBtn.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Save preset", juce::File(), "*.auralis");
        fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file != juce::File())
                    processor.saveUserPreset (file.withFileExtension ("auralis"), file.getFileNameWithoutExtension());
            });
    };

    loadBtn.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Load preset", juce::File(), "*.auralis");
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                if (fc.getResult() != juce::File())
                    processor.loadUserPreset (fc.getResult());
            });
    };

    resetBtn.onClick = [this] { processor.resetToDefaults(); };
    abBtn.onClick = [this]
    {
        processor.toggleAb();
        refreshAbLabel();
    };
    refreshAbLabel();

    addAndMakeVisible (presetBox);
    addAndMakeVisible (saveBtn);
    addAndMakeVisible (loadBtn);
    addAndMakeVisible (abBtn);
    addAndMakeVisible (resetBtn);
    addAndMakeVisible (intensitySlider);
    addAndMakeVisible (intensityLabel);
    addAndMakeVisible (bypassBtn);
    addAndMakeVisible (spectrum);
    addAndMakeVisible (paramViewport);
    paramViewport.setViewedComponent (&paramGrid, false);

    intensitySlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 20);
    intensitySlider.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff2a3138));

    buildParamGrid();
    startTimerHz (30);
    setResizable (true, false);
    setResizeLimits (760, 540, 1600, 1000);
    setSize (900, 640);
}

void AuralisAudioProcessorEditor::buildParamGrid()
{
    int x = 0, y = 0;
    for (auto* param : processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
        if (ranged == nullptr)
            continue;
        const juce::String id = ranged->getParameterID();
        if (isTopBarParam (id))
            continue;

        if (auto* boolParam = dynamic_cast<juce::AudioParameterBool*> (ranged))
        {
            ToggleCell cell;
            cell.button = std::make_unique<juce::ToggleButton> (boolParam->getName (64));
            cell.button->setBounds (x * cellWidth, y * cellHeight + cellHeight / 2, cellWidth - 8, 24);
            cell.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                processor.apvts, id, *cell.button);
            paramGrid.addAndMakeVisible (cell.button.get());
            toggleCells.push_back (std::move (cell));
        }
        else
        {
            Cell cell;
            cell.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag,
                juce::Slider::TextBoxBelow);
            cell.slider->setBounds (x * cellWidth, y * cellHeight, cellWidth - 8, cellHeight - 22);
            cell.label = std::make_unique<juce::Label> (juce::String(), ranged->getName (64));
            cell.label->setBounds (x * cellWidth, y * cellHeight + cellHeight - 22, cellWidth - 8, 20);
            cell.label->setJustificationType (juce::Justification::centred);
            cell.label->setFont (juce::FontOptions (11.0f));
            cell.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                processor.apvts, id, *cell.slider);
            paramGrid.addAndMakeVisible (cell.slider.get());
            paramGrid.addAndMakeVisible (cell.label.get());
            cells.push_back (std::move (cell));
        }

        if (++x == gridColumns)
        {
            x = 0;
            ++y;
        }
    }
    paramGrid.setSize (gridColumns * cellWidth, (y + (x > 0 ? 1 : 0)) * cellHeight);
}

void AuralisAudioProcessorEditor::refreshAbLabel()
{
    abBtn.setButtonText (processor.getAbLabel());
}

void AuralisAudioProcessorEditor::timerCallback()
{
    spectrum.repaint();
}

void AuralisAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b2026));
}

void AuralisAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    auto top = area.removeFromTop (topBarHeight).reduced (6, 6);
    presetBox.setBounds (top.removeFromLeft (150));
    saveBtn.setBounds (top.removeFromLeft (64));
    loadBtn.setBounds (top.removeFromLeft (64));
    resetBtn.setBounds (top.removeFromLeft (64));
    abBtn.setBounds (top.removeFromLeft (40));
    bypassBtn.setBounds (top.removeFromRight (80));
    auto intensityArea = top.removeFromRight (220);
    intensityLabel.setBounds (intensityArea.removeFromLeft (60));
    intensitySlider.setBounds (intensityArea);

    auto spectrumArea = area.removeFromTop (juce::jmax (spectrumMinHeight, proportionOfHeight (0.30f)));
    spectrum.setBounds (spectrumArea.reduced (4));

    paramViewport.setBounds (area.reduced (4));
}