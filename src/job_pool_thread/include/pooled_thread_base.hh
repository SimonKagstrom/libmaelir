#pragma once

#include "event_notifier.hh"
#include "time.hh"

#include <atomic>
#include <optional>
#include <variant>

class JobPoolThread;

class PooledThreadBase : public IEventNotifier
{
public:
    friend class JobPoolThread;

    PooledThreadBase();
    virtual ~PooledThreadBase();

    virtual void OnStartup()
    {
    }

    virtual std::optional<milliseconds> OnActivation() = 0;

    auto& GetNotifier()
    {
        return *this;
    }

    os::TimerManager& GetTimerManager()
    {
        return m_timer_manager;
    }

    void Awake();

private:
    // Used by the JobPoolThread. Either nullopt -> wait forever, 0ms -> run again or > 0ms -> wait for the specified duration
    std::optional<milliseconds> RunLoop();

    // From IEventNotifier
    void Notify() final;
    void NotifyFromIsr() final;

    os::TimerManager m_timer_manager;
    JobPoolThread* m_job_pool_thread {nullptr};

    std::atomic<bool> m_awake {false};
};
