// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "IDspProcessor.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <vector>

namespace auralis
{
class DspPipeline
{
public:
    struct Slot
    {
        std::unique_ptr<IDspProcessor> proc;
        std::atomic<bool> enabled { true };
        Slot() = default;
        explicit Slot (std::unique_ptr<IDspProcessor> p) noexcept : proc (std::move (p)) {}
        Slot (Slot&& o) noexcept
            : proc (std::move (o.proc)), enabled (o.enabled.load()) {}
    };

    // Setup-only: must not be called once the audio thread runs.
    IDspProcessor* addProcessor (std::unique_ptr<IDspProcessor> p);
    void prepare (const ProcessContext& ctx);
    void process (AudioBufferView& buffer);
    void reset();
    void setEnabled (std::size_t index, bool enabled) noexcept;
    std::size_t size() const noexcept { return slots.size(); }

private:
    std::vector<Slot> slots; // never resized once prepare was called
};
}
