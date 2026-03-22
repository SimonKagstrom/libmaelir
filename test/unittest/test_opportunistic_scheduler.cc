#include "mock_time.hh"
#include "opportunistic_scheduler.hh"
#include "os/thread.hh"
#include "test.hh"

class UnittestOpportunisticSemaphore : public os::OpportunisticBinarySemaphore
{
public:
    using os::OpportunisticBinarySemaphore::OpportunisticBinarySemaphore;

    bool DoTryAcquireNoSuspend() noexcept
    {
        return TryAcquireNoSuspend();
    }
};

class SchedulerFixture : public TimeFixture
{
public:
    SchedulerFixture()
    {
        m_expectations.push_back(
            NAMED_ALLOW_CALL(*kernel, TriggerWakeup(_)).SIDE_EFFECT(next_wakeup_time = _1));

        threads.push_back(std::make_unique<os::MockThread>());
        current_thread = threads.back().get();
        os::detail::SetCurrentThread(current_thread);
    }

    auto RunScheduler()
    {
        return scheduler.Schedule();
    }

    auto AdvanceTimeAndSchedule(milliseconds time)
    {
        AdvanceTime(time);
        return RunScheduler();
    }

    os::MockThread* CreateThread()
    {
        threads.push_back(std::make_unique<os::MockThread>());
        return threads.back().get();
    }

    os::MockThread* CreateAndActivateThread()
    {
        auto out = CreateThread();
        SetCurrentThread(out);
        return out;
    }

    void SetCurrentThread(os::MockThread* thread)
    {
        os::detail::SetCurrentThread(thread);
    }

    os::binary_semaphore semaphore {0};
    os::OpportunisticScheduler scheduler {semaphore};
    milliseconds next_wakeup_time {0ms};

    std::shared_ptr<os::MockKernel> kernel {os::MockKernel().Create()};
    std::vector<std::unique_ptr<os::MockThread>> threads;
    os::MockThread* current_thread;

private:
    std::vector<std::unique_ptr<trompeloeil::expectation>> m_expectations;
};

TEST_CASE_FIXTURE(SchedulerFixture, "a binary semaphore can be acquired if it is available")
{
    UnittestOpportunisticSemaphore sem {1};

    // No unexpected mock calls
    sem.acquire();

    AND_THEN("it cannot be acquired again")
    {
        REQUIRE_CALL(*current_thread, Suspend());
        sem.acquire();
    }
}

TEST_CASE_FIXTURE(SchedulerFixture, "if it's not available, the thread is suspended on acquire")
{
    UnittestOpportunisticSemaphore sem {0};

    REQUIRE_CALL(*current_thread, Suspend());
    sem.acquire();
}

TEST_CASE_FIXTURE(SchedulerFixture,
                  "try_acquire will return immediately if the semaphore can be acquired")
{
    UnittestOpportunisticSemaphore sem {1};

    REQUIRE(sem.try_acquire() == true);

    AND_THEN("it cannot be acquired again")
    {
        REQUIRE(sem.try_acquire() == false);
    }
}


TEST_CASE_FIXTURE(SchedulerFixture,
                  "A suspended thread will be awoken when the semaphore is released")
{
    UnittestOpportunisticSemaphore sem {0};

    REQUIRE_CALL(*current_thread, Suspend());
    sem.acquire();

    REQUIRE_CALL(*current_thread, Awake());
    sem.release();
}

#if 0
TEST_CASE_FIXTURE(SchedulerFixture, "TooEarlySuspend will trigger a wakeup at no_later_than")
{
    UnittestOpportunisticSemaphore sem {0};

    auto now = os::GetTimeStamp();

    WHEN("a thread is suspended with a no_earlier_than time")
    {
        auto r_suspended = NAMED_REQUIRE_CALL(*current_thread, Suspend());
        sem.DoSuspendForTooEarly(os::EarliestAfter(100ms).NoLaterThan(200ms));

        THEN("the thread is suspended")
        {
            r_suspended = nullptr;
        }
        AND_THEN("the next trigger time is no_later_than")
        {
            REQUIRE(next_wakeup_time == now + 200ms);
        }

        auto other_thread = CreateAndActivateThread();
        AND_WHEN("another thread is suspended with a no_earlier_than time, with no_later before "
                 "the current")
        {
            REQUIRE_CALL(*other_thread, Suspend());
            sem.DoSuspendForTooEarly(os::EarliestAfter(50ms).NoLaterThan(150ms));

            THEN("the next trigger is shortened")
            {
                REQUIRE(next_wakeup_time == now + 150ms);
            }
        }

        AND_WHEN("another thread is suspended with a no_earlier_than time, with no_later after the "
                 "current")
        {
            REQUIRE_CALL(*other_thread, Suspend());
            sem.DoSuspendForTooEarly(os::EarliestAfter(50ms).NoLaterThan(250ms));

            THEN("the first trigger  time is kept")
            {
                REQUIRE(next_wakeup_time == now + 200ms);
            }
        }
    }
}
#endif

