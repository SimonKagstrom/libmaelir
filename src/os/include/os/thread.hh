#pragma once

#include "os_implementation.hh"
#include "semaphore.hh"
#include "thread_parameters.hh"

#include <atomic>
#include <cstdint>
#include <optional>

namespace os
{

class OsThread
{
public:
    OsThread()
    {
        m_self = detail::CreateThread([this]() { ThreadLoop(); });
    }

    virtual ~OsThread()
    {
        if (m_running)
        {
            Stop();
            os::detail::WaitThreadExit(m_self);
        }
    }

    virtual void Awake() = 0;

    void Stop()
    {
        m_running = false;
        Awake();
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
        detail::StartThread(m_self, name, core, priority, stack_size);
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
    ThreadHandle m_self;
    std::atomic_bool m_running {true};
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
