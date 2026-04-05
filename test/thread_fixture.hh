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

        // TODO: Allow multiple threads
        m_on_thread_start = NAMED_ALLOW_CALL(*kernel_mock, OnThreadStart(_))
                                .SIDE_EFFECT(m_thread->OnStartup());
    }

    bool DoRunLoop()
    {
        REQUIRE(m_thread);

        auto now = os::GetTimeStamp();
        if (m_next_wakeup_absolute && now >= *m_next_wakeup_absolute)
        {
            m_thread->m_semaphore.release();
        }

        // The thread is not ready
        if (!m_thread->m_semaphore.try_acquire())
        {
            return false;
        }

        m_next_wakeup_time = m_thread->RunLoop();
        if (m_next_wakeup_time)
        {
            m_next_wakeup_absolute = now + *m_next_wakeup_time;
        }
        else
        {
            m_next_wakeup_absolute = std::nullopt;
        }

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

    std::optional<milliseconds> m_next_wakeup_absolute;

    std::unique_ptr<trompeloeil::expectation> m_on_thread_start;

    std::shared_ptr<os::MockKernel> kernel_mock {os::detail::GetKernelMock()};
};
