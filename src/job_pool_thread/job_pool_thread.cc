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
    for (auto thread : m_ready_threads)
    {
        thread->OnStartup();
    }
}

std::optional<milliseconds>
JobPoolThread::OnActivation()
{
    std::optional<milliseconds> out;

    auto ready = m_ready_threads;
    m_ready_threads.clear();

    for (auto thread : ready)
    {
        auto it = std::find_if(m_threads.begin(), m_threads.end(), [&](const auto& data) {
            return data.thread == thread;
        });
        if (it == m_threads.end())
        {
            // Removed
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

    return out;
}

void
JobPoolThread::AttachPooledThread(PooledThreadBase* thread)
{
    thread->m_job_pool_thread = this;

    m_threads.push_back({thread, nullptr});
    m_ready_threads.push_back(thread);
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
    DetachThreadFromLists(thread);
}

void
JobPoolThread::DetachThreadFromLists(PooledThreadBase* thread)
{
    m_ready_threads.erase(std::remove(m_ready_threads.begin(), m_ready_threads.end(), thread),
                          m_ready_threads.end());
    m_threads.erase(std::remove_if(m_threads.begin(),
                                   m_threads.end(),
                                   [&](const auto& data) { return data.thread == thread; }),
                    m_threads.end());
}


// The base class for a pooled thread

PooledThreadBase::PooledThreadBase()
    : m_timer_manager(*this)
{
}

PooledThreadBase::~PooledThreadBase()
{
    if (m_job_pool_thread)
    {
        m_job_pool_thread->RemoveThread(this);
    }
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

// From IEventNotifier
void
PooledThreadBase::Notify()
{
}

void
PooledThreadBase::NotifyFromIsr()
{
}
