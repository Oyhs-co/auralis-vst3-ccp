// SPDX-License-Identifier: AGPL-3.0-only

#include "PluginEditor.h"

#include <cmath>
#include <cstring>

namespace
{
constexpr int topBarH = 48;
constexpr int metersW = 96;
constexpr int headerH = 22;
constexpr int cellH = 72;
constexpr int macroCell = 110;
constexpr int gridCols = 4;

// STUDIO DARK palette — do not drift from these literals.
const juce::Colour bgCol     { 0xff0d1117 };
const juce::Colour panelCol  { 0xff161b22 };
const juce::Colour borderCol { 0xff30363d };
const juce::Colour textCol   { 0xffc9d1d9 };
const juce::Colour mutedCol  { 0xff8b949e };
const juce::Colour accentCol { 0xff2dd4bf };
const juce::Colour amberCol  { 0xffd29922 };
const juce::Colour btnCol    { 0xff21262d };

bool isTopBarParam (const juce::String& id)
{
    return id == "gain" || id == "intensity" || id == "bypass";
}

struct ModuleDef { const char* prefix; const char* title; const char* enableId; };
constexpr ModuleDef moduleDefs[] =
{
    { "eq_", "Equalizer", "eq_en" }, { "bass_", "Bass", "bass_en" }, { "clarity_", "Clarity", "clarity_en" },
    { "comp_", "Dynamics", "comp_en" }, { "amb_", "Ambience", "amb_en" },
    { "stereo_", "Stereo", "stereo_en" }, { "lim_", "Limiter", nullptr },
};

const ModuleDef* moduleDefFor (const juce::String& id)
{
    for (auto& def : moduleDefs)
        if (id.startsWith (def.prefix))
            return &def;
    return nullptr;
}

juce::String shortToggleLabel (const juce::String& id)
{
    if (id == "eq_low_en") return "L";
    if (id == "eq_band1_en") return "1";
    if (id == "eq_band2_en") return "2";
    if (id == "eq_high_en") return "H";
    return "RMS"; // comp_rms is the only other header bool in the layout
}

juce::String prettyLabel (const juce::String& id, const ModuleDef& def)
{
    juce::StringArray words;
    words.addTokens (id.substring ((int) std::strlen (def.prefix)), "_", "");
    juce::String out;
    for (auto& w : words)
        out << w.substring (0, 1).toUpperCase() << w.substring (1) << " ";
    return out.trim();
}

void styleButton (juce::TextButton& b)
{
    b.setColour (juce::TextButton::buttonColourId, btnCol);
    b.setColour (juce::TextButton::buttonOnColourId, btnCol);
    b.setColour (juce::TextButton::textColourOffId, textCol);
    b.setColour (juce::TextButton::textColourOnId, accentCol);
}

void styleToggle (juce::ToggleButton& b)
{
    b.setColour (juce::ToggleButton::textColourId, mutedCol);
    b.setColour (juce::ToggleButton::tickColourId, accentCol);
    b.setColour (juce::ToggleButton::tickDisabledColourId, borderCol);
}

void styleKnob (juce::Slider& s)
{
    s.setColour (juce::Slider::trackColourId, btnCol);
    s.setColour (juce::Slider::rotarySliderFillColourId, accentCol);
    s.setColour (juce::Slider::thumbColourId, textCol);
    s.setColour (juce::Slider::textBoxTextColourId, textCol);
    s.setColour (juce::Slider::textBoxBackgroundColourId, panelCol);
    s.setColour (juce::Slider::textBoxOutlineColourId, borderCol);
}

void setupRotary (juce::Slider& s, int textBoxW, int textBoxH)
{
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
    s.setMouseDragSensitivity (180);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    styleKnob (s);
}

void styleModuleTitle (juce::Label& title, const juce::String& text, juce::Component& parent)
{
    title.setText (text, juce::dontSendNotification);
    title.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, textCol);
    parent.addAndMakeVisible (title);
}
} // namespace

