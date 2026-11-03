// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "DspTypes.h"

namespace auralis
{
class IDspProcessor
{
public:
    virtual ~IDspProcessor() = default;
    virtual void prepare (const ProcessContext& ctx) = 0;
    virtual void process (AudioBufferView& buffer) = 0;
    virtual void reset() = 0;
};
}
