#include "opportunistic_semaphore.hh"

#include "os/thread.hh"

using namespace os;

std::vector<OpportunisticBinarySemaphore*> OpportunisticBinarySemaphore::g_waiting_semaphores;

OpportunisticBinarySemaphore::OpportunisticBinarySemaphore(uint8_t initial_value)
    : m_value(initial_value)
{
}

OpportunisticBinarySemaphore::~OpportunisticBinarySemaphore()
{
    std::erase(g_waiting_semaphores, this);
}

void
OpportunisticBinarySemaphore::release() noexcept
{
    m_value.store(1, std::memory_order_release);

    auto now = os::GetTimeStamp();
    while (!m_waiting_threads.empty())
    {
        auto& entry = m_waiting_threads.front();

        if (entry.config.no_earlier_than.count() - now.count() > 0)
        {
            // Not yet time to wake up this thread
            m_pending_wakeups.push_back(entry);
            break;
        }
        m_waiting_threads.pop_front();
        os::AwakeThread(entry.thread);
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
    return do_try_acquire_no_suspend();
}


bool
OpportunisticBinarySemaphore::try_acquire_for(const WakeupConfiguration& config) noexcept
{
    if (config.no_earlier_than.count() > 0)
    {
        os::Sleep(config.no_earlier_than);
    }

    if (do_try_acquire_no_suspend())
    {
        return true;
    }
    // We need to wait
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

    return false;
}


bool OpportunisticBinarySemaphore::do_try_acquire_no_suspend() noexcept
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

    for (auto sem : g_waiting_semaphores)
    {
        for (auto& entry : sem->m_pending_wakeups)
        {
            os::AwakeThread(entry.thread);
        }
        sem->m_pending_wakeups.clear();
    }
}