AuralisAudioProcessorEditor::AuralisAudioProcessorEditor (AuralisAudioProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      intensityAttachment (p.apvts, "intensity", intensitySlider),
      bypassAttachment (p.apvts, "bypass", bypassBtn),
      spectrum (p.getInputAnalyzer(), p.getOutputAnalyzer(), p.getStereoCorrelationAtomic(),
                p.getCompressorGrAtomic(), p.getLimiterGrAtomic(), p.getEqualizer()),
      meters (p.getInputAnalyzer(), p.getOutputAnalyzer(),
              p.getCompressorGrAtomic(), p.getLimiterGrAtomic())
{
    for (auto& factory : auralis::PresetManager::factoryPresets())
        presetBox.addItem (factory.name, presetBox.getNumItems() + 1);
    presetBox.setTextWhenNothingSelected ("Preset");
    presetBox.setColour (juce::ComboBox::backgroundColourId, btnCol);
    presetBox.setColour (juce::ComboBox::textColourId, textCol);
    presetBox.setColour (juce::ComboBox::outlineColourId, borderCol);
    presetBox.setColour (juce::ComboBox::arrowColourId, mutedCol);
    presetBox.onChange = [this]
    {
        processor.applyPreset (auralis::PresetManager::factoryPresets()[(int) presetBox.getSelectedId() - 1]);
        takeBaseline();
    };

    saveBtn.onClick = [this] { browseForPreset (true); };
    loadBtn.onClick = [this] { browseForPreset (false); };
    resetBtn.onClick = [this]
    {
        processor.resetToDefaults();
        takeBaseline();
    };
    abBtn.onClick = [this]
    {
        processor.toggleAb();
        abBtn.setButtonText (processor.getAbLabel());
    };
    abBtn.setButtonText (processor.getAbLabel());

    viewBtn.setClickingTogglesState (true);
    viewBtn.onClick = [this]
    {
        advancedView = viewBtn.getToggleState();
        viewBtn.setButtonText (advancedView ? "Advanced" : "Simple");
        relayoutModules();
    };

    setupRotary (intensitySlider, 46, 14);
    intensityLabel.setFont (juce::FontOptions (11.0f));
    intensityLabel.setColour (juce::Label::textColourId, mutedCol);
    intensityLabel.setJustificationType (juce::Justification::centredRight);
    styleToggle (bypassBtn);

    for (auto* btn : { &saveBtn, &loadBtn, &resetBtn, &abBtn, &viewBtn })
        styleButton (*btn);

    addAndMakeVisible (presetBox);
    addAndMakeVisible (saveBtn);
    addAndMakeVisible (loadBtn);
    addAndMakeVisible (resetBtn);
    addAndMakeVisible (abBtn);
    addAndMakeVisible (viewBtn);
    addAndMakeVisible (intensitySlider);
    addAndMakeVisible (intensityLabel);
    addAndMakeVisible (bypassBtn);
    addAndMakeVisible (spectrum);
    addAndMakeVisible (meters);
    addAndMakeVisible (paramViewport);
    paramViewport.setViewedComponent (&paramHost, false);

    buildModules();
    takeBaseline();
    startTimerHz (30);
    setResizable (true, false);
    setResizeLimits (760, 540, 1920, 1200);
    setSize (980, 680);
}

void AuralisAudioProcessorEditor::browseForPreset (bool saving)
{
    fileChooser = std::make_unique<juce::FileChooser> (saving ? "Save preset" : "Load preset", juce::File(), "*.auralis");
    fileChooser->launchAsync ((saving ? juce::FileBrowserComponent::saveMode : juce::FileBrowserComponent::openMode)
                                  | juce::FileBrowserComponent::canSelectFiles,
        [this, saving] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File())
                return;
            if (saving)
                processor.saveUserPreset (file.withFileExtension ("auralis"), file.getFileNameWithoutExtension());
            else
                processor.loadUserPreset (file);
            takeBaseline(); // the state just saved/loaded becomes the unmodified reference
        });
}

