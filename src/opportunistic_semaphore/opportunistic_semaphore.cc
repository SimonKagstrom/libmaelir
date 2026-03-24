#include "opportunistic_semaphore.hh"

#include "debug_assert.hh"
#include "os/thread.hh"

using namespace os;

namespace
{

OpportunisticScheduler* g_scheduler;

} // namespace

OpportunisticBinarySemaphore::OpportunisticBinarySemaphore(uint8_t initial_value)
    : m_semaphore_index(g_scheduler->AddSemaphore(this))
    , m_semaphore(initial_value)
{
    g_scheduler->AddSemaphore(this);
}

OpportunisticBinarySemaphore::~OpportunisticBinarySemaphore()
{
    g_scheduler->RemoveSemaphore(this);
}

void
OpportunisticBinarySemaphore::release()
{
    m_semaphore.release();
    g_scheduler->RequestSchedule(m_semaphore_index);
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
    debug_assert(config.wakeup_interval.earliest > 0ms);

    auto self = os::GetCurrentThread();
    if (config.no_earlier_than == 0ms)
    {
        if (config.wakeup_interval.earliest == config.wakeup_interval.latest)
        {
            // Wait for a precise time - regular semaphore behavior
            return m_semaphore.try_acquire_for_ms(config.wakeup_interval.latest);
        }
        else
        {
            // Pending opportunistic wakeup
            g_scheduler->AddPendingEntry(self, m_semaphore_index, config);
            os::SuspendThread(os::GetCurrentThread());
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
            os::SuspendThread(os::GetCurrentThread());
            return m_semaphore.try_acquire();
        }
    }
}



OpportunisticScheduler::OpportunisticScheduler(os::binary_semaphore& semaphore)
    : m_semaphore(semaphore)
{
    g_scheduler = this;
}

OpportunisticScheduler::~OpportunisticScheduler()
{
    g_scheduler = nullptr;
}

void
OpportunisticScheduler::AddPendingEntry(ThreadHandle thread,
                                        uint8_t sem_index,
                                        const WakeupConfiguration& config)
{
    auto now = os::GetTimeStamp();
    WakeupConfiguration adjusted_config = config + now;

    m_pending.push_back({thread, adjusted_config});

    std::ranges::sort(
        m_pending, {}, [](const auto& entry) { return entry.config.wakeup_interval.latest; });

    m_semaphore.release();
}


void
OpportunisticScheduler::AddEarlyEntry(ThreadHandle thread,
                                      uint8_t sem_index,
                                      const WakeupConfiguration& config)
{
    auto now = os::GetTimeStamp();
    WakeupConfiguration adjusted_config = config + now;

    m_too_early.push_back({thread, adjusted_config});

    m_semaphore.release();
}

void
OpportunisticScheduler::RequestSchedule(uint8_t sem_index)
{
    // TODO: Take the semaphore index into account
    m_semaphore.release();
}

std::optional<milliseconds>
OpportunisticScheduler::Schedule()
{
    std::optional<milliseconds> out;
    auto now = os::GetTimeStamp();

    while (!m_too_early.empty())
    {
        auto& entry = m_too_early.front();

        if (entry.config.no_earlier_than > now)
        {
            m_too_early.pop_front();

            if (now > entry.config.wakeup_interval.latest)
            {
                os::AwakeThread(entry.thread);
            }
            else
            {
                m_pending.push_back(entry);
                // TODO: Not good
                std::ranges::sort(m_pending, {}, [](const auto& entry) {
                    return entry.config.wakeup_interval.latest;
                });
            }
        }
    }

    while (!m_pending.empty())
    {
        out = m_pending.front().config.wakeup_interval.latest;

        if (now < m_pending.front().config.wakeup_interval.earliest)
        {
            break;
        }
        const auto& entry = m_pending.front();
        m_ready_for_wakeup.push_back(entry);
        m_pending.pop_front();
    }

    for (auto& entry : m_ready_for_wakeup)
    {
        os::AwakeThread(entry.thread);
    }
    m_ready_for_wakeup.clear();

    return out;
}
