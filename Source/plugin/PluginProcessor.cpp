// SPDX-License-Identifier: AGPL-3.0-only
// Auralis audio processor implementation: host adapter for the fixed DSP chain.

#include "PluginProcessor.h"
#include "ParameterLayout.h"
#include "PluginEditor.h"

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
    params.intensity = apvts.getRawParameterValue ("intensity");
    params.eqEn = apvts.getRawParameterValue ("eq_en");
    params.eqLowEn = apvts.getRawParameterValue ("eq_low_en");
    params.eqBand1En = apvts.getRawParameterValue ("eq_band1_en");
    params.eqBand2En = apvts.getRawParameterValue ("eq_band2_en");
    params.eqHighEn = apvts.getRawParameterValue ("eq_high_en");
    params.bassEn = apvts.getRawParameterValue ("bass_en");
    params.clarityEn = apvts.getRawParameterValue ("clarity_en");
    params.compEn = apvts.getRawParameterValue ("comp_en");
    params.ambEn = apvts.getRawParameterValue ("amb_en");
    params.stereoEn = apvts.getRawParameterValue ("stereo_en");
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
    pipeline.prepare ({ sampleRate, samplesPerBlock, juce::jmax (1, getTotalNumOutputChannels()) });
    analyzerIn.prepare (sampleRate, samplesPerBlock);
    analyzerOut.prepare (sampleRate, samplesPerBlock);
    pushParameters();
    setLatencySamples (limiterProcessor->getLatencySamples());
}

void AuralisAudioProcessor::releaseResources()
{
    pipeline.reset(); // fresh DSP state for the next prepare (covers sample-rate/layout changes)
}

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
    // Global intensity (0..1) scales effect depth: 0 = neutral chain, 1 = as set.
    // Frequencies, times and the limiter (a safety stage) are not scaled.
    const float i = params.intensity->load();

    gainProcessor->setGainDb (params.gain->load());
    gainProcessor->setBypass (params.bypass->load() > 0.5f);

    eqProcessor->setBand (0, params.eqLowFreq->load(), params.eqLowGain->load() * i, 0.707f);
    eqProcessor->setBand (1, params.eqBand1Freq->load(), params.eqBand1Gain->load() * i, params.eqBand1Q->load());
    eqProcessor->setBand (2, params.eqBand2Freq->load(), params.eqBand2Gain->load() * i, params.eqBand2Q->load());
    eqProcessor->setBand (3, params.eqHighFreq->load(), params.eqHighGain->load() * i, 0.707f);

    bassProcessor->setShelfFreq (params.bassFreq->load());
    bassProcessor->setShelfGainDb (params.bassGain->load() * i);
    bassProcessor->setSaturation (params.bassSaturation->load() * i);
    bassProcessor->setSubHighpassFreq (params.bassSubHp->load());

    clarityProcessor->setAmount (params.clarityAmount->load() * i);
    clarityProcessor->setCenterFreq (params.clarityFreq->load());
    clarityProcessor->setPresenceDb (params.clarityPresence->load() * i);

    compProcessor->setThresholdDb (params.compThreshold->load());
    compProcessor->setRatio (1.0f + (params.compRatio->load() - 1.0f) * i);
    compProcessor->setAttackMs (params.compAttack->load());
    compProcessor->setReleaseMs (params.compRelease->load());
    compProcessor->setKneeDb (params.compKnee->load());
    compProcessor->setMakeupDb (params.compMakeup->load() * i);
    compProcessor->setRmsMode (params.compRms->load() > 0.5f);

    ambProcessor->setMix (params.ambMix->load() * i);
    ambProcessor->setPredelayMs (params.ambPredelay->load());
    ambProcessor->setDecaySeconds (params.ambDecay->load());
    ambProcessor->setDamping (params.ambDamping->load());
    ambProcessor->setWidth (params.ambWidth->load());

    stereoProcessor->setWidth (1.0f + (params.stereoWidth->load() - 1.0f) * i);
    stereoProcessor->setMidGainDb (params.stereoMidGain->load() * i);
    stereoProcessor->setSideGainDb (params.stereoSideGain->load() * i);
    stereoProcessor->setBalance (params.stereoBalance->load());

    limiterProcessor->setThresholdDb (params.limThreshold->load());
    limiterProcessor->setCeilingDb (params.limCeiling->load());
    limiterProcessor->setReleaseMs (params.limRelease->load());

    applyEnables();
}

