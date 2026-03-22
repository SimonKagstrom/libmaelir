#include "opportunistic_semaphore.hh"

#include "debug_assert.hh"
#include "os/thread.hh"

using namespace os;

namespace
{

OpportunisticScheduler* g_scheduler;


std::vector<OpportunisticBinarySemaphore*> g_waiting_semaphores;

} // namespace

OpportunisticBinarySemaphore::OpportunisticBinarySemaphore(uint8_t initial_value)
    : m_value(initial_value)
{
}

OpportunisticBinarySemaphore::~OpportunisticBinarySemaphore()
{
    // TODO FIXME!
    std::erase(g_waiting_semaphores, this);
}

void
OpportunisticBinarySemaphore::release() noexcept
{
    debug_assert(g_scheduler);
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
OpportunisticBinarySemaphore::release_from_isr() noexcept
{
    //    m_value.store(1, std::memory_order_release);

    // TODO:
    release();
    return false;
}

bool
OpportunisticBinarySemaphore::try_acquire() noexcept
{
    return TryAcquireNoSuspend();
}


bool
OpportunisticBinarySemaphore::try_acquire_for(const WakeupConfiguration& config) noexcept
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
     *      - return m_semaphore.try_acquire_for(Y)
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
     */

    if (config.no_earlier_than > 0ms)
    {
        SuspendForTooEarly(config);

        // Back after wakeup (at the latest after no_later_than)
        return TryAcquireNoSuspend();
    }

    if (TryAcquireNoSuspend())
    {
        return true;
    }
    SuspendForNoLaterThan(config);

    return TryAcquireNoSuspend();
}


void
OpportunisticBinarySemaphore::SuspendForTooEarly(const WakeupConfiguration& config) noexcept
{
    debug_assert(g_scheduler);

    g_scheduler->AddSuspendedThread(os::GetCurrentThread(), config);

    os::SuspendThread(os::GetCurrentThread());
}

void
OpportunisticBinarySemaphore::SuspendForNoLaterThan(const WakeupConfiguration& config) noexcept
{
    auto self = os::GetCurrentThread();

    auto now = os::GetTimeStamp();
    m_waiting_threads.push_back({self, {config.no_earlier_than + now, config.no_later_than + now}});
    std::ranges::sort(
        m_waiting_threads, {}, [](const auto& entry) { return entry.config.no_later_than; });

    if (config.no_later_than.count() > 0)
    {
        os::TriggerWakeup(config.no_later_than);
    }

    g_waiting_semaphores.push_back(this);
    os::SuspendThread(self);
}

bool
OpportunisticBinarySemaphore::TryAcquireNoSuspend() noexcept
{
    if (std::atomic_exchange_explicit(&m_value, 0, std::memory_order_acquire) == 1)
    {
        return true;
    }

    return false;
}


void
OpportunisticBinarySemaphore::RunScheduler()
{
    auto now = os::GetTimeStamp();

    for (auto sem : g_waiting_semaphores)
    {
        while (!sem->m_waiting_threads.empty())
        {
            auto& entry = sem->m_waiting_threads.front();

            if (entry.config.no_later_than.count() - now.count() > 0)
            {
                // Not yet time to wake up this thread
                break;
            }
            sem->m_waiting_threads.pop_front();
            os::AwakeThread(entry.thread);
        }
    }
}


OpportunisticScheduler::OpportunisticScheduler()
{
    g_scheduler = this;
}

OpportunisticScheduler::~OpportunisticScheduler()
{
    g_scheduler = nullptr;
}

void
OpportunisticScheduler::Schedule() noexcept
{
    auto now = os::GetTimeStamp();
}

void
OpportunisticScheduler::AddSuspendedThread(ThreadHandle thread, const WakeupConfiguration& config)
{
    auto now = os::GetTimeStamp();

    if (config.no_earlier_than > 0ms)
    {
        debug_assert(config.no_later_than >= config.no_earlier_than);

        m_pending_too_early.push_back(
            {thread, {config.no_earlier_than + now, config.no_later_than + now}});
    }
    auto next_wakeup = config.no_later_than + now;
    if (m_next_wakeup_time > next_wakeup)
    {
        m_next_wakeup_time = next_wakeup;
        os::TriggerWakeup(next_wakeup);
    }
}
