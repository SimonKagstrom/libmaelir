#pragma once

#include "opportunistic_semaphore.hh"
#include "os/thread.hh"

// TODO: Replace with safe
#include <atomic>
#include <deque>
#include <mutex>
#include <unordered_set>
#include <vector>

class SchedulerFixture;

namespace os
{

constexpr auto kMaxSemaphores = 32;

class OpportunisticScheduler
{
public:
    friend class ::SchedulerFixture;
    friend class OpportunisticBinarySemaphore;

    OpportunisticScheduler(os::binary_semaphore& semaphore);
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

    void AddPendingEntry(ThreadHandle thread, uint8_t sem_index, const WakeupConfiguration& config);

    void AddEarlyEntry(ThreadHandle thread, uint8_t sem_index, const WakeupConfiguration& config);


    void RequestScheduleForSemaphore(uint8_t sem_index);
    void RequestSchedule();

    // Return the next wakeup time
    std::optional<milliseconds> Schedule();

private:
    os::binary_semaphore& m_semaphore;
    std::vector<OpportunisticBinarySemaphore*> m_semaphores;

    std::unordered_set<uint8_t> m_released_semaphores;

    // Threads where the low interval has not yet been reached
    std::deque<OpportunisticBinarySemaphore::WaitEntry> m_pending;
    // Threads which have not yet reached the allowed earliest time
    std::deque<OpportunisticBinarySemaphore::WaitEntry> m_too_early;


    std::array<std::deque<OpportunisticBinarySemaphore::WaitEntry>, kMaxSemaphores>
        m_too_early_per_semaphore;


    std::mutex m_mutex;
};

class OpportunisticSchedulerThread : public os::OsThread
{
public:
private:
    void Awake() final
    {
        m_semaphore.release();
    }
    void ThreadLoop() final;

    os::binary_semaphore m_semaphore {0};
    OpportunisticScheduler m_scheduler {m_semaphore};
};

} // namespace os
