#pragma once

#include "base_thread.hh"
#include "mock_time.hh"
#include "pooled_thread_base.hh"
#include "test.hh"
#pragma once

template <typename ThreadClass>
class ThreadFixtureBase : public TimeFixture
{
public:
    void SetThread(ThreadClass* thread)
    {
        m_thread = thread;

        // TODO: Allow multiple threads
        m_on_thread_start =
            NAMED_ALLOW_CALL(*kernel_mock, OnThreadStart(_)).SIDE_EFFECT(m_thread->OnStartup());
    }

    bool DoRunLoop()
    {
        REQUIRE(m_thread);

        auto now = os::GetTimeStamp();
        if (m_next_wakeup_absolute && now >= *m_next_wakeup_absolute)
        {
            m_thread->GetNotifier().release();
            m_next_wakeup_absolute = std::nullopt;
        }

        // The thread is not ready
        if (!m_thread->GetNotifier().try_acquire())
        {
            return false;
        }

        auto wake_in = m_thread->RunLoop();
        if (wake_in)
        {
            m_next_wakeup_absolute = now + *wake_in;
        }
        else
        {
            m_next_wakeup_absolute = std::nullopt;
        }

        return true;
    }

    /*
     * Run the loop again while the thread is ready, like the real thread loop does. E.g., a job
     * pool wakes a pooled thread from its timer (i.e., during the run loop). Limited, since a
     * thread can be ready all the time.
     */
    void RunLoopWhileReady()
    {
        constexpr auto kMaxRunsPerTimePoint = 16;

        for (auto i = 0; i < kMaxRunsPerTimePoint && DoRunLoop(); i++)
        {
        }
    }

    void AdvanceTimeAndRunLoop(milliseconds time)
    {
        auto run_until = os::GetTimeStamp() + time;

        // Run once first (to prime the next_wakeup)
        RunLoopWhileReady();
        if (!m_next_wakeup_absolute)
        {
            AdvanceTime(time);
            RunLoopWhileReady();
            return;
        }

        while (m_next_wakeup_absolute && *m_next_wakeup_absolute <= run_until)
        {
            SetTime(std::min(*m_next_wakeup_absolute, run_until));
            RunLoopWhileReady();
        }

        SetTime(run_until);
        RunLoopWhileReady();
    }

    /// Return the time the thread should wake up next time (if any)
    std::optional<milliseconds> NextWakeupTime() const
    {
        if (!m_next_wakeup_absolute)
        {
            return std::nullopt;
        }

        return *m_next_wakeup_absolute - os::GetTimeStamp();
    }

private:
    std::shared_ptr<os::MockKernel> kernel_mock {os::detail::GetKernelMock()};

    ThreadClass* m_thread {nullptr};

    std::optional<milliseconds> m_next_wakeup_absolute;

    std::unique_ptr<trompeloeil::expectation> m_on_thread_start;
};

using ThreadFixture = ThreadFixtureBase<os::BaseThread>;
using PooledThreadFixture = ThreadFixtureBase<PooledThreadBase>;
