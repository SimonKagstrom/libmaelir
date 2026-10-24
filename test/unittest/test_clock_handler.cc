#include "clock_handler.hh"
#include "job_pool_thread.hh"
#include "test.hh"
#include "thread_fixture.hh"

namespace
{

constexpr uint32_t kTime = 1729771234;

class Fixture : public ThreadFixture
{
public:
    Fixture()
    {
        // Pooled, like on the target. The job pool owns it from here on
        job_pool.AttachPooledThread(std::move(clock_handler_owner));
        SetThread(&job_pool);

        job_pool.Start("job_pool");
    }

    void SendTime(uint32_t utc, std::optional<int16_t> timezone_minutes = std::nullopt)
    {
        post_office.Send<MSG::network_time>({utc, timezone_minutes});
        RunLoopWhileReady();
    }

    ApplicationState state;
    PostOffice<MSG::AllMessages> post_office;

    std::unique_ptr<ClockHandler> clock_handler_owner {
        std::make_unique<ClockHandler>(state, post_office)};

    JobPoolThread job_pool;
};

} // namespace

TEST_SUITE_BEGIN("clock_handler");

TEST_CASE_FIXTURE(Fixture, "the clock is not set before a time is received")
{
    AdvanceTimeAndRunLoop(5s);

    CHECK(state.Get<AS::clock>() == 0);
    CHECK(state.Get<AS::timezone>() == 0);
}

TEST_CASE_FIXTURE(Fixture, "the clock follows the network time")
{
    AdvanceTimeAndRunLoop(1s);

    WHEN("a network time with a timezone is received")
    {
        SendTime(kTime, 120);

        THEN("the clock and timezone are set immediately")
        {
            CHECK(state.Get<AS::clock>() == kTime);
            CHECK(state.Get<AS::timezone>() == 120);
        }

        AND_WHEN("almost a second passes")
        {
            AdvanceTimeAndRunLoop(999ms);

            THEN("the clock is unchanged")
            {
                CHECK(state.Get<AS::clock>() == kTime);
            }

            AND_WHEN("the second is complete")
            {
                AdvanceTimeAndRunLoop(1ms);

                THEN("the clock ticks")
                {
                    CHECK(state.Get<AS::clock>() == kTime + 1);
                }
            }
        }

        AND_WHEN("an hour passes")
        {
            AdvanceTimeAndRunLoop(1h);

            THEN("the clock has not drifted")
            {
                CHECK(state.Get<AS::clock>() == kTime + 3600);
            }
        }

        AND_WHEN("time passes in uneven steps")
        {
            for (auto i = 0; i < 10; i++)
            {
                AdvanceTimeAndRunLoop(350ms);
            }

            THEN("only whole seconds are counted, without losing the rest")
            {
                CHECK(state.Get<AS::clock>() == kTime + 3);

                AdvanceTimeAndRunLoop(500ms);
                CHECK(state.Get<AS::clock>() == kTime + 4);
            }
        }

        AND_WHEN("the clock thread runs late")
        {
            // E.g., a busy job pool. Only the time stamps matter
            AdvanceTime(2500ms);
            RunLoopWhileReady();

            THEN("the clock catches up")
            {
                CHECK(state.Get<AS::clock>() == kTime + 2);
            }

        }

        AND_WHEN("the clock thread runs late several times")
        {
            AdvanceTime(1500ms);
            RunLoopWhileReady();
            AdvanceTime(1500ms);
            RunLoopWhileReady();

            THEN("the parts of seconds left over aren't lost")
            {
                CHECK(state.Get<AS::clock>() == kTime + 3);
            }
        }

        AND_WHEN("a new, earlier, time without a timezone is received in the middle of a second")
        {
            AdvanceTimeAndRunLoop(500ms);
            SendTime(1000);

            THEN("the clock is set back, but the timezone is kept")
            {
                CHECK(state.Get<AS::clock>() == 1000);
                CHECK(state.Get<AS::timezone>() == 120);
            }

            AND_THEN("the clock ticks a second after the new time, not at the old second")
            {
                AdvanceTimeAndRunLoop(999ms);
                CHECK(state.Get<AS::clock>() == 1000);
                AdvanceTimeAndRunLoop(1ms);
                CHECK(state.Get<AS::clock>() == 1001);
            }
        }

        AND_WHEN("a negative timezone is received")
        {
            SendTime(kTime, -210);

            THEN("it's set")
            {
                CHECK(state.Get<AS::timezone>() == -210);
            }
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "the clock handles the time stamp wrapping")
{
    // The millisecond time stamp is a uint32_t, so it wraps after ~49 days
    SetTime(milliseconds(std::numeric_limits<uint32_t>::max()) - 499ms);
    SendTime(kTime);

    WHEN("the time stamp wraps")
    {
        SetTime(500ms);
        RunLoopWhileReady();

        THEN("the clock ticks normally")
        {
            CHECK(state.Get<AS::clock>() == kTime + 1);
        }
    }
}
