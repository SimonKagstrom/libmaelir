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

    void AttachPooledThread(std::unique_ptr<PooledThreadBase> thread);

private:
    struct ThreadData
    {
        std::unique_ptr<PooledThreadBase> thread;
        os::TimerHandle wakeup_handle;
    };

    void Awake(PooledThreadBase* thread);
    // On thread exit
    void RemoveThread(PooledThreadBase* thread);

    void DetachThreadFromLists(PooledThreadBase* thread);

    std::vector<ThreadData> m_threads;
    std::vector<PooledThreadBase*> m_ready_threads;

    std::vector<PooledThreadBase*> m_removed_threads;
};
