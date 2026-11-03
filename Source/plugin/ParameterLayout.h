// SPDX-License-Identifier: AGPL-3.0-only
// Auralis parameter layout: every host-exposed parameter, with stable string IDs.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

juce::AudioProcessorValueTreeState::ParameterLayout createAuralisParameterLayout();