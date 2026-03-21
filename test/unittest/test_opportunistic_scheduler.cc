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
        threads.push_back(std::make_unique<os::MockThread>());
        current_thread = threads.back().get();
        os::detail::SetCurrentThread(current_thread);
    }

    void AdvanceTimeAndSchedule(milliseconds time)
    {
        AdvanceTime(time);
        os::OpportunisticBinarySemaphore::RunScheduler();
    }

    os::MockThread* CreateThread()
    {
        threads.push_back(std::make_unique<os::MockThread>());
        return threads.back().get();
    }

    void SetCurrentThread(os::MockThread* thread)
    {
        os::detail::SetCurrentThread(thread);
    }

    std::vector<std::unique_ptr<os::MockThread>> threads;
    os::MockThread* current_thread;
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

TEST_CASE_FIXTURE(Fixture,
                  "try_acquire_for will not return before no_earlier_than has been reached")
{
    os::OpportunisticBinarySemaphore sem {0};

    auto r_suspended = NAMED_REQUIRE_CALL(*current_thread, Suspend());
    auto now = os::GetTimeStamp();
    // The return value really doesn't work here
    sem.try_acquire_for(os::EarliestAfter(10ms));
    r_suspended = nullptr;

    THEN("it will wait until no_earlier_than has been reached")
    {
        REQUIRE(os::GetTimeStamp() - now == 10ms);
        AND_THEN("the thread can be awoken")
        {
            REQUIRE_CALL(*current_thread, Awake());
            sem.release();
            os::OpportunisticBinarySemaphore::RunScheduler();
        }
    }
}


TEST_CASE_FIXTURE(Fixture, "two threads wait on the same semaphore")
{
    os::OpportunisticBinarySemaphore sem {0};
 
    auto other_thread = CreateThread();
}
