#pragma once

#include "os/thread.hh"
#include "semaphore.hh"
#include "time.hh"
#include "timer_manager.hh"

#include <atomic>
#include <optional>

class ThreadFixture;

namespace os
{

class BaseThread : public OsThread
{
public:
    // For unit tests
    friend class ::ThreadFixture;

    BaseThread()
        : m_timer_manager(m_semaphore)
    {
    }

    virtual ~BaseThread() override
    {
        Stop();
    }

    void Awake() final
    {
        m_semaphore.release();
    }

protected:
    /// @brief the thread has been awoken
    virtual std::optional<milliseconds> OnActivation() = 0;

    /**
     * @brief Start a timer in this thread.
     *
     * @param timeout the timeout of the timer
     * @param on_timeout the function to call on timeout
     * @return a handle to the started timer.
     */
    TimerHandle StartTimer(
        milliseconds timeout, std::function<std::optional<milliseconds>()> on_timeout = []() {
            return std::optional<milliseconds>();
        })
    {
        return m_timer_manager.StartTimer(timeout, on_timeout);
    }

    /**
     * @brief Defer the execution of a function.
     *
     * @param deferred_job the function to execute
     *
     * @return a handle to the started job
     */
    TimerHandle Defer(std::function<std::optional<milliseconds>()> deferred_job)
    {
        return m_timer_manager.StartTimer(0ms, deferred_job);
    }

    os::binary_semaphore& GetSemaphore()
    {
        return m_semaphore;
    }

    os::TimerManager& GetTimerManager()
    {
        return m_timer_manager;
    }

    // The thread has just started
    virtual void OnStartup()
    {
    }

private:
    struct Impl;

    std::optional<milliseconds> SelectWakeup(auto a, auto b) const
    {
        if (a && b)
        {
            return std::min(*a, *b);
        }
        else if (a)
        {
            return *a;
        }
        else if (b)
        {
            return *b;
        }

        // Unreachable
        return std::nullopt;
    }

    // Accessible for unit tests
    std::optional<milliseconds> RunLoop()
    {
        auto thread_wakeup = OnActivation();
        auto timer_expiration = m_timer_manager.Expire();

        return SelectWakeup(thread_wakeup, timer_expiration);
    }

    void ThreadLoop() final
    {
        OnStartup();

        while (IsRunning())
        {
            auto time = RunLoop();

            if (time)
            {
                m_semaphore.try_acquire_for(*time);
            }
            else
            {
                m_semaphore.acquire();
            }
        }
    }

    binary_semaphore m_semaphore {0};
    Impl* m_impl {nullptr}; // Raw pointer to allow forward declaration
    TimerManager m_timer_manager;
};

} // namespace os
