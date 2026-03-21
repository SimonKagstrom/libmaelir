#include "mock_time.hh"
#include "opportunistic_scheduler.hh"
#include "os/thread.hh"
#include "test.hh"

namespace
{

class Fixture : public TimeFixture
{
public:
    Fixture()
    {
        os::detail::SetCurrentThread(&init_thread);
    }

    void AdvanceTimeAndSchedule(milliseconds time)
    {
        AdvanceTime(time);
        os::OpportunisticBinarySemaphore::RunScheduler();
    }

    os::MockThread init_thread;
    os::MockThread* current_thread {&init_thread};
};

} // namespace

TEST_CASE_FIXTURE(Fixture, "a binary semaphore can be acquired if it is available")
{
    os::OpportunisticBinarySemaphore sem {1};

    // No unexpected mock calls
    sem.acquire();

    AND_THEN("it cannot be acquired again")
    {
        REQUIRE_CALL(*current_thread, Suspend());
        sem.acquire();
    }
}

TEST_CASE_FIXTURE(Fixture, "if it's not available, the thread is suspended on acquire")
{
    os::OpportunisticBinarySemaphore sem {0};

    REQUIRE_CALL(*current_thread, Suspend());
    sem.acquire();
}

TEST_CASE_FIXTURE(Fixture, "try_acquire will return immediately if the semaphore can be acquired")
{
    os::OpportunisticBinarySemaphore sem {1};

    REQUIRE(sem.try_acquire() == true);

    AND_THEN("it cannot be acquired again")
    {
        REQUIRE(sem.try_acquire() == false);
    }
}


TEST_CASE_FIXTURE(Fixture, "A suspended thread will be awoken when the semaphore is released")
{
    os::OpportunisticBinarySemaphore sem {0};

    REQUIRE_CALL(*current_thread, Suspend());
    sem.acquire();

    REQUIRE_CALL(*current_thread, Awake());
    sem.release();
}


TEST_CASE_FIXTURE(Fixture, "try_acquire_for will return once no_later_than has been reached")
{
    os::OpportunisticBinarySemaphore sem {0};

    auto r_suspended = NAMED_REQUIRE_CALL(*current_thread, Suspend());
    auto rv = sem.try_acquire_for(os::LatestAfter(200ms));
    r_suspended = nullptr;

    AdvanceTimeAndSchedule(199ms);
    AND_THEN("the thread will be awoken after no_later_than")
    {
        REQUIRE_CALL(*current_thread, Awake());
        AdvanceTimeAndSchedule(1ms);
        REQUIRE(rv == false);
    }
}

TEST_CASE_FIXTURE(Fixture,
                  "try_acquire_for will not return before no_earlier_than has been reached")
{
    os::OpportunisticBinarySemaphore sem {0};

    auto r_suspended = NAMED_REQUIRE_CALL(*current_thread, Suspend());
    // The return value really doesn't work here
    sem.try_acquire_for(os::EarliestAfter(10ms).NoLaterThan(20ms));
    r_suspended = nullptr;

    WHEN("time is earlier than no_earlier_than")
    {
        auto r_no_awake = NAMED_FORBID_CALL(*current_thread, Awake());
        AdvanceTimeAndSchedule(9ms);
        sem.release();
        THEN("the thread is not awoken")
        {
            r_no_awake = nullptr;
        }

        AND_WHEN("the earliest deadline has passed")
        {
            auto r_awake = NAMED_REQUIRE_CALL(*current_thread, Awake());
            AdvanceTimeAndSchedule(1ms);

            THEN("the thread is awoken")
            {
                r_awake = nullptr;
            }
        }
    }
}