TEST_CASE_FIXTURE(SchedulerFixture, "the scheduler can wakeup suspended threads")
{
    UnittestOpportunisticSemaphore sem {0};
}

TEST_CASE_FIXTURE(SchedulerFixture, "the scheduler can opportunistically wakeup threads that are ready")
{
    UnittestOpportunisticSemaphore sem {0};
}


TEST_CASE_FIXTURE(SchedulerFixture,
                  "try_acquire_for will return once no_later_than has been reached")
{
    UnittestOpportunisticSemaphore sem {0};

    auto r_suspended = NAMED_REQUIRE_CALL(*current_thread, Suspend());
    auto rv = sem.try_acquire_for(os::LatestAfter(200ms));
    r_suspended = nullptr;

    AND_THEN("the thread will be awoken after no_later_than (with no semaphore)")
    {
        AdvanceTimeAndSchedule(199ms);
        REQUIRE_CALL(*current_thread, Awake());
        AdvanceTimeAndSchedule(1ms);
        REQUIRE(rv == false);
    }

    AND_WHEN("the semaphore is released before no_later_than")
    {
        auto r_woke = NAMED_REQUIRE_CALL(*current_thread, Awake());
        AdvanceTimeAndSchedule(100ms);
        sem.release();
        AdvanceTimeAndSchedule(1ms);
        THEN("it's only awoken directly")
        {
            r_woke = nullptr;
        }
    }
}

TEST_CASE_FIXTURE(SchedulerFixture,
                  "try_acquire_for will not return before no_earlier_than has been reached")
{
    UnittestOpportunisticSemaphore sem {0};

    auto now = os::GetTimeStamp();
    auto r_suspended = NAMED_REQUIRE_CALL(*current_thread, Suspend());
    // The return value really doesn't work here
    sem.try_acquire_for(os::EarliestAfter(10ms));
    r_suspended = nullptr;

    THEN("it will wait until no_earlier_than has been reached")
    {
        REQUIRE(next_wakeup_time == now + 10ms);
        AdvanceTimeAndSchedule(9ms);
        AND_THEN("the thread can be awoken after the no_earlier_than time has passed")
        {
            AdvanceTimeAndSchedule(1ms);
            REQUIRE_CALL(*current_thread, Awake());
            sem.release();

            RunScheduler();
        }
    }
}


TEST_CASE_FIXTURE(SchedulerFixture, "the second thread will have to wait for an acquired semaphore")
{
    UnittestOpportunisticSemaphore sem {1};

    auto first_thread = current_thread;
    auto other_thread = CreateThread();
    REQUIRE(current_thread != other_thread);

    auto rv = sem.try_acquire();
    REQUIRE(rv);

    WHEN("the second thread tries to acquire the semaphore")
    {
        SetCurrentThread(other_thread);
        auto r_suspended = NAMED_REQUIRE_CALL(*other_thread, Suspend());
        sem.acquire();

        THEN("it has to sleep")
        {
            r_suspended = nullptr;
        }

        AND_WHEN("the first thread releases the semaphore")
        {
            auto r_woke = NAMED_REQUIRE_CALL(*other_thread, Awake());
            SetCurrentThread(first_thread);
            sem.release();
            RunScheduler();

            THEN("the second thread is awoken")
            {
                r_woke = nullptr;
            }
        }
    }
}

TEST_CASE_FIXTURE(SchedulerFixture, "")
{
}
