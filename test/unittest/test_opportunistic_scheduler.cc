#include "mock_time.hh"
#include "opportunistic_scheduler.hh"
#include "os/thread.hh"
#include "test.hh"

class UnittestOpportunisticSemaphore : public os::OpportunisticBinarySemaphore
{
public:
    using os::OpportunisticBinarySemaphore::OpportunisticBinarySemaphore;
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
        std::optional<milliseconds> out;
        auto now = os::GetTimeStamp();

        // Really done by the scheduler thread
        if (scheduler.m_semaphore.try_acquire())
        {
            out = scheduler.Schedule();

            if (!out)
            {
                next_wakeup_time = 0xffffffffms;
            }
            else
            {
                next_wakeup_time = *out;
                printf("Next wakeup at %u ms\n", next_wakeup_time.count());
            }
        }

        return out;
    }

    auto AdvanceTimeAndSchedule(milliseconds time)
    {
        AdvanceTime(time);

        auto now = os::GetTimeStamp();
        if (now >= next_wakeup_time)
        {
            semaphore.release();
        }

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
    milliseconds next_wakeup_time {0xffffffffms};

    std::shared_ptr<os::MockKernel> kernel {os::MockKernel().Create()};
    std::vector<std::unique_ptr<os::MockThread>> threads;
    os::MockThread* current_thread;

private:
    std::vector<std::unique_ptr<trompeloeil::expectation>> m_expectations;
};

TEST_CASE_FIXTURE(SchedulerFixture, "scheduling without waiters is a nop")
{
    FORBID_CALL(*current_thread, Suspend());
    FORBID_CALL(*current_thread, Awake());

    REQUIRE(scheduler.Schedule() == std::nullopt);
}

TEST_CASE_FIXTURE(SchedulerFixture, "a pending wakeup can be added to the scheduler")
{
    WHEN("an entry in the 10..20ms interval is added")
    {
        auto t1 = CreateThread();

        scheduler.AddPendingEntry(t1, 0, {0ms, {10ms, 20ms}});
        THEN("scheduling directly will not wake the thread")
        {
            FORBID_CALL(*t1, Awake());
            auto wakeup = scheduler.Schedule();

            AND_THEN("the next wakeup time is the latest time")
            {
                REQUIRE(wakeup == os::GetTimeStamp() + 20ms);
            }

            AND_THEN("also not after 9ms")
            {
                AdvanceTime(9ms);
                wakeup = scheduler.Schedule();
                REQUIRE(wakeup == os::GetTimeStamp() + 11ms);
            }
        }
        AND_THEN("the thread wakes up at the latest time")
        {
            REQUIRE_CALL(*t1, Awake());
            AdvanceTime(20ms);
            auto wakeup = scheduler.Schedule();

            AND_THEN("the next wakeup is forever")
            {
                REQUIRE(wakeup == std::nullopt);
            }
        }

        AND_WHEN("someone does schedule during the wakeup interval")
        {
            REQUIRE_CALL(*t1, Awake());
            AdvanceTime(15ms);
            auto wakeup = scheduler.Schedule();

            THEN("the thread is woke up")
            {
                scheduler.RequestSchedule(0);
            }
            AND_THEN("the next wakeup is forever")
            {
                REQUIRE(wakeup == std::nullopt);
            }
        }

        AND_WHEN("an entry with an earlier latest wakeup interval is added after a while")
        {
            AdvanceTime(5ms);
            REQUIRE(scheduler.Schedule() == os::GetTimeStamp() + 15ms);

            auto t2 = CreateThread();
            scheduler.AddPendingEntry(t2, 0, {0ms, {7ms, 10ms}}); // 12ms..15ms from start

            auto wakeup = scheduler.Schedule();
            THEN("the next wakeup is the last time of the two entries")
            {
                REQUIRE(wakeup == os::GetTimeStamp() + 10ms);
            }

            AND_WHEN("the earliest thread is woke up due to the schedule request after it's "
                     "earliest time")
            {
                AdvanceTime(7ms);
                auto r_thread_2_woke = NAMED_REQUIRE_CALL(*t2, Awake());
                auto r_thread_1_woke = NAMED_REQUIRE_CALL(*t1, Awake());
                wakeup = scheduler.Schedule();

                THEN("both threads are woken")
                {
                    r_thread_2_woke = nullptr;
                    r_thread_1_woke = nullptr;
                }
                AND_THEN("wakeup is forever")
                {
                    REQUIRE(wakeup == std::nullopt);
                }
            }
        }
    }
}


