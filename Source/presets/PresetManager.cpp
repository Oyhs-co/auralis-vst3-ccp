// SPDX-License-Identifier: AGPL-3.0-only

#include "PresetManager.h"

namespace auralis
{
std::vector<Preset> PresetManager::factoryPresets()
{
    // Values are in real units (dB, Hz, ratios, 0..1 amounts); only touched params are listed.
    return {
        { "Warm Bass",
          { { "bass_freq", 90.0f }, { "bass_gain", 5.0f }, { "bass_saturation", 0.25f },
            { "eq_low_gain", 2.0f }, { "eq_low_freq", 90.0f } } },
        { "Vocal Clarity",
          { { "clarity_amount", 0.45f }, { "clarity_freq", 3200.0f }, { "clarity_presence", 2.0f },
            { "eq_band1_freq", 250.0f }, { "eq_band1_gain", -2.0f }, { "eq_band1_q", 1.2f },
            { "eq_band2_freq", 3000.0f }, { "eq_band2_gain", 2.0f },
            { "comp_threshold", -18.0f }, { "comp_ratio", 3.0f }, { "comp_makeup", 2.0f } } },
        { "Wide Stereo",
          { { "stereo_width", 1.4f }, { "amb_mix", 0.15f }, { "amb_decay", 1.2f },
            { "eq_high_gain", 1.5f } } },
    };
}

juce::String PresetManager::toJson (const Preset& preset)
{
    auto* params = new juce::DynamicObject();
    for (auto& [id, value] : preset.values)
        params->setProperty (id, juce::var (value));

    auto* root = new juce::DynamicObject();
    root->setProperty ("schema_version", schemaVersion);
    root->setProperty ("name", preset.name);
    root->setProperty ("params", juce::var (params));

    return juce::JSON::toString (juce::var (root));
}

bool PresetManager::fromJson (const juce::String& source, Preset& out)
{
    auto parsed = juce::JSON::parse (source);
    auto* root = parsed.getDynamicObject();
    if (root == nullptr)
        return false;

    if ((int) root->getProperty ("schema_version") != schemaVersion)
        return false; // future versions migrate here

    out.name = root->getProperty ("name").toString();
    out.values.clear();

    auto* params = root->getProperty ("params").getDynamicObject();
    if (params != nullptr)
        for (auto& entry : params->getProperties())
            if (auto v = entry.value; v.isDouble() || v.isInt())
                out.values[entry.name.toString()] = (float) (double) v;

    return true;
}
}