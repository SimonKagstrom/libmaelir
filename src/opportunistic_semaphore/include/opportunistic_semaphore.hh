#pragma once

#include "os/thread.hh"
#include "semaphore.hh"

// TODO: Replace with safe
#include <atomic>
#include <deque>
namespace os
{

struct WakeupConfiguration
{
    milliseconds no_earlier_than {0ms};
    milliseconds no_later_than {0ms};

    WakeupConfiguration& NoLaterThan(milliseconds time)
    {
        no_later_than = time;
        return *this;
    }

    WakeupConfiguration& NoEarlierThan(milliseconds time)
    {
        no_earlier_than = time;
        return *this;
    }
};

static inline WakeupConfiguration
OnEvent()
{
    return {0ms, 0ms};
}

static inline WakeupConfiguration
EarliestAfter(milliseconds time)
{
    return {time, time};
}

static inline WakeupConfiguration
LatestAfter(milliseconds time)
{
    return {0ms, time};
}

class OpportunisticBinarySemaphore
{
public:
    explicit OpportunisticBinarySemaphore(uint8_t initial_value);
    ~OpportunisticBinarySemaphore();

    void release() noexcept;

    // Return true if a higher prio task was awoken
    bool release_from_isr() noexcept;

    void acquire() noexcept
    {
        try_acquire_for(OnEvent());
    }

    bool try_acquire() noexcept;

    bool try_acquire_for(const WakeupConfiguration& config) noexcept;


    static void RunScheduler();

private:
    struct Entry
    {
        os::ThreadHandle thread;
        WakeupConfiguration config;
    };

    std::atomic<uint8_t> m_value;
    std::deque<Entry> m_waiting_threads;

    static std::vector<OpportunisticBinarySemaphore*> g_waiting_semaphores;
};

} // namespace os
