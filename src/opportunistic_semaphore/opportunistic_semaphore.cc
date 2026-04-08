#include "opportunistic_semaphore.hh"
#include "opportunistic_scheduler.hh"

#include "debug_assert.hh"
#include "os/thread.hh"

using namespace os;

OpportunisticBinarySemaphore::OpportunisticBinarySemaphore(uint8_t initial_value)
    : m_semaphore_index(g_scheduler->AddSemaphore(this))
    , m_semaphore(initial_value)
{
}

OpportunisticBinarySemaphore::~OpportunisticBinarySemaphore()
{
    g_scheduler->RemoveSemaphore(this);
}

void
OpportunisticBinarySemaphore::release()
{
    m_semaphore.release();
    g_scheduler->RequestScheduleForSemaphore(m_semaphore_index);
}

// Return true if a higher prio task was awoken
bool
OpportunisticBinarySemaphore::release_from_isr()
{
    //    m_value.store(1, std::memory_order_release);

    // TODO:
    release();
    return false;
}

bool
OpportunisticBinarySemaphore::try_acquire_for(const WakeupConfiguration& config)
{
    /*
     * m_semaphore is a os::binary_semaphore
     *
     * - try_acquire_for:
     *   - if no_earlier_than == 0 && wakeup_interval == [X, X]
     *       return m_semaphore.try_acquire_for(X)
     *   - if no_earlier_than == 0 && wakeup_interval == [X, Y]
     *      - m_scheduler.AddPendingWaiter({thread, config})
     *      - Suspend
     *      - return m_semaphore.try_acquire()
     *
     *   - if no_earlier_than > 0:
     *      - m_scheduler.AddSuspendedThread(thread, config)
     *      - Mark as pending too early
     *      - Suspend
     *      - return try_acquire()
     *
     *
     * release:
     *   - if not pending too early:
     *     - m_semaphore.release()
     *   - else:
     *     - Remove mark pending too early
     *     - m_scheduler.ReleasePendingThreads()
     *     - m_semaphore.release()
     *
     *
     * scheduler:
     *   - Thread with the highest prio
     *   - Has it's own semaphore, at startup waits on events
     */
    debug_assert(config.wakeup_interval.earliest <= config.wakeup_interval.latest);
    //debug_assert(config.wakeup_interval.earliest > 0ms);

    auto self = os::GetCurrentThread();
    if (config.no_earlier_than == 0ms)
    {
        if (config.wakeup_interval.earliest == config.wakeup_interval.latest)
        {
            // Wait for a precise time - regular semaphore behavior
            auto out = m_semaphore.try_acquire_for_ms(config.wakeup_interval.latest);
            // Opportunistically wakeup
            g_scheduler->RequestSchedule();
            return out;
        }
        else
        {
            // Pending opportunistic wakeup
            g_scheduler->AddPendingEntry(self, m_semaphore_index, config);
            os::SuspendThread(self);
            return m_semaphore.try_acquire();
        }
    }
    else // Minimum time before trying to acquire
    {
        debug_assert(config.no_earlier_than <= config.wakeup_interval.earliest);

        if (config.wakeup_interval.earliest == config.wakeup_interval.latest)
        {
            // Wait for a precise time - regular semaphore behavior (after delay)
            os::Sleep(config.no_earlier_than);
            return m_semaphore.try_acquire_for_ms(config.wakeup_interval.latest);
        }
        else
        {
            g_scheduler->AddEarlyEntry(self, m_semaphore_index, config);
            os::SuspendThread(self);
            return m_semaphore.try_acquire();
        }
    }
}