void AuralisAudioProcessorEditor::ModulePanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (panelCol);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (borderCol);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
}

void AuralisAudioProcessorEditor::buildModules()
{
    styleModuleTitle (macrosPanel.title, "Macros", macrosPanel);
    paramHost.addAndMakeVisible (macrosPanel);
    for (auto id : { "eq_low_gain", "bass_gain", "clarity_amount", "comp_threshold",
                     "comp_ratio", "amb_mix", "stereo_width", "lim_ceiling" })
        macrosPanel.cells.push_back (makeKnob (id, macrosPanel));

    for (auto& def : moduleDefs)
    {
        auto panel = std::make_unique<ModulePanel>();
        styleModuleTitle (panel->title, juce::String (def.title).toUpperCase(), *panel);
        styleToggle (panel->enableBtn);
        if (def.enableId != nullptr)
        {
            panel->enableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                processor.apvts, def.enableId, panel->enableBtn);
            panel->addAndMakeVisible (panel->enableBtn);
        }
        paramHost.addAndMakeVisible (panel.get());
        modules.push_back (std::move (panel));
    }

    for (auto* param : processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
        if (ranged == nullptr)
            continue;
        const juce::String id = ranged->getParameterID();
        if (isTopBarParam (id))
            continue;
        const auto* def = moduleDefFor (id);
        if (def == nullptr)
            continue;
        auto& panel = *modules[(size_t) (def - moduleDefs)];
        if (dynamic_cast<juce::AudioParameterBool*> (ranged) != nullptr)
        {
            if (id == def->enableId) // module enable already lives in the header
                continue;
            panel.headerToggles.push_back (makeHeaderToggle (id, shortToggleLabel (id), panel));
        }
        else
        {
            panel.cells.push_back (makeKnob (id, panel));
        }
    }
}

AuralisAudioProcessorEditor::Cell AuralisAudioProcessorEditor::makeKnob (const juce::String& id, juce::Component& parent)
{
    Cell cell;
    cell.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag,
                                                  juce::Slider::TextBoxBelow);
    auto* s = cell.slider.get();
    setupRotary (*s, 60, 18);
    if (auto* ranged = processor.apvts.getParameter (id))
        s->setDoubleClickReturnValue (true, ranged->convertFrom0to1 (ranged->getDefaultValue()));

    const auto* def = moduleDefFor (id);
    cell.label = std::make_unique<juce::Label>();
    cell.label->setText (def != nullptr ? prettyLabel (id, *def) : id, juce::dontSendNotification);
    cell.label->setFont (juce::FontOptions (11.0f));
    cell.label->setColour (juce::Label::textColourId, mutedCol);
    cell.label->setJustificationType (juce::Justification::centred);

    cell.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, id, *s);
    parent.addAndMakeVisible (s);
    parent.addAndMakeVisible (cell.label.get());
    return cell;
}

AuralisAudioProcessorEditor::HeaderToggle AuralisAudioProcessorEditor::makeHeaderToggle (
    const juce::String& id, const juce::String& label, juce::Component& parent)
{
    HeaderToggle toggle;
    toggle.button = std::make_unique<juce::ToggleButton> (label);
    styleToggle (*toggle.button);
    toggle.button->setColour (juce::ToggleButton::textColourId, textCol);
    toggle.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, id, *toggle.button);
    parent.addAndMakeVisible (toggle.button.get());
    return toggle;
}