TEST_CASE_FIXTURE(SchedulerFixture, "early entries are transformed to pending and woke entries")
{
    WHEN("an event entry which should not wake earlier than in 10 ms is added")
    {
        scheduler.AddEarlyEntry(current_thread, 0, os::WakeupConfiguration {10ms, {10ms, 1s}});

        auto wakeup = scheduler.Schedule();
        THEN("the next wakeup is at the latest point")
        {
            REQUIRE(wakeup == os::GetTimeStamp() + 1s);
        }

        AND_WHEN("time moves a bit")
        {
            AdvanceTime(9ms);
            wakeup = scheduler.Schedule();

            THEN("the next wakeup also moves")
            {
                REQUIRE(wakeup == os::GetTimeStamp() + 991ms);
            }
        }

        AND_WHEN("the scheduler is triggered during the wake interval")
        {
            auto r_woke = NAMED_REQUIRE_CALL(*current_thread, Awake());
            AdvanceTime(10ms);
            auto wakeup = scheduler.Schedule();

            THEN("the thread is woke")
            {
                r_woke = nullptr;
            }
            AND_THEN("the next wakeup is forever")
            {
                REQUIRE(wakeup == std::nullopt);
            }
        }

        WHEN("another entry is added with a later wake deadline than the first, but an earlier "
             "earliest")
        {
            auto t2 = CreateThread();
            scheduler.AddEarlyEntry(t2, 0, os::WakeupConfiguration {5ms, {20ms, 2s}});

            auto wakeup = scheduler.Schedule();
            THEN("the next wakeup is from the current thread")
            {
                REQUIRE(wakeup == os::GetTimeStamp() + 1s);
            }
        }

        WHEN("another entry is added with an earlier wake deadline than the first")
        {
            auto t2 = CreateThread();
            scheduler.AddEarlyEntry(t2, 0, os::WakeupConfiguration {5ms, {20ms, 500ms}});

            auto wakeup = scheduler.Schedule();
            THEN("the next wakeup is from the new thread")
            {
                REQUIRE(wakeup == os::GetTimeStamp() + 500ms);
            }
        }
    }
}

TEST_CASE_FIXTURE(SchedulerFixture,
                  "the scheduler can be triggered to wake pending entries on release()")
{
    // TBD
}


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
        FORBID_CALL(*current_thread, Suspend());
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


TEST_CASE_FIXTURE(SchedulerFixture,
                  "try_acquire_for will return once no_later_than has been reached")
{
    UnittestOpportunisticSemaphore sem {0};

    auto r_suspended = NAMED_REQUIRE_CALL(*current_thread, Suspend());
    auto rv = sem.try_acquire_for(os::LatestAfter(200ms));
    r_suspended = nullptr;

    RunScheduler();

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
        RunScheduler();
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
        REQUIRE(os::GetTimeStamp() - now == 10ms);
    }
}

TEST_CASE_FIXTURE(SchedulerFixture,
                  "the scheduler can opportunistically wakeup threads that are ready")
{
    UnittestOpportunisticSemaphore sem_1 {0};
    UnittestOpportunisticSemaphore sem_2 {0};

    auto thread_1 = current_thread;

    // Two active threads
    REQUIRE_CALL(*thread_1, Suspend());
    sem_1.try_acquire_for(os::LatestAfter(200ms));

    auto thread_2 = CreateAndActivateThread();
    REQUIRE_CALL(*thread_2, Suspend());
    sem_2.try_acquire_for(os::LatestAfter(100ms));

    auto now = os::GetTimeStamp();

    auto next_wakeup = RunScheduler();
    REQUIRE(next_wakeup == now + 100ms);

    WHEN("the first thread is woke up due to timeout")
    {
        AdvanceTimeAndSchedule(99ms);

        auto r_earliest = NAMED_REQUIRE_CALL(*thread_2, Awake());
        auto r_opportunistically_woke = NAMED_REQUIRE_CALL(*thread_1, Awake());
        AdvanceTimeAndSchedule(1ms);
        THEN("the second one is also woke")
        {
            r_earliest = nullptr;
            r_opportunistically_woke = nullptr;
        }
    }

    WHEN("the first thread is woke up due to event (semaphore release)")
    {
        AdvanceTimeAndSchedule(1ms);

        auto r_released = NAMED_REQUIRE_CALL(*thread_1, Awake());
        auto r_opportunistically_woke = NAMED_REQUIRE_CALL(*thread_2, Awake());
        sem_1.release();
        RunScheduler();

        THEN("the second one is also woke")
        {
            r_released = nullptr;
            r_opportunistically_woke = nullptr;
        }
    }
}

