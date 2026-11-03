// SPDX-License-Identifier: AGPL-3.0-only
// Auralis audio processor: JUCE adapter. Wires the host to the DSP pipeline; no DSP lives here.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "DspPipeline.h"
#include "../presets/PresetManager.h"
#include "gain/GainProcessor.h"
#include "equalizer/EqualizerProcessor.h"
#include "bass/BassEnhancerProcessor.h"
#include "clarity/ClarityProcessor.h"
#include "dynamics/CompressorProcessor.h"
#include "ambience/AmbienceProcessor.h"
#include "stereo/StereoEnhancerProcessor.h"
#include "limiter/LimiterProcessor.h"
#include "SpectrumAnalyzer.h"

class AuralisAudioProcessorEditor;
class AuralisAudioProcessor : public juce::AudioProcessor
{
public:
    AuralisAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    // Presets, A/B and meters (message thread only; called by the editor).
    void applyPreset (const auralis::Preset& preset);
    void resetToDefaults();
    bool saveUserPreset (const juce::File& file, const juce::String& name);
    bool loadUserPreset (const juce::File& file);
    void toggleAb();
    juce::String getAbLabel() const;
    float getCorrelation() const;
    const std::atomic<float>& getStereoCorrelationAtomic() const { return stereoProcessor->getCorrelationAtomic(); }
    auralis::SpectrumAnalyzer& getInputAnalyzer() { return analyzerIn; }
    auralis::SpectrumAnalyzer& getOutputAnalyzer() { return analyzerOut; }

private:
    // Cached raw parameter pointers: read once per block, never looked up by string in RT.
    struct Params
    {
        std::atomic<float>* gain = nullptr;
        std::atomic<float>* bypass = nullptr;
        std::atomic<float>* intensity = nullptr;
        std::atomic<float>* eqLowFreq = nullptr;
        std::atomic<float>* eqLowGain = nullptr;
        std::atomic<float>* eqBand1Freq = nullptr;
        std::atomic<float>* eqBand1Gain = nullptr;
        std::atomic<float>* eqBand1Q = nullptr;
        std::atomic<float>* eqBand2Freq = nullptr;
        std::atomic<float>* eqBand2Gain = nullptr;
        std::atomic<float>* eqBand2Q = nullptr;
        std::atomic<float>* eqHighFreq = nullptr;
        std::atomic<float>* eqHighGain = nullptr;
        std::atomic<float>* bassFreq = nullptr;
        std::atomic<float>* bassGain = nullptr;
        std::atomic<float>* bassSaturation = nullptr;
        std::atomic<float>* bassSubHp = nullptr;
        std::atomic<float>* clarityAmount = nullptr;
        std::atomic<float>* clarityFreq = nullptr;
        std::atomic<float>* clarityPresence = nullptr;
        std::atomic<float>* compThreshold = nullptr;
        std::atomic<float>* compRatio = nullptr;
        std::atomic<float>* compAttack = nullptr;
        std::atomic<float>* compRelease = nullptr;
        std::atomic<float>* compKnee = nullptr;
        std::atomic<float>* compMakeup = nullptr;
        std::atomic<float>* compRms = nullptr;
        std::atomic<float>* ambMix = nullptr;
        std::atomic<float>* ambPredelay = nullptr;
        std::atomic<float>* ambDecay = nullptr;
        std::atomic<float>* ambDamping = nullptr;
        std::atomic<float>* ambWidth = nullptr;
        std::atomic<float>* stereoWidth = nullptr;
        std::atomic<float>* stereoMidGain = nullptr;
        std::atomic<float>* stereoSideGain = nullptr;
        std::atomic<float>* stereoBalance = nullptr;
        std::atomic<float>* limThreshold = nullptr;
        std::atomic<float>* limCeiling = nullptr;
        std::atomic<float>* limRelease = nullptr;
    };

    void cacheParams();
    void buildPipeline();
    void pushParameters();
    void snapshotTo (juce::ValueTree& slot);
    void restoreFrom (const juce::ValueTree& slot);

    friend class AuralisAudioProcessorEditor;

    juce::AudioProcessorValueTreeState apvts;
    Params params;

    auralis::DspPipeline pipeline;
    auralis::GainProcessor* gainProcessor = nullptr;
    auralis::EqualizerProcessor* eqProcessor = nullptr;
    auralis::BassEnhancerProcessor* bassProcessor = nullptr;
    auralis::ClarityProcessor* clarityProcessor = nullptr;
    auralis::CompressorProcessor* compProcessor = nullptr;
    auralis::AmbienceProcessor* ambProcessor = nullptr;
    auralis::StereoEnhancerProcessor* stereoProcessor = nullptr;
    auralis::LimiterProcessor* limiterProcessor = nullptr;
    auralis::SpectrumAnalyzer analyzerIn;  // pre-chain observer
    auralis::SpectrumAnalyzer analyzerOut; // post-chain observer

    // A/B slots: param id -> real value. Saved inside the host state (Slots child).
    juce::ValueTree abSlots[2] { juce::ValueTree ("AbState"), juce::ValueTree ("AbState") };
    int activeSlot = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuralisAudioProcessor)
};