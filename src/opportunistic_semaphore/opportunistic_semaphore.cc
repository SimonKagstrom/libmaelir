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
    debug_assert(config.wakeup_interval.earliest > 0ms);

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
    // Otherwise this should not be used
    debug_assert(config.no_earlier_than == 0ms);
    debug_assert(config.wakeup_interval.earliest < config.wakeup_interval.latest);

    std::scoped_lock lock(m_mutex);
    auto now = os::GetTimeStamp();
    WakeupConfiguration adjusted_config = config + now;

    m_pending.push_back({thread, adjusted_config, sem_index});

    std::ranges::sort(
        m_pending, {}, [](const auto& entry) { return entry.config.wakeup_interval.latest; });

    m_semaphore.release();
}


void
OpportunisticScheduler::AddEarlyEntry(ThreadHandle thread,
                                      uint8_t sem_index,
                                      const WakeupConfiguration& config)
{
    debug_assert(config.no_earlier_than > 0ms);

    std::scoped_lock lock(m_mutex);
    auto now = os::GetTimeStamp();
    WakeupConfiguration adjusted_config = config + now;

    m_too_early.push_back({thread, adjusted_config, sem_index});
    m_too_early_per_semaphore[sem_index].push_back({thread, adjusted_config, sem_index});

    m_semaphore.release();
}

void
OpportunisticScheduler::RequestSchedule()
{
    m_semaphore.release();
}

void
OpportunisticScheduler::RequestScheduleForSemaphore(uint8_t sem_index)
{
    std::scoped_lock lock(m_mutex);
    m_released_semaphores.insert(sem_index);
    RequestSchedule();
}

std::optional<milliseconds>
OpportunisticScheduler::Schedule()
{
    std::scoped_lock lock(m_mutex);


    milliseconds out = 0xffffffffms;
    auto now = os::GetTimeStamp();

    std::vector<ThreadHandle> ready_for_wakeup;

    for (auto released : m_released_semaphores)
    {
        /*
         * Move all pending for this thread to ready-for-wakeup (before too-early has been evaluated).
         *
         * This might have to be reconsidered, if the semaphore is released multiple times before the
         * too-early time has passed.
         */
        auto pending = std::ranges::remove_if(
            m_pending, [&](const auto& e) { return e.sem_index == released; });
        for (auto& entry : pending)
        {
            ready_for_wakeup.push_back(entry.thread);
        }
        m_pending.erase(pending.begin(), pending.end());

        for (auto& entry : m_too_early_per_semaphore[released])
        {
            out = entry.config.no_earlier_than;
            entry.config.wakeup_interval.earliest = entry.config.no_earlier_than;
            entry.config.wakeup_interval.latest = entry.config.no_earlier_than;

            m_pending.push_back(entry);
        }
        auto it = std::ranges::remove_if(m_too_early,
                                         [&](const auto& e) { return e.sem_index == released; });
        m_too_early.erase(it.begin(), it.end());
        m_too_early_per_semaphore[released].clear();
    }
    m_released_semaphores.clear();

    while (!m_too_early.empty())
    {
        auto& entry = m_too_early.front();

        if (entry.config.no_earlier_than <= now)
        {
            if (now > entry.config.wakeup_interval.latest)
            {
                os::AwakeThread(entry.thread);
            }
            else
            {
                m_pending.push_back(entry);
            }

            m_too_early.pop_front();
        }
        else
        {
            break;
        }
    }

    for (auto& entry : m_too_early)
    {
        out = std::min(out, entry.config.wakeup_interval.latest);
    }

    m_released_semaphores.clear();
    // TODO: Not good
    std::ranges::sort(
        m_pending, {}, [](const auto& entry) { return entry.config.wakeup_interval.latest; });

    while (!m_pending.empty())
    {
        out = m_pending.front().config.wakeup_interval.latest;

        if (now < m_pending.front().config.wakeup_interval.earliest)
        {
            break;
        }
        const auto& entry = m_pending.front();
        ready_for_wakeup.push_back(entry.thread);
        m_pending.pop_front();
    }

    for (auto& entry : ready_for_wakeup)
    {
        os::AwakeThread(entry);
    }
    ready_for_wakeup.clear();

    if (m_pending.empty() && m_too_early.empty())
    {
        return std::nullopt;
    }

    if (out == 0xffffffffms)
    {
        return std::nullopt;
    }

    return out;
}