TEST_CASE_FIXTURE(SchedulerFixture,
                  "only waiters in the wakeup_interval will be opportunistically woken up")
{
    UnittestOpportunisticSemaphore sem_1 {0};
    UnittestOpportunisticSemaphore sem_2 {0};

    auto thread_1 = current_thread;

    // Three active threads
    REQUIRE_CALL(*thread_1, Suspend());
    sem_1.acquire();

    auto thread_2 = CreateAndActivateThread();
    REQUIRE_CALL(*thread_2, Suspend());
    sem_2.try_acquire_for(os::WakeupConfiguration {0ms, {1ms, 100ms}});

    auto thread_3 = CreateAndActivateThread();
    REQUIRE_CALL(*thread_3, Suspend());
    sem_2.try_acquire_for(os::WakeupConfiguration {0ms, {10ms, 200ms}});

    RunScheduler();

    AdvanceTimeAndSchedule(9ms);
    WHEN("a semaphore is released")
    {
        REQUIRE_CALL(*thread_1, Awake());
        auto r_woke_2 = NAMED_REQUIRE_CALL(*thread_2, Awake());
        auto r_no_woke_3 = NAMED_FORBID_CALL(*thread_3, Awake());

        sem_1.release();
        RunScheduler();
        THEN("the threads in the wakeup_interval are woken")
        {
            r_woke_2 = nullptr;
        }
        AND_THEN("pending threads are not")
        {
            r_no_woke_3 = nullptr;
        }

        AND_WHEN("the time is advanced again to the last wakeup time")
        {
            auto r_woke_3 = NAMED_REQUIRE_CALL(*thread_3, Awake());
            AdvanceTimeAndSchedule(191ms);

            THEN("the other thread is also woke")
            {
                r_woke_3 = nullptr;
            }
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

TEST_CASE_FIXTURE(SchedulerFixture, "two non-overlapping sleepers have been added")
{
    UnittestOpportunisticSemaphore sem {0};

    REQUIRE_CALL(*current_thread, Suspend());
    sem.try_acquire_for(os::WakeupConfiguration {0ms, {20ms, 30ms}});

    WHEN("the second sleeper has a strict interval")
    {
        auto thread_2 = CreateAndActivateThread();
        REQUIRE_CALL(*thread_2, Suspend());
        sem.try_acquire_for(os::WakeupConfiguration {0ms, {50ms, 50ms}});

        RunScheduler();

        THEN("the next wakeup is the earliest time of the first sleeper")
        {
            REQUIRE(next_wakeup_time == os::GetTimeStamp() + 30ms);
        }

        AND_WHEN("the first sleeper is woke up")
        {
            AdvanceTimeAndSchedule(29ms);
            auto r_woke = NAMED_REQUIRE_CALL(*current_thread, Awake());
            AdvanceTimeAndSchedule(1ms);

            THEN("the first sleeper is woke")
            {
                r_woke = nullptr;
            }
            AND_THEN("the next wakeup is at the end of the time")
            {
                REQUIRE(next_wakeup_time == 0xffffffffms);
            }
        }
    }

    AND_WHEN("the second sleeper has a opportunistic interval")
    {
        auto thread_2 = CreateAndActivateThread();
        REQUIRE_CALL(*thread_2, Suspend());
        sem.try_acquire_for(os::WakeupConfiguration {0ms, {40ms, 50ms}});

        RunScheduler();

        THEN("the next wakeup is the earliest time of the first sleeper")
        {
            REQUIRE(next_wakeup_time == os::GetTimeStamp() + 30ms);
        }

        AND_THEN(
            "wakeup of the first sleeper and next wakeup time is handled like the strict variant")
        {
            AdvanceTimeAndSchedule(29ms);
            REQUIRE_CALL(*current_thread, Awake());

            AdvanceTimeAndSchedule(1ms);
            REQUIRE(next_wakeup_time == os::GetTimeStamp() + 20ms);
        }
    }
}

TEST_CASE_FIXTURE(SchedulerFixture,
                  "a sleeper can specify both an earliest time and a wakeup interval")
{
    UnittestOpportunisticSemaphore sem {0};

    auto r_suspended = NAMED_REQUIRE_CALL(*current_thread, Suspend());
    sem.try_acquire_for(os::WakeupConfiguration {10ms, {20ms, 30ms}});
    RunScheduler();

    THEN("the thread is suspended")
    {
        r_suspended = nullptr;
    }
    AND_THEN("the next wakeup is scheduled for the latest time")
    {
        REQUIRE(next_wakeup_time == os::GetTimeStamp() + 30ms);
    }

    AND_WHEN("the semaphore is released before the earliest time")
    {
        AdvanceTimeAndSchedule(5ms);

        auto r_no_woke = NAMED_FORBID_CALL(*current_thread, Awake());
        sem.release();
        RunScheduler();

        THEN("the thread is not woke")
        {
            r_no_woke = nullptr;
        }
        AND_THEN("the next wakeup is the earliest time")
        {
            REQUIRE(next_wakeup_time == os::GetTimeStamp() + 5ms);
        }

        AND_THEN("the thread wakes when the earliest time is reached")
        {
            REQUIRE_CALL(*current_thread, Awake());
            AdvanceTimeAndSchedule(5ms);
        }
    }

    AND_WHEN("the last time in the interval is reached")
    {
        // Not yet
        AdvanceTimeAndSchedule(29ms);

        auto r_woke = NAMED_REQUIRE_CALL(*current_thread, Awake());

        AdvanceTimeAndSchedule(1ms);
        THEN("the thread is woke")
        {
            r_woke = nullptr;
        }
    }
}
