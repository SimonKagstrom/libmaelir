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

    // Like the GPS reader, which does this every second while there is a fix
    void SetGpsTime(uint32_t utc)
    {
        state.Set<AS::gps_utc>(utc);
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

TEST_CASE_FIXTURE(Fixture, "the clock follows the GPS time")
{
    AdvanceTimeAndRunLoop(1s);

    WHEN("a GPS time arrives in the middle of a second")
    {
        AdvanceTimeAndRunLoop(500ms);
        SetGpsTime(kTime);

        THEN("the clock is set immediately")
        {
            CHECK(state.Get<AS::clock>() == kTime);
        }

        AND_THEN("the clock ticks a second after the GPS time, not at the old second")
        {
            AdvanceTimeAndRunLoop(999ms);
            CHECK(state.Get<AS::clock>() == kTime);
            AdvanceTimeAndRunLoop(1ms);
            CHECK(state.Get<AS::clock>() == kTime + 1);
        }

        AND_WHEN("the GPS time keeps arriving every second")
        {
            for (auto i = 1u; i <= 10; i++)
            {
                AdvanceTimeAndRunLoop(1s);
                SetGpsTime(kTime + i);
                REQUIRE(state.Get<AS::clock>() == kTime + i);
            }

            THEN("the clock follows it")
            {
                CHECK(state.Get<AS::clock>() == kTime + 10);
            }
        }

        AND_WHEN("the GPS time jumps forward")
        {
            AdvanceTimeAndRunLoop(200ms);
            SetGpsTime(kTime + 60);

            THEN("the clock follows it")
            {
                CHECK(state.Get<AS::clock>() == kTime + 60);
            }
        }

        AND_WHEN("the GPS time jumps back")
        {
            AdvanceTimeAndRunLoop(200ms);
            SetGpsTime(kTime - 60);

            THEN("the clock follows it")
            {
                CHECK(state.Get<AS::clock>() == kTime - 60);
            }
        }

        AND_WHEN("the fix is lost, so the GPS time is no longer updated")
        {
            AdvanceTimeAndRunLoop(1h);

            THEN("the clock keeps ticking, and isn't set back to the last GPS time")
            {
                CHECK(state.Get<AS::clock>() == kTime + 3600);
            }

            AND_WHEN("the fix comes back")
            {
                AdvanceTimeAndRunLoop(500ms);
                SetGpsTime(kTime + 3605);

                THEN("the clock follows the GPS time again")
                {
                    CHECK(state.Get<AS::clock>() == kTime + 3605);
                }
            }
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "the GPS time has priority over the network time")
{
    WHEN("the network time comes first")
    {
        SendTime(kTime, 60);

        AND_WHEN("a GPS time arrives")
        {
            SetGpsTime(kTime + 2);

            THEN("the GPS time is used, and the timezone is kept")
            {
                CHECK(state.Get<AS::clock>() == kTime + 2);
                CHECK(state.Get<AS::timezone>() == 60);
            }

            AND_WHEN("another network time arrives while there is a fix")
            {
                state.Set<AS::gps_status>(GpsStatus::kPositionValid);
                SendTime(kTime + 100, 120);

                THEN("only its timezone is used")
                {
                    CHECK(state.Get<AS::clock>() == kTime + 2);
                    CHECK(state.Get<AS::timezone>() == 120);
                }
            }

            AND_WHEN("another network time arrives without a fix")
            {
                state.Set<AS::gps_status>(GpsStatus::kNoFix);
                SendTime(kTime + 100, 120);

                THEN("it's used")
                {
                    CHECK(state.Get<AS::clock>() == kTime + 100);
                    CHECK(state.Get<AS::timezone>() == 120);
                }
            }
        }
    }

    WHEN("the GPS time comes first, with a fix")
    {
        state.Set<AS::gps_status>(GpsStatus::kPositionValid);
        SetGpsTime(kTime);
        SendTime(1000, 120);

        THEN("only the timezone is taken from the network time")
        {
            CHECK(state.Get<AS::clock>() == kTime);
            CHECK(state.Get<AS::timezone>() == 120);
        }

        AND_WHEN("the fix is lost, and a network time arrives")
        {
            state.Set<AS::gps_status>(GpsStatus::kSilent);
            SendTime(1000, 60);

            THEN("the network time is used")
            {
                CHECK(state.Get<AS::clock>() == 1000);
                CHECK(state.Get<AS::timezone>() == 60);
            }

            AND_WHEN("the fix comes back")
            {
                state.Set<AS::gps_status>(GpsStatus::kPositionValid);
                SetGpsTime(kTime + 5);

                THEN("the GPS time is used again")
                {
                    CHECK(state.Get<AS::clock>() == kTime + 5);
                }
            }
        }
    }

    WHEN("there is a valid position, but no GPS time")
    {
        // E.g., demo mode, or a GPS with week rollover dates (which are dropped)
        state.Set<AS::gps_status>(GpsStatus::kPositionValid);
        SendTime(kTime, 120);

        THEN("the network time is used")
        {
            CHECK(state.Get<AS::clock>() == kTime);
            CHECK(state.Get<AS::timezone>() == 120);
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
