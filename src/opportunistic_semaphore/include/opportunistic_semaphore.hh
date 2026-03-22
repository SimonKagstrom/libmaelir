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

    struct
    {
        milliseconds earliest;
        milliseconds latest;
    } wakeup_interval {0ms, 0ms};

    WakeupConfiguration& NoLaterThan(milliseconds time)
    {
        wakeup_interval.latest = time;
        return *this;
    }

    WakeupConfiguration& NoEarlierThan(milliseconds time)
    {
        no_earlier_than = time;
        wakeup_interval.earliest = time;
        return *this;
    }
};

static inline WakeupConfiguration
OnEvent()
{
    // Default
    return {};
}

static inline WakeupConfiguration
EarliestAfter(milliseconds time)
{
    return {time, {time, time}};
}

static inline WakeupConfiguration
LatestAfter(milliseconds time)
{
    return {0ms, {0ms, time}};
}

class OpportunisticScheduler;

class OpportunisticBinarySemaphore
{
public:
    friend class ::SchedulerFixture;
    friend class OpportunisticScheduler;

    explicit OpportunisticBinarySemaphore(uint8_t initial_value);
    ~OpportunisticBinarySemaphore();

    void release();

    // Return true if a higher prio task was awoken
    bool release_from_isr();

    void acquire()
    {
        m_semaphore.acquire();
    }

    void acquire(const WakeupConfiguration& config)
    {
        if (config.no_earlier_than.count() > 0)
        {
            os::Sleep(config.no_earlier_than);
        }
        acquire();
    }

    bool try_acquire()
    {
        return m_semaphore.try_acquire();
    }

    bool try_acquire_for(const WakeupConfiguration& config);

    // TODO: Privatize, but maybe available for unit tests
    struct Entry
    {
        os::ThreadHandle thread;
        WakeupConfiguration config;
    };

    struct WaitEntry
    {
        Entry entry;
        uint8_t sem_index;
    };

protected:
    // Protected for unit tests
    void SuspendForTooEarly(const WakeupConfiguration& config);
    void SuspendForNoLaterThan(const WakeupConfiguration& config);

    bool TryAcquireNoSuspend();

private:
    const uint8_t m_semaphore_index;
    os::binary_semaphore m_semaphore;
    std::atomic<uint8_t> m_value;
    std::deque<Entry> m_waiting_threads;
};


class OpportunisticScheduler
{
public:
    friend class ::SchedulerFixture;
    friend class OpportunisticBinarySemaphore;

    OpportunisticScheduler(os::binary_semaphore &semaphore);
    ~OpportunisticScheduler();

    uint8_t AddSemaphore(OpportunisticBinarySemaphore* sem)
    {
        m_semaphores.push_back(sem);
        // TODO: Wrong when something gets removed...
        return m_semaphores.size() - 1;
    }

    void RemoveSemaphore(OpportunisticBinarySemaphore* sem)
    {
        m_semaphores.erase(std::remove(m_semaphores.begin(), m_semaphores.end(), sem),
                           m_semaphores.end());
    }

    void
    AddPendingWakeup(ThreadHandle thread, uint8_t sem_index, const WakeupConfiguration& config);
    void
    AddPendingTooEarly(ThreadHandle thread, uint8_t sem_index, const WakeupConfiguration& config);

    // Return the next wakeup time
    std::optional<milliseconds> Schedule();

private:
    void AddSuspendedThread(ThreadHandle thread, const WakeupConfiguration& config);
    void AddPendingWaiter(const OpportunisticBinarySemaphore::Entry& entry);

    os::binary_semaphore &m_semaphore;
    std::vector<OpportunisticBinarySemaphore*> m_semaphores;

    std::array<std::deque<OpportunisticBinarySemaphore::Entry>, 32> m_pending_wakeup;
    std::array<std::deque<OpportunisticBinarySemaphore::Entry>, 32> m_pending_too_early;
};

} // namespace os
