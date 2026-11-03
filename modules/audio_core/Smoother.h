// SPDX-License-Identifier: AGPL-3.0-only
// One-pole smoother for control values entering the audio path.

#pragma once

#include <cmath>

namespace auralis
{
class Smoother
{
public:
    void prepare (double sampleRate, float timeMs) noexcept
    {
        coeff = 1.0f - std::exp (-1000.0f / (timeMs * (float) sampleRate));
        current = target;
    }

    void setTarget (float t) noexcept { target = t; }
    void snap() noexcept { current = target; }
    float next() noexcept { current += coeff * (target - current); return current; }
    float value() const noexcept { return current; }

private:
    float coeff = 1.0f;
    float current = 0.0f;
    float target = 0.0f;
};
}
