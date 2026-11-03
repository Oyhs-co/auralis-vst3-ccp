// SPDX-License-Identifier: AGPL-3.0-only
// Auralis audio processor implementation: host adapter for the fixed DSP chain.

#include "PluginProcessor.h"
#include "ParameterLayout.h"

AuralisAudioProcessor::AuralisAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createAuralisParameterLayout())
{
    cacheParams();
    buildPipeline();
}

void AuralisAudioProcessor::cacheParams()
{
    params.gain = apvts.getRawParameterValue ("gain");
    params.bypass = apvts.getRawParameterValue ("bypass");
    params.eqLowFreq = apvts.getRawParameterValue ("eq_low_freq");
    params.eqLowGain = apvts.getRawParameterValue ("eq_low_gain");
    params.eqBand1Freq = apvts.getRawParameterValue ("eq_band1_freq");
    params.eqBand1Gain = apvts.getRawParameterValue ("eq_band1_gain");
    params.eqBand1Q = apvts.getRawParameterValue ("eq_band1_q");
    params.eqBand2Freq = apvts.getRawParameterValue ("eq_band2_freq");
    params.eqBand2Gain = apvts.getRawParameterValue ("eq_band2_gain");
    params.eqBand2Q = apvts.getRawParameterValue ("eq_band2_q");
    params.eqHighFreq = apvts.getRawParameterValue ("eq_high_freq");
    params.eqHighGain = apvts.getRawParameterValue ("eq_high_gain");
    params.bassFreq = apvts.getRawParameterValue ("bass_freq");
    params.bassGain = apvts.getRawParameterValue ("bass_gain");
    params.bassSaturation = apvts.getRawParameterValue ("bass_saturation");
    params.bassSubHp = apvts.getRawParameterValue ("bass_sub_hp");
    params.clarityAmount = apvts.getRawParameterValue ("clarity_amount");
    params.clarityFreq = apvts.getRawParameterValue ("clarity_freq");
    params.clarityPresence = apvts.getRawParameterValue ("clarity_presence");
    params.compThreshold = apvts.getRawParameterValue ("comp_threshold");
    params.compRatio = apvts.getRawParameterValue ("comp_ratio");
    params.compAttack = apvts.getRawParameterValue ("comp_attack");
    params.compRelease = apvts.getRawParameterValue ("comp_release");
    params.compKnee = apvts.getRawParameterValue ("comp_knee");
    params.compMakeup = apvts.getRawParameterValue ("comp_makeup");
    params.compRms = apvts.getRawParameterValue ("comp_rms");
    params.ambMix = apvts.getRawParameterValue ("amb_mix");
    params.ambPredelay = apvts.getRawParameterValue ("amb_predelay");
    params.ambDecay = apvts.getRawParameterValue ("amb_decay");
    params.ambDamping = apvts.getRawParameterValue ("amb_damping");
    params.ambWidth = apvts.getRawParameterValue ("amb_width");
    params.stereoWidth = apvts.getRawParameterValue ("stereo_width");
    params.stereoMidGain = apvts.getRawParameterValue ("stereo_mid_gain");
    params.stereoSideGain = apvts.getRawParameterValue ("stereo_side_gain");
    params.stereoBalance = apvts.getRawParameterValue ("stereo_balance");
    params.limThreshold = apvts.getRawParameterValue ("lim_threshold");
    params.limCeiling = apvts.getRawParameterValue ("lim_ceiling");
    params.limRelease = apvts.getRawParameterValue ("lim_release");
}

