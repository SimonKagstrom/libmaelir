#pragma once

#include "os_implementation.hh"
#include "semaphore.hh"
#include "thread_parameters.hh"

#include <atomic>
#include <cstdint>
#include <optional>

class ThreadFixture;

namespace os
{

class OsThread
{
public:
    // For unit tests
    friend class ::ThreadFixture;

    virtual ~OsThread() = default;

    virtual void Awake() = 0;

    void Stop()
    {
        if (m_running)
        {
            m_running = false;
            Awake();
            os::detail::WaitThreadExit(m_self);
        }
    }

    /**
     * @brief Start the thread
     *
     * @param name the name of the thread
     * @param core the core to pin the thread to
     * @param priority the thread priority (strict)
     * @param stack_size the stack size of the thread
     */
    void Start(const char* name, ThreadCore core, ThreadPriority priority, uint32_t stack_size)
    {
        m_running = true;
        m_self = detail::StartThread(name, core, priority, stack_size, [this]() { ThreadLoop(); });
    }

    // The rest are just helpers for overloaded common cases
    void Start(const char* name)
    {
        Start(name, ThreadCore::kCore0, ThreadPriority::kLow, kDefaultStackSize);
    }

    void Start(const char* name, ThreadCore core)
    {
        Start(name, core, ThreadPriority::kLow, kDefaultStackSize);
    }

    void Start(const char* name, ThreadPriority priority)
    {
        Start(name, ThreadCore::kCore0, priority, kDefaultStackSize);
    }

    void Start(const char* name, ThreadPriority priority, uint32_t stack_size)
    {
        Start(name, ThreadCore::kCore0, priority, stack_size);
    }

    void Start(const char* name, ThreadCore core, ThreadPriority priority)
    {
        Start(name, core, priority, kDefaultStackSize);
    }

    void Start(const char* name, ThreadCore core, uint32_t stack_size)
    {
        Start(name, core, ThreadPriority::kLow, stack_size);
    }

    void Start(const char* name, uint32_t stack_size)
    {
        Start(name, ThreadCore::kCore0, ThreadPriority::kLow, stack_size);
    }

protected:
    virtual void ThreadLoop() = 0;

    bool IsRunning() const
    {
        return m_running;
    }

private:
    // For unit tests
    ThreadHandle GetThreadHandle() const
    {
        return m_self;
    }

    ThreadHandle m_self;
    std::atomic_bool m_running {false};
};


static inline ThreadHandle
GetCurrentThread()
{
    return detail::GetCurrentThread();
}

static inline void
AwakeThread(ThreadHandle thread)
{
    detail::AwakeThread(thread);
}

static inline void
SuspendThread(ThreadHandle thread)
{
    detail::SuspendThread(thread);
}


} // namespace os
