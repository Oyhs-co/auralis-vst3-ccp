// SPDX-License-Identifier: AGPL-3.0-only

#include "DspPipeline.h"

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
