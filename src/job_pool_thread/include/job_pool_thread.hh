#pragma once

#include "base_thread.hh"
#include "pooled_thread_base.hh"

#include <etl/bitset.h>
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
    static constexpr auto kMaxThreads = 32;

    struct ThreadData
    {
        std::unique_ptr<PooledThreadBase> thread;
        os::TimerHandle wakeup_handle;
    };

    void WakeupPooledThread(PooledThreadBase* thread);
    // On thread exit
    void RemoveThread(PooledThreadBase* thread);

    void CleanupRemovedThreads();

    void DetachThreadFromLists(PooledThreadBase* thread);

    etl::bitset<kMaxThreads, uint32_t> m_free_thread_ids;
    std::array<ThreadData, kMaxThreads> m_threads;
    std::vector<PooledThreadBase*> m_ready_threads;

    std::vector<PooledThreadBase*> m_removed_threads;

    std::atomic<uint32_t> m_ready_mask;
};
