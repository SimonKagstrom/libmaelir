#include "job_pool_thread.hh"

#include "debug_assert.hh"

#include <ranges>

// Contains the implementation of both JobPoolThread and PooledThreadBase
JobPoolThread::JobPoolThread()
{
    m_free_thread_ids.set();
}

void
JobPoolThread::OnStartup()
{
    // If something was removed before start
    CleanupRemovedThreads();

    for (auto thread : m_ready_threads)
    {
        thread->OnStartup();
    }
}

std::optional<milliseconds>
JobPoolThread::OnActivation()
{
    std::optional<milliseconds> out;

    // Basically if a thread was removed in OnStartup() - but be sure
    CleanupRemovedThreads();

    auto woken = m_ready_mask.load();
    etl::bitset<kMaxThreads, uint32_t> woken_bits {woken};

    // If something was added to the mask after the load above, it will be handled the next round
    for (auto index = woken_bits.find_first(true); index != woken_bits.npos;
         index = woken_bits.find_next(true, index + 1))
    {
        auto thread = m_threads[index].thread.get();

        if (!thread)
        {
            // This should be very unlikely
            printf("Thread %u has been removed (?), not making ready\n", (unsigned)index);
        }
        else
        {
            m_ready_threads.push_back(thread);
        }

        m_ready_mask &= ~(1 << index);
    }

    auto ready = m_ready_threads;
    m_ready_threads.clear();

    for (auto thread : ready)
    {
        if (thread->m_detached)
        {
            // Will be removed
            continue;
        }
        auto result = thread->RunLoop();

        if (result)
        {
            // Timeout value (or 0ms, which is also handled the same way)
            m_threads[thread->m_thread_id].wakeup_handle = StartTimer(*result, [this, thread]() {
                Awake(thread);

                return std::nullopt;
            });
        }
    }

    // Cleanup threads removed during activation
    CleanupRemovedThreads();

    return out;
}

void
JobPoolThread::AttachPooledThread(std::unique_ptr<PooledThreadBase> thread)
{
    auto index = m_free_thread_ids.find_first(true);
    assert(index != m_free_thread_ids.npos);

    m_free_thread_ids[index] = false;

    thread->m_thread_id = static_cast<uint8_t>(index);
    thread->m_job_pool_thread = this;

    m_ready_threads.push_back(thread.get());
    m_threads[index] = {std::move(thread), nullptr};
}

// Context: Another thread, or even an interrupt
void
JobPoolThread::Awake(PooledThreadBase* thread)
{
    debug_assert(thread->m_thread_id != 255);
    m_ready_mask |= (1 << thread->m_thread_id);

    BaseThread::Awake();
}

void
JobPoolThread::RemoveThread(PooledThreadBase* thread)
{
    m_removed_threads.push_back(thread);
}

void
JobPoolThread::CleanupRemovedThreads()
{
    for (auto thread : m_removed_threads)
    {
        DetachThreadFromLists(thread);
    }
    m_removed_threads.clear();
}

void
JobPoolThread::DetachThreadFromLists(PooledThreadBase* thread)
{
    m_ready_threads.erase(std::remove(m_ready_threads.begin(), m_ready_threads.end(), thread),
                          m_ready_threads.end());
    auto index = thread->m_thread_id;

    debug_assert(index != 255);

    m_threads[index] = {nullptr, nullptr};
    m_free_thread_ids[index] = true;
}


// The base class for a pooled thread

PooledThreadBase::PooledThreadBase()
    : m_timer_manager(*this)
{
}

void
PooledThreadBase::Awake()
{
    debug_assert(m_job_pool_thread);

    m_job_pool_thread->Awake(this);
}

std::optional<milliseconds>
PooledThreadBase::RunLoop()
{
    auto thread_wakeup = OnActivation();
    auto timer_expiration = m_timer_manager.Expire();

    return os::SelectWakeup(thread_wakeup, timer_expiration);
}

void
PooledThreadBase::Stop()
{
    debug_assert(m_job_pool_thread);

    m_detached = true;
    m_job_pool_thread->RemoveThread(this);
}

// Context: Another thread potentially
void
PooledThreadBase::Notify()
{
    Awake();
}

// Context: Interrupt
void
PooledThreadBase::NotifyFromIsr()
{
    Awake();
}
