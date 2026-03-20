#pragma once

#include "semaphore.hh"

namespace os
{

struct WakeupConfiguration
{
    milliseconds no_earlier_than {0ms};
    milliseconds no_later_than {0ms};

    WakeupConfiguration &NoLaterThan(milliseconds time)
    {
        no_later_than = time;
        return *this;
    }

    WakeupConfiguration &NoEarlierThan(milliseconds time)
    {
        no_earlier_than = time;
        return *this;
    }
};

WakeupConfiguration OnEvent()
{
    return {0ms, 0ms};
}

WakeupConfiguration EarliestAfter(milliseconds time)
{
    return {time, milliseconds::max()};
}

class OpportunisticBinarySemaphore
{
public:
    OpportunisticBinarySemaphore(uint8_t initial_value);

    void release() noexcept;

    // Return true if a higher prio task was awoken
    bool release_from_isr() noexcept;

    void acquire() noexcept
    {
        try_acquire_for(OnEvent());
    }

    bool try_acquire() noexcept
    {
        return try_acquire_for(OnEvent());
    }

    bool try_acquire_for(const WakeupConfiguration& config) noexcept;
};

} // namespace os