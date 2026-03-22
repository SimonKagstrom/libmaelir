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
    , m_value(initial_value)
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
    m_value.store(1, std::memory_order_release);

    auto now = os::GetTimeStamp();
    while (!m_waiting_threads.empty())
    {
        auto& entry = m_waiting_threads.front();

        m_waiting_threads.pop_front();
        os::AwakeThread(entry.thread);

        g_scheduler->Schedule();
    }
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
     * - acquire will just call m_semaphore.acquire()
     * - try_acquire will just call m_semaphore.try_acquire()
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
    auto self = os::GetCurrentThread();
    if (config.no_earlier_than == 0ms)
    {
        if (config.wakeup_interval.earliest == 0ms)
        {
            return m_semaphore.try_acquire_for_ms(config.wakeup_interval.latest);
        }
        else
        {
            g_scheduler->AddPendingWakeup(self, m_semaphore_index, config);
            os::SuspendThread(os::GetCurrentThread());
            return m_semaphore.try_acquire();
        }
    }
    else
    {
        // Should wait until no_earlier_than
        // TODO
        return m_semaphore.try_acquire();
    }
}


bool
OpportunisticBinarySemaphore::TryAcquireNoSuspend()
{
    if (std::atomic_exchange_explicit(&m_value, 0, std::memory_order_acquire) == 1)
    {
        return true;
    }

    return false;
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
OpportunisticScheduler::AddPendingWakeup(ThreadHandle thread,
                                         uint8_t sem_index,
                                         const WakeupConfiguration& config)
{
    auto now = os::GetTimeStamp();
    WakeupConfiguration adjusted_config = config;

    adjusted_config.no_earlier_than += now;
    adjusted_config.wakeup_interval.earliest += now;
    adjusted_config.wakeup_interval.latest += now;

    m_pending_wakeup[sem_index].push_back({thread, adjusted_config});

    std::ranges::sort(m_pending_wakeup[sem_index], {}, [](const auto& entry) {
        return entry.config.wakeup_interval.latest;
    });

    m_semaphore.release();
}

void
OpportunisticScheduler::AddPendingTooEarly(ThreadHandle thread,
                                           uint8_t sem_index,
                                           const WakeupConfiguration& config)
{
}

std::optional<milliseconds>
OpportunisticScheduler::Schedule()
{
    auto now = os::GetTimeStamp();

    for (auto i = 0; i < m_pending_wakeup.size(); i++)
    {
        auto& pending = m_pending_wakeup[i];
        while (!pending.empty())
        {
            auto& entry = pending.front();
            if (entry.config.wakeup_interval.latest >= now)
            {
                pending.pop_front();
                os::AwakeThread(entry.thread);
            }
            else
            {
                break;
            }
        }
    }
    auto lowest = 0xffffffffms;
    std::optional<milliseconds> out = std::nullopt;

    for (auto i = 0; i < m_pending_wakeup.size(); i++)
    {
        auto& pending = m_pending_wakeup[i];
        if (!pending.empty())
        {
            auto& entry = pending.front();
            if (entry.config.wakeup_interval.latest < lowest)
            {
                lowest = entry.config.wakeup_interval.latest;
                out = lowest - now;
            }
        }
    }

    return out;
}
