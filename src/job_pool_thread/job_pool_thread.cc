#include "job_pool_thread.hh"

#include "debug_assert.hh"

#include <ranges>

// Contains the implementation of both JobPoolThread and PooledThreadBase
JobPoolThread::JobPoolThread()
{
}

void
JobPoolThread::OnStartup()
{
    for (auto thread : m_removed_threads)
    {
        DetachThreadFromLists(thread);
    }
    m_removed_threads.clear();

    for (auto thread : m_ready_threads)
    {
        thread->OnStartup();
    }
}

std::optional<milliseconds>
JobPoolThread::OnActivation()
{
    std::optional<milliseconds> out;

    for (auto thread : m_removed_threads)
    {
        DetachThreadFromLists(thread);
    }
    m_removed_threads.clear();

    auto ready = m_ready_threads;
    m_ready_threads.clear();

    for (auto thread : ready)
    {
        auto it = std::find_if(m_threads.begin(), m_threads.end(), [&](const auto& data) {
            return data.thread.get() == thread;
        });
        if (it == m_threads.end())
        {
            // Removed
            continue;
        }
        if (thread->m_detached)
        {
            // Will be removed
            continue;
        }
        auto result = thread->RunLoop();

        if (result)
        {
            if (result == 0ms)
            {
                // Ready again
                m_ready_threads.push_back(thread);
                out = 0ms;
            }
            else if (result.has_value())
            {
                it->wakeup_handle = StartTimer(*result, [this, thread]() {
                    Awake(thread);

                    return std::nullopt;
                });
            }
        }
    }

    for (auto thread : m_removed_threads)
    {
        DetachThreadFromLists(thread);
    }
    m_removed_threads.clear();

    return out;
}

void
JobPoolThread::AttachPooledThread(std::unique_ptr<PooledThreadBase> thread)
{
    thread->m_job_pool_thread = this;

    m_ready_threads.push_back(thread.get());
    m_threads.push_back({std::move(thread), nullptr});
}

void
JobPoolThread::Awake(PooledThreadBase* thread)
{
    m_ready_threads.push_back(thread);

    BaseThread::Awake();
}

void
JobPoolThread::RemoveThread(PooledThreadBase* thread)
{
    m_removed_threads.push_back(thread);
}

void
JobPoolThread::DetachThreadFromLists(PooledThreadBase* thread)
{
    m_ready_threads.erase(std::remove(m_ready_threads.begin(), m_ready_threads.end(), thread),
                          m_ready_threads.end());
    m_threads.erase(std::remove_if(m_threads.begin(),
                                   m_threads.end(),
                                   [&](const auto& data) { return data.thread.get() == thread; }),
                    m_threads.end());
}


// The base class for a pooled thread

PooledThreadBase::PooledThreadBase()
    : m_timer_manager(*this)
{
}

PooledThreadBase::~PooledThreadBase()
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
}

// Context: Interrupt
void
PooledThreadBase::NotifyFromIsr()
{
}