int AuralisAudioProcessorEditor::layoutPanel (ModulePanel& panel, int width, int cellW, int ch)
{
    constexpr int pad = 8;
    const int gridX = pad + juce::jmax (0, (width - 2 * pad - gridCols * cellW) / 2);
    auto header = juce::Rectangle<int> (pad, pad, width - 2 * pad, headerH);
    panel.title.setBounds (header.removeFromLeft (140));
    for (auto& toggle : panel.headerToggles)
        toggle.button->setBounds (header.removeFromRight (46).withSizeKeepingCentre (44, 18));
    if (panel.enableAttachment != nullptr)
        panel.enableBtn.setBounds (header.removeFromRight (44).withSizeKeepingCentre (40, 18));

    const int gridY = header.getBottom() + 4;
    const int rows = (int) ((panel.cells.size() + gridCols - 1) / gridCols);
    for (size_t i = 0; i < panel.cells.size(); ++i)
    {
        const int col = (int) (i % gridCols);
        const int row = (int) (i / gridCols);
        auto cellArea = juce::Rectangle<int> (gridX + col * cellW, gridY + row * ch, cellW, ch);
        panel.cells[i].label->setBounds (cellArea.removeFromBottom (14));
        panel.cells[i].slider->setBounds (cellArea);
    }
    return gridY + rows * ch + pad;
}

void AuralisAudioProcessorEditor::relayoutModules()
{
    // 14 px keeps the content clear of the vertical scrollbar; no allocations here.
    const int w = juce::jmax (paramViewport.getWidth() - 14, 480);
    int y = 4;
    macrosPanel.setVisible (! advancedView);
    if (! advancedView)
    {
        const int h = layoutPanel (macrosPanel, w, macroCell, macroCell);
        macrosPanel.setBounds (6, y, w, h);
        y += h + 6;
    }
    for (auto& module : modules)
    {
        module->setVisible (advancedView);
        if (advancedView)
        {
            const int panelW = w - 12;
            const int h = layoutPanel (*module, panelW, (panelW - 16) / gridCols, cellH);
            module->setBounds (6, y, panelW, h);
            y += h + 6;
        }
    }
    paramHost.setSize (w, y);
}

void AuralisAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    auto top = area.removeFromTop (topBarH);
    top.reduce (8, 0);
    auto place = [&top] (juce::Component& c, int w)
    { c.setBounds (top.removeFromLeft (w).withSizeKeepingCentre (w, 26)); };

    place (presetBox, 160);
    place (saveBtn, 56);
    place (loadBtn, 56);
    place (resetBtn, 58);
    place (abBtn, 44);
    place (viewBtn, 78);
    bypassBtn.setBounds (top.removeFromRight (76).withSizeKeepingCentre (72, 24));
    auto intensityArea = top.removeFromRight (124);
    intensityLabel.setBounds (intensityArea.removeFromLeft (54));
    intensitySlider.setBounds (intensityArea);

    auto mid = area.removeFromTop (juce::jmax (180, (int) ((float) area.getHeight() * 0.32f)));
    meters.setBounds (mid.removeFromRight (metersW));
    spectrum.setBounds (mid.reduced (6, 4));

    paramViewport.setBounds (area.reduced (6, 4));
    relayoutModules();
}

void AuralisAudioProcessorEditor::takeBaseline()
{
    baselineSnapshot = juce::ValueTree ("Baseline");
    for (auto* param : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            baselineSnapshot.setProperty (ranged->getParameterID(), ranged->getValue(), nullptr);
}

bool AuralisAudioProcessorEditor::baselineMatches() const
{
    for (auto* param : processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
        if (ranged == nullptr)
            continue;
        const auto v = baselineSnapshot.getProperty (ranged->getParameterID());
        if (v.isVoid() || std::abs ((float) v - ranged->getValue()) > 1.0e-4f)
            return false;
    }
    return true;
}

void AuralisAudioProcessorEditor::timerCallback()
{
    spectrum.repaint();
    meters.repaint();
    if (++tickCount < 15) // preset check at 2 Hz, repaints at 30 Hz
        return;
    tickCount = 0;
    const bool modified = ! baselineMatches();
    if (modified != presetModified)
    {
        presetModified = modified;
        presetBox.setColour (juce::ComboBox::textColourId, modified ? amberCol : textCol);
    }
}

void AuralisAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (bgCol);
}
