// SPDX-License-Identifier: AGPL-3.0-only

#include "DspPipeline.h"

#include <cmath>

namespace auralis
{
IDspProcessor* DspPipeline::addProcessor (std::unique_ptr<IDspProcessor> p)
{
    slots.push_back (Slot (std::move (p)));
    return slots.back().proc.get();
}

void DspPipeline::prepare (const ProcessContext& ctx)
{
    for (auto& slot : slots)
        if (slot.proc != nullptr)
            slot.proc->prepare (ctx); // disabled slots stay prepared for instant re-enable
}

void DspPipeline::process (AudioBufferView& buffer)
{
    // contract §3: hosts can deliver NaN/inf; sanitize once here so no processor state is poisoned
    for (int c = 0; c < buffer.numChannels(); ++c)
    {
        float* ch = buffer.channel (c);
        for (int i = 0; i < buffer.numFrames(); ++i)
            if (! std::isfinite (ch[i]))
                ch[i] = 0.0f;
    }

    for (auto& slot : slots)
        if (slot.enabled.load() && slot.proc != nullptr)
            slot.proc->process (buffer);
}

void DspPipeline::reset()
{
    for (auto& slot : slots)
        if (slot.proc != nullptr)
            slot.proc->reset();
}

void DspPipeline::setEnabled (std::size_t index, bool enabled) noexcept
{
    if (index < slots.size())
        slots[index].enabled.store (enabled);
}
}
