// SPDX-License-Identifier: AGPL-3.0-only
// Preset data: JSON (schema_version, name, real-valued params). No APVTS dependency —
// the plugin adapter applies values to the parameter tree.

#pragma once

#include <juce_core/juce_core.h>

#include <map>
#include <vector>

namespace auralis
{
struct Preset
{
    juce::String name;
    std::map<juce::String, float> values; // param ID -> real value
};

class PresetManager
{
public:
    static constexpr int schemaVersion = 1;

    static std::vector<Preset> factoryPresets();
    static juce::String toJson (const Preset& preset);
    // Returns false for malformed JSON or an unknown schema version (migration hook).
    static bool fromJson (const juce::String& source, Preset& out);
};
}