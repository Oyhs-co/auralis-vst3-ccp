// SPDX-License-Identifier: AGPL-3.0-only
// Auralis parameter layout: stable IDs are API — never rename, only append.

#include "ParameterLayout.h"

namespace
{
juce::AudioParameterFloat* addFloat (juce::AudioProcessorValueTreeState::ParameterLayout& layout,
    const char* id, const char* name, float minV, float maxV, float defaultV, float skew = 1.0f)
{
    juce::NormalisableRange<float> range (minV, maxV, 0.0f, skew);
    if (skew == 1.0f)
        range.interval = 0.01f;
    auto param = std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name, range, defaultV);
    auto* raw = param.get();
    layout.add (std::move (param));
    return raw;
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createAuralisParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gain", 1 }, "Master Gain",
        juce::NormalisableRange<float> (-60.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 }, "Bypass", false));

    // Equalizer: low shelf / two peaking bands / high shelf. Shelves have no Q.
    addFloat (layout, "eq_low_freq",  "EQ Low Freq",   20.0f,   500.0f,  100.0f, 0.3f);
    addFloat (layout, "eq_low_gain",  "EQ Low Gain",  -15.0f,    15.0f,    0.0f);
    addFloat (layout, "eq_band1_freq","EQ Band1 Freq", 200.0f,  2000.0f,  400.0f, 0.3f);
    addFloat (layout, "eq_band1_gain","EQ Band1 Gain",-15.0f,    15.0f,    0.0f);
    addFloat (layout, "eq_band1_q",   "EQ Band1 Q",      0.1f,    10.0f,   0.8f);
    addFloat (layout, "eq_band2_freq","EQ Band2 Freq",1000.0f, 10000.0f, 2500.0f, 0.3f);
    addFloat (layout, "eq_band2_gain","EQ Band2 Gain",-15.0f,    15.0f,    0.0f);
    addFloat (layout, "eq_band2_q",   "EQ Band2 Q",      0.1f,    10.0f,   0.8f);
    addFloat (layout, "eq_high_freq", "EQ High Freq", 2000.0f, 20000.0f, 8000.0f, 0.3f);
    addFloat (layout, "eq_high_gain", "EQ High Gain", -15.0f,    15.0f,    0.0f);

    // Bass enhancement. sub_hp 0 = protection off.
    addFloat (layout, "bass_freq",       "Bass Freq",      40.0f, 200.0f, 100.0f, 0.3f);
    addFloat (layout, "bass_gain",       "Bass Gain",     -12.0f,  12.0f,   0.0f);
    addFloat (layout, "bass_saturation", "Bass Saturation", 0.0f,   1.0f,   0.0f);
    addFloat (layout, "bass_sub_hp",     "Bass Sub HP",     0.0f,  60.0f,   0.0f);

    // Clarity / harmonic exciter.
    addFloat (layout, "clarity_amount",   "Clarity Amount",    0.0f,    1.0f,   0.0f);
    addFloat (layout, "clarity_freq",     "Clarity Freq",   1000.0f, 8000.0f, 3000.0f, 0.3f);
    addFloat (layout, "clarity_presence", "Clarity Presence", -6.0f,   6.0f,   0.0f);

    // Dynamics.
    addFloat (layout, "comp_threshold", "Comp Threshold", -60.0f,   0.0f,   0.0f);
    addFloat (layout, "comp_ratio",       "Comp Ratio",      1.0f,  20.0f,   1.0f);
    addFloat (layout, "comp_attack",      "Comp Attack",     1.0f, 100.0f,  10.0f);
    addFloat (layout, "comp_release",     "Comp Release",   10.0f, 1000.0f, 150.0f);
    addFloat (layout, "comp_knee",        "Comp Knee",       0.0f,  24.0f,   6.0f);
    addFloat (layout, "comp_makeup",      "Comp Makeup",     0.0f,  24.0f,   0.0f);
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "comp_rms", 1 }, "Comp RMS", false));

    // Ambience (reverb).
    addFloat (layout, "amb_mix",      "Ambience Mix",        0.0f,   1.0f,   0.0f);
    addFloat (layout, "amb_predelay", "Ambience Predelay",   0.0f, 200.0f,  20.0f);
    addFloat (layout, "amb_decay",    "Ambience Decay",      0.1f,  10.0f,   1.5f);
    addFloat (layout, "amb_damping",  "Ambience Damping",    0.0f,   1.0f,   0.5f);
    addFloat (layout, "amb_width",    "Ambience Width",      0.0f,   1.0f,   0.7f);

    // Stereo image.
    addFloat (layout, "stereo_width",     "Stereo Width",     0.0f,  2.0f,  1.0f);
    addFloat (layout, "stereo_mid_gain",  "Stereo Mid Gain", -12.0f, 12.0f,  0.0f);
    addFloat (layout, "stereo_side_gain", "Stereo Side Gain", -12.0f, 12.0f,  0.0f);
    addFloat (layout, "stereo_balance",   "Stereo Balance",   -1.0f,  1.0f,  0.0f);

    // Output limiter.
    addFloat (layout, "lim_threshold", "Limiter Threshold", -24.0f,  0.0f,   0.0f);
    addFloat (layout, "lim_ceiling",   "Limiter Ceiling",   -12.0f,  0.0f,   0.0f);
    addFloat (layout, "lim_release",   "Limiter Release",     5.0f, 1000.0f, 50.0f);

    return layout;
}