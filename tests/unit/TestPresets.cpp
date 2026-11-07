// SPDX-License-Identifier: AGPL-3.0-only
// Unit tests for auralis::PresetManager (JSON round-trip, schema guard, factory presets).

#include "../../Source/presets/PresetManager.h"

#include <cmath>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace
{
int failures = 0;

#define CHECK(cond) do { if (! (cond)) { ++failures; std::cerr << "CHECK failed: " #cond \
    " at " << __FILE__ << ':' << __LINE__ << '\n'; } } while (false)

using auralis::Preset;
using auralis::PresetManager;

void jsonRoundTrip()
{
    const Preset original { "My Preset",
        { { "gain", -3.5f }, { "bass_gain", 4.0f }, { "clarity_amount", 0.5f } } };

    const auto json = PresetManager::toJson (original);
    Preset decoded;
    CHECK (PresetManager::fromJson (json, decoded));

    CHECK (decoded.name == original.name);
    CHECK (decoded.values.size() == original.values.size());
    for (auto& [id, value] : original.values)
    {
        const auto it = decoded.values.find (id);
        CHECK (it != decoded.values.end());
        if (it != decoded.values.end())
            CHECK (std::fabs (it->second - value) <= 1.0e-6f);
    }
}

void rejectsMalformedJson()
{
    Preset out;
    CHECK (! PresetManager::fromJson ("not json{", out));
    CHECK (! PresetManager::fromJson ("", out));
}

void rejectsUnknownSchema()
{
    const Preset preset { "X", { { "gain", 1.0f } } };
    const auto json = PresetManager::toJson (preset);

    // Parse and bump the schema version (robust to JSON formatting).
    auto parsed = juce::JSON::parse (json);
    auto* root = parsed.getDynamicObject();
    CHECK (root != nullptr);
    if (root == nullptr)
        return;
    root->setProperty ("schema_version", 2);
    CHECK ((int) root->getProperty ("schema_version") == 2);
    const auto bumped = juce::JSON::toString (parsed);

    Preset out;
    CHECK (! PresetManager::fromJson (bumped, out));
    CHECK (! PresetManager::fromJson ("{\"schema_version\":2,\"name\":\"X\",\"params\":{}}", out));
}

bool isKnownParamId (const juce::String& id)
{
    static const juce::String known[] = {
        "gain", "bypass", "intensity",
        "eq_low_freq", "eq_low_gain",
        "eq_band1_freq", "eq_band1_gain", "eq_band1_q",
        "eq_band2_freq", "eq_band2_gain", "eq_band2_q",
        "eq_high_freq", "eq_high_gain",
        "bass_freq", "bass_gain", "bass_saturation", "bass_sub_hp",
        "clarity_amount", "clarity_freq", "clarity_presence",
        "comp_threshold", "comp_ratio", "comp_attack", "comp_release",
        "comp_knee", "comp_makeup", "comp_rms",
        "amb_mix", "amb_predelay", "amb_decay", "amb_damping", "amb_width",
        "stereo_width", "stereo_mid_gain", "stereo_side_gain", "stereo_balance",
        "lim_threshold", "lim_ceiling", "lim_release",
    };
    for (auto& k : known)
        if (k == id)
            return true;
    return false;
}

void factoryPresetsAreWellFormed()
{
    const auto presets = PresetManager::factoryPresets();
    CHECK (! presets.empty());

    for (auto& preset : presets)
    {
        CHECK (! preset.name.isEmpty());

        for (auto& [id, value] : preset.values)
        {
            CHECK (std::isfinite (value));
            CHECK (isKnownParamId (id));
        }

        Preset decoded;
        CHECK (PresetManager::fromJson (PresetManager::toJson (preset), decoded));
        CHECK (decoded.name == preset.name);
        CHECK (decoded.values.size() == preset.values.size());
        for (auto& [id, value] : preset.values)
        {
            const auto it = decoded.values.find (id);
            CHECK (it != decoded.values.end());
            if (it != decoded.values.end())
                CHECK (it->second == value); // lossless: same float bits after round-trip
        }
    }
}

void fromJsonIgnoresNothing()
{
    // The adapter filters unknown IDs at apply time; fromJson itself must preserve them.
    const auto json = PresetManager::toJson ({ "Extra", { { "gain", 1.0f }, { "not_a_param", 1.0f } } });

    Preset decoded;
    CHECK (PresetManager::fromJson (json, decoded));
    const auto it = decoded.values.find ("not_a_param");
    CHECK (it != decoded.values.end());
    if (it != decoded.values.end())
        CHECK (std::fabs (it->second - 1.0f) <= 1.0e-6f);
}
} // namespace

// run_PresetsTests in global namespace so TestMain.cpp can see it
int run_PresetsTests()
{
    jsonRoundTrip();
    rejectsMalformedJson();
    rejectsUnknownSchema();
    factoryPresetsAreWellFormed();
    fromJsonIgnoresNothing();
    return failures;
}