void AuralisAudioProcessor::buildPipeline()
{
    // Fixed chain (plan 5.2): Gain -> EQ -> Bass -> Clarity -> Dynamics -> Ambience -> Stereo -> Limiter.
    gainProcessor = static_cast<auralis::GainProcessor*> (
        pipeline.addProcessor (std::make_unique<auralis::GainProcessor>()));
    eqProcessor = static_cast<auralis::EqualizerProcessor*> (
        pipeline.addProcessor (std::make_unique<auralis::EqualizerProcessor>()));
    bassProcessor = static_cast<auralis::BassEnhancerProcessor*> (
        pipeline.addProcessor (std::make_unique<auralis::BassEnhancerProcessor>()));
    clarityProcessor = static_cast<auralis::ClarityProcessor*> (
        pipeline.addProcessor (std::make_unique<auralis::ClarityProcessor>()));
    compProcessor = static_cast<auralis::CompressorProcessor*> (
        pipeline.addProcessor (std::make_unique<auralis::CompressorProcessor>()));
    ambProcessor = static_cast<auralis::AmbienceProcessor*> (
        pipeline.addProcessor (std::make_unique<auralis::AmbienceProcessor>()));
    stereoProcessor = static_cast<auralis::StereoEnhancerProcessor*> (
        pipeline.addProcessor (std::make_unique<auralis::StereoEnhancerProcessor>()));
    limiterProcessor = static_cast<auralis::LimiterProcessor*> (
        pipeline.addProcessor (std::make_unique<auralis::LimiterProcessor>()));
}

void AuralisAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    pipeline.prepare ({ sampleRate, samplesPerBlock });
    pushParameters();
    setLatencySamples (limiterProcessor->getLatencySamples());
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

void AuralisAudioProcessor::pushParameters()
{
    gainProcessor->setGainDb (params.gain->load());
    gainProcessor->setBypass (params.bypass->load() > 0.5f);

    eqProcessor->setBand (0, params.eqLowFreq->load(), params.eqLowGain->load(), 0.707f);
    eqProcessor->setBand (1, params.eqBand1Freq->load(), params.eqBand1Gain->load(), params.eqBand1Q->load());
    eqProcessor->setBand (2, params.eqBand2Freq->load(), params.eqBand2Gain->load(), params.eqBand2Q->load());
    eqProcessor->setBand (3, params.eqHighFreq->load(), params.eqHighGain->load(), 0.707f);

    bassProcessor->setShelfFreq (params.bassFreq->load());
    bassProcessor->setShelfGainDb (params.bassGain->load());
    bassProcessor->setSaturation (params.bassSaturation->load());
    bassProcessor->setSubHighpassFreq (params.bassSubHp->load());

    clarityProcessor->setAmount (params.clarityAmount->load());
    clarityProcessor->setCenterFreq (params.clarityFreq->load());
    clarityProcessor->setPresenceDb (params.clarityPresence->load());

    compProcessor->setThresholdDb (params.compThreshold->load());
    compProcessor->setRatio (params.compRatio->load());
    compProcessor->setAttackMs (params.compAttack->load());
    compProcessor->setReleaseMs (params.compRelease->load());
    compProcessor->setKneeDb (params.compKnee->load());
    compProcessor->setMakeupDb (params.compMakeup->load());
    compProcessor->setRmsMode (params.compRms->load() > 0.5f);

    ambProcessor->setMix (params.ambMix->load());
    ambProcessor->setPredelayMs (params.ambPredelay->load());
    ambProcessor->setDecaySeconds (params.ambDecay->load());
    ambProcessor->setDamping (params.ambDamping->load());
    ambProcessor->setWidth (params.ambWidth->load());

    stereoProcessor->setWidth (params.stereoWidth->load());
    stereoProcessor->setMidGainDb (params.stereoMidGain->load());
    stereoProcessor->setSideGainDb (params.stereoSideGain->load());
    stereoProcessor->setBalance (params.stereoBalance->load());

    limiterProcessor->setThresholdDb (params.limThreshold->load());
    limiterProcessor->setCeilingDb (params.limCeiling->load());
    limiterProcessor->setReleaseMs (params.limRelease->load());
}

void AuralisAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    pushParameters();

    float* channels[2];
    const auto numChannels = juce::jmin (buffer.getNumChannels(), getTotalNumOutputChannels());
    for (int c = 0; c < numChannels; ++c)
        channels[c] = buffer.getWritePointer (c);
    auralis::AudioBufferView view (channels, numChannels, buffer.getNumSamples());

    pipeline.process (view);
    analyzer.process (view); // post-chain metering; read-only
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