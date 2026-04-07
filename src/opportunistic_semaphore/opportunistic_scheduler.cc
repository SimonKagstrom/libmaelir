#include "opportunistic_scheduler.hh"

#include "debug_assert.hh"

#include <algorithm>
#include <ranges>

using namespace os;

namespace os
{
OpportunisticScheduler* g_scheduler;
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
