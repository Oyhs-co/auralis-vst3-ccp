// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "IDspProcessor.h"

namespace auralis
{
class GainProcessor : public IDspProcessor
{
public:
    void prepare (const ProcessContext& ctx) override;
    void process (AudioBufferView& buffer) override;
    void reset() override;

    // Control methods: call once per block, never inside the sample loop.
    void setGainDb (float db) noexcept;
    void setBypass (bool b) noexcept;

private:
    float coeff = 0.0f;
    float current = 1.0f;
    float target = 1.0f;
    bool bypass = false;
};
}
