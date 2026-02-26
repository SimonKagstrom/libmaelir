#pragma once

#include "base_thread.hh"
#include "mock_time.hh"
#include "test.hh"
#pragma once

class ThreadFixture : public TimeFixture
{
public:
    void SetThread(os::BaseThread* thread)
    {
        m_thread = thread;
    }

    bool DoRunLoop()
    {
        REQUIRE(m_thread);

        // The thread is not ready
        if (!m_thread->m_semaphore.try_acquire())
        {
            return false;
        }

        m_next_wakeup_time = m_thread->RunLoop();

        return true;
    }

    /// Return the time the thread should wake up next time (if any)
    std::optional<milliseconds> NextWakeupTime() const
    {
        return m_next_wakeup_time;
    }

private:
    os::BaseThread* m_thread {nullptr};

    std::optional<milliseconds> m_next_wakeup_time;
};
