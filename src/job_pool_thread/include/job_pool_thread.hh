#pragma once

#include "base_thread.hh"
#include "pooled_thread_base.hh"

#include <vector>

class JobPoolThread : public os::BaseThread
{
public:
    friend class PooledThreadBase;

    JobPoolThread();

    void OnStartup() final;
    std::optional<milliseconds> OnActivation() final;

    void AttachPooledThread(PooledThreadBase* thread);

private:
    struct SleepThread
    {
        PooledThreadBase* thread;
        milliseconds wakeup_time;
    };

    void Awake(PooledThreadBase* thread);
    // On thread exit
    void RemoveThread(PooledThreadBase* thread);

    void DetachThreadFromLists(PooledThreadBase* thread);

    std::vector<PooledThreadBase*> m_wait_for_event_threads;
    std::vector<PooledThreadBase*> m_ready_threads;
    std::vector<SleepThread> m_sleeping_threads; // Sorted by wakeup time

    std::vector<PooledThreadBase*> m_removed_threads;
};
