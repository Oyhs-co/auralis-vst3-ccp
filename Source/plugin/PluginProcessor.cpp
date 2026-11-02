// SPDX-License-Identifier: AGPL-3.0-only
// Auralis audio processor implementation: gain passthrough, bypass and APVTS state.

#include "PluginProcessor.h"

AuralisAudioProcessor::AuralisAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    gainParam = apvts.getRawParameterValue ("gain");
    bypassParam = apvts.getRawParameterValue ("bypass");
}

juce::AudioProcessorValueTreeState::ParameterLayout AuralisAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gain", 1 },
        "Master Gain",
        juce::NormalisableRange<float> (-60.0f, 12.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 },
        "Bypass",
        false));

    return layout;
}

void AuralisAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    gainSmoothed.reset (sampleRate, 0.02);
    gainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (gainParam->load()));
}

void AuralisAudioProcessor::releaseResources() {}

bool AuralisAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainIn = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != mainOut)
        return false;

    return mainIn == juce::AudioChannelSet::mono() || mainIn == juce::AudioChannelSet::stereo();
}

void AuralisAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    if (bypassParam->load() > 0.5f)
        return; // buffer already holds the input; passthrough

    gainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (gainParam->load()));

    const auto numChannels = (juce::uint32) juce::jmin (buffer.getNumChannels(), getTotalNumInputChannels(), getTotalNumOutputChannels());
    const auto numSamples = (juce::uint32) buffer.getNumSamples();

    for (auto ch = 0u; ch < numChannels; ++ch)
    {
        auto* channelData = buffer.getWritePointer ((int) ch);

        for (auto i = 0u; i < numSamples; ++i)
            channelData[i] *= gainSmoothed.getNextValue();
    }
}

juce::AudioProcessorEditor* AuralisAudioProcessor::createEditor() { return nullptr; }
bool AuralisAudioProcessor::hasEditor() const { return false; }

const juce::String AuralisAudioProcessor::getName() const { return JucePlugin_Name; }
bool AuralisAudioProcessor::acceptsMidi() const { return false; }
bool AuralisAudioProcessor::producesMidi() const { return false; }
bool AuralisAudioProcessor::isMidiEffect() const { return false; }
double AuralisAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int AuralisAudioProcessor::getNumPrograms() { return 1; }
int AuralisAudioProcessor::getCurrentProgram() { return 0; }
void AuralisAudioProcessor::setCurrentProgram (int) {}
const juce::String AuralisAudioProcessor::getProgramName (int) { return "Default"; }
void AuralisAudioProcessor::changeProgramName (int, const juce::String&) {}

void AuralisAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void AuralisAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorParameter* AuralisAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter ("bypass");
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AuralisAudioProcessor(); }
