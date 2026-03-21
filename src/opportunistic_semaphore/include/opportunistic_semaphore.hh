#pragma once

#include "os/thread.hh"
#include "semaphore.hh"

// TODO: Replace with safe
#include <atomic>
#include <deque>

class SchedulerFixture;

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
    friend class ::SchedulerFixture;

    explicit OpportunisticBinarySemaphore(uint8_t initial_value);
    ~OpportunisticBinarySemaphore();

    void release() noexcept;

    // Return true if a higher prio task was awoken
    bool release_from_isr() noexcept;

    void acquire() noexcept
    {
        try_acquire_for(OnEvent());
    }

    void acquire(const WakeupConfiguration& config) noexcept
    {
        if (config.no_earlier_than.count() > 0)
        {
            os::Sleep(config.no_earlier_than);
        }
        acquire();
    }

    bool try_acquire() noexcept;

    bool try_acquire_for(const WakeupConfiguration& config) noexcept;

    // TODO: Privatize, but maybe available for unit tests
    struct Entry
    {
        os::ThreadHandle thread;
        WakeupConfiguration config;
    };

    struct WaitEntry
    {
        Entry entry;
        OpportunisticBinarySemaphore* sem;
    };

protected:
    // Protected for unit tests
    void SuspendForTooEarly(const WakeupConfiguration& config) noexcept;
    void SuspendForNoLaterThan(const WakeupConfiguration& config) noexcept;

    bool TryAcquireNoSuspend() noexcept;

private:
    static void RunScheduler();

    std::atomic<uint8_t> m_value;
    std::deque<Entry> m_waiting_threads;
    std::vector<OpportunisticBinarySemaphore::Entry> m_pending_wakeups;
};

} // namespace os