void AuralisAudioProcessor::pushNeutralParameters()
{
    // True bypass (U-01): every processor at its verified-neutral settings outputs the exact
    // dry signal, transitions are click-free through the internal smoothers, and the limiter
    // ring keeps advancing so the declared latency stays valid. Peaks above 0 dBFS still hit
    // the limiter's 0 dB ceiling — the one intentional safety clamp during bypass.
    gainProcessor->setGainDb (0.0f);
    gainProcessor->setBypass (false);

    eqProcessor->setBand (0, params.eqLowFreq->load(), 0.0f, 0.707f);
    eqProcessor->setBand (1, params.eqBand1Freq->load(), 0.0f, params.eqBand1Q->load());
    eqProcessor->setBand (2, params.eqBand2Freq->load(), 0.0f, params.eqBand2Q->load());
    eqProcessor->setBand (3, params.eqHighFreq->load(), 0.0f, 0.707f);
    eqProcessor->setBandEnabled (0, true);
    eqProcessor->setBandEnabled (1, true);
    eqProcessor->setBandEnabled (2, true);
    eqProcessor->setBandEnabled (3, true);

    bassProcessor->setShelfFreq (params.bassFreq->load());
    bassProcessor->setShelfGainDb (0.0f);
    bassProcessor->setSaturation (0.0f);
    bassProcessor->setSubHighpassFreq (0.0f);

    clarityProcessor->setAmount (0.0f);
    clarityProcessor->setCenterFreq (params.clarityFreq->load());
    clarityProcessor->setPresenceDb (0.0f);

    compProcessor->setThresholdDb (0.0f);
    compProcessor->setRatio (1.0f);
    compProcessor->setAttackMs (params.compAttack->load());
    compProcessor->setReleaseMs (params.compRelease->load());
    compProcessor->setKneeDb (params.compKnee->load());
    compProcessor->setMakeupDb (0.0f);
    compProcessor->setRmsMode (params.compRms->load() > 0.5f);

    ambProcessor->setMix (0.0f);
    ambProcessor->setPredelayMs (params.ambPredelay->load());
    ambProcessor->setDecaySeconds (params.ambDecay->load());
    ambProcessor->setDamping (params.ambDamping->load());
    ambProcessor->setWidth (params.ambWidth->load());

    stereoProcessor->setWidth (1.0f);
    stereoProcessor->setMidGainDb (0.0f);
    stereoProcessor->setSideGainDb (0.0f);
    stereoProcessor->setBalance (0.0f);

    limiterProcessor->setThresholdDb (0.0f);
    limiterProcessor->setCeilingDb (0.0f);
    limiterProcessor->setReleaseMs (params.limRelease->load());

    applyEnables();
}

void AuralisAudioProcessor::applyEnables()
{
    // Per-module enables (U-03). Slot order matches buildPipeline(); the limiter
    // slot (7) is never disabled — that would invalidate the reported latency.
    pipeline.setEnabled (1, params.eqEn->load() > 0.5f);
    pipeline.setEnabled (2, params.bassEn->load() > 0.5f);
    pipeline.setEnabled (3, params.clarityEn->load() > 0.5f);
    pipeline.setEnabled (4, params.compEn->load() > 0.5f);
    pipeline.setEnabled (5, params.ambEn->load() > 0.5f);
    pipeline.setEnabled (6, params.stereoEn->load() > 0.5f);

    eqProcessor->setBandEnabled (0, params.eqLowEn->load() > 0.5f);
    eqProcessor->setBandEnabled (1, params.eqBand1En->load() > 0.5f);
    eqProcessor->setBandEnabled (2, params.eqBand2En->load() > 0.5f);
    eqProcessor->setBandEnabled (3, params.eqHighEn->load() > 0.5f);
}

void AuralisAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (params.bypass->load() > 0.5f)
        pushNeutralParameters(); // true bypass: dry in, dry out, latency unchanged
    else
        pushParameters();

    float* channels[2];
    const auto numChannels = juce::jmin (buffer.getNumChannels(), getTotalNumOutputChannels());
    for (int c = 0; c < numChannels; ++c)
        channels[c] = buffer.getWritePointer (c);
    auralis::AudioBufferView view (channels, numChannels, buffer.getNumSamples());

    analyzerIn.process (view); // read-only pre-chain metering
    pipeline.process (view);
    analyzerOut.process (view); // read-only post-chain metering
}

// --- Presets / A/B (message thread only) ---

void AuralisAudioProcessor::applyPreset (const auralis::Preset& preset)
{
    for (auto& [id, value] : preset.values)
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
}

void AuralisAudioProcessor::resetToDefaults()
{
    for (auto* param : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            ranged->setValueNotifyingHost (ranged->getDefaultValue());
}

bool AuralisAudioProcessor::saveUserPreset (const juce::File& file, const juce::String& name)
{
    auralis::Preset preset;
    preset.name = name;
    for (auto* param : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            preset.values[ranged->getParameterID()] = ranged->convertFrom0to1 (ranged->getValue());
    return file.replaceWithText (auralis::PresetManager::toJson (preset));
}

bool AuralisAudioProcessor::loadUserPreset (const juce::File& file)
{
    auralis::Preset preset;
    if (! auralis::PresetManager::fromJson (file.loadFileAsString(), preset))
        return false;
    applyPreset (preset);
    return true;
}

void AuralisAudioProcessor::snapshotTo (juce::ValueTree& slot)
{
    slot = juce::ValueTree ("AbState");
    for (auto* param : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            slot.setProperty (ranged->getParameterID(), ranged->getValue(), nullptr); // normalized 0..1
}

void AuralisAudioProcessor::restoreFrom (const juce::ValueTree& slot)
{
    for (auto* param : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            if (auto value = slot.getProperty (ranged->getParameterID()); value.isDouble() || value.isInt())
                ranged->setValueNotifyingHost ((float) (double) value);
}

void AuralisAudioProcessor::toggleAb()
{
    snapshotTo (abSlots[activeSlot]);
    activeSlot ^= 1;
    if (abSlots[activeSlot].getNumProperties() == 0)
        snapshotTo (abSlots[activeSlot]); // first use: identical copy, no jump
    else
        restoreFrom (abSlots[activeSlot]);
}

juce::String AuralisAudioProcessor::getAbLabel() const
{
    return activeSlot == 0 ? "A" : "B";
}

juce::AudioProcessorEditor* AuralisAudioProcessor::createEditor()
{
    return new AuralisAudioProcessorEditor (*this);
}
bool AuralisAudioProcessor::hasEditor() const { return true; }

const juce::String AuralisAudioProcessor::getName() const { return JucePlugin_Name; }
bool AuralisAudioProcessor::acceptsMidi() const { return false; }
bool AuralisAudioProcessor::producesMidi() const { return false; }
bool AuralisAudioProcessor::isMidiEffect() const { return false; }
double AuralisAudioProcessor::getTailLengthSeconds() const
{
    return (double) ambProcessor->getDecaySeconds(); // reverb tail for host buffer sizing
}

int AuralisAudioProcessor::getNumPrograms() { return 1; }
int AuralisAudioProcessor::getCurrentProgram() { return 0; }
void AuralisAudioProcessor::setCurrentProgram (int) {}
const juce::String AuralisAudioProcessor::getProgramName (int) { return "Default"; }
void AuralisAudioProcessor::changeProgramName (int, const juce::String&) {}

void AuralisAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto xml = apvts.copyState().createXml();
    if (xml == nullptr)
        return;

    xml->setAttribute ("schema_version", auralis::PresetManager::schemaVersion);

    // A/B slots travel inside the host state; removed again on load.
    auto* slots = new juce::XmlElement ("Slots");
    slots->setAttribute ("active", activeSlot);
    for (int s = 0; s < 2; ++s)
        if (auto slotXml = abSlots[s].createXml(); slotXml != nullptr)
        {
            slotXml->setAttribute ("index", s);
            slots->addChildElement (slotXml.release());
        }
    xml->addChildElement (slots);

    copyXmlToBinary (*xml, destData);
}

void AuralisAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    // Migration hook: older states without schema_version are treated as version 1.
    const int schema = xml->getIntAttribute ("schema_version", 1);
    juce::ignoreUnused (schema);

    if (auto* slots = xml->getChildByName ("Slots"))
    {
        activeSlot = juce::jlimit (0, 1, slots->getIntAttribute ("active", 0));
        for (auto* slotXml : slots->getChildIterator())
            if (auto tree = juce::ValueTree::fromXml (*slotXml); tree.isValid())
                abSlots[juce::jlimit (0, 1, slotXml->getIntAttribute ("index", 0))] = tree;
        xml->removeChildElement (slots, true);
    }

    apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorParameter* AuralisAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter ("bypass");
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AuralisAudioProcessor(); }