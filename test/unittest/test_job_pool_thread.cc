#include "job_pool_thread.hh"
#include "test.hh"
#include "thread_fixture.hh"


namespace
{

class PooledThread : public PooledThreadBase
{
public:
    void DoStop()
    {
        Stop();
    }

public:
    PooledThread()
    {
        m_expectations.push_back(NAMED_ALLOW_CALL(*this, OnStartup()));
    }

    MAKE_MOCK0(OnStartup, void(), final);
    MAKE_MOCK0(OnActivation, std::optional<milliseconds>(), final);

private:
    std::vector<std::unique_ptr<trompeloeil::expectation>> m_expectations;
};

class Fixture : public ThreadFixture
{
public:
    Fixture()
    {
        SetThread(job_thread.get());
    }

    std::pair<std::unique_ptr<PooledThread>, PooledThread*> CreatePooledThread()
    {
        auto thread = std::make_unique<PooledThread>();
        auto raw_ptr = thread.get();

        return {std::move(thread), raw_ptr};
    }

    std::unique_ptr<JobPoolThread> job_thread {std::make_unique<JobPoolThread>()};
};

} // namespace

TEST_SUITE_BEGIN("job_pool_thread");

TEST_CASE_FIXTURE(Fixture, "OnStartup is called for all pooled threads on start")
{
    auto [p0_up, p0] = CreatePooledThread();
    auto [p1_up, p1] = CreatePooledThread();

    ALLOW_CALL(*p0, OnActivation()).RETURN(std::nullopt);
    ALLOW_CALL(*p1, OnActivation()).RETURN(std::nullopt);

    job_thread->AttachPooledThread(std::move(p0_up));
    job_thread->AttachPooledThread(std::move(p1_up));

    WHEN("the job thread is started")
    {
        auto r0 = NAMED_REQUIRE_CALL(*p0, OnStartup());
        auto r1 = NAMED_REQUIRE_CALL(*p1, OnStartup());

        job_thread->Start("job_pool");
        DoRunLoop();

        THEN("the OnStartup callbacks have been called")
        {
            r0 = nullptr;
            r1 = nullptr;
        }
    }

    WHEN("a pooled thread is exited before startup")
    {
        auto r0 = NAMED_FORBID_CALL(*p0, OnStartup());
        auto r1 = NAMED_REQUIRE_CALL(*p1, OnStartup());

        p0->DoStop();
        job_thread->Start("job_pool");
        DoRunLoop();

        THEN("only the remaining one gets called")
        {
            r0 = nullptr;
            r1 = nullptr;
        }
    }

    WHEN("a pooled thread is added after startup")
    {
        // OnStartup should be called
    }
}


TEST_CASE_FIXTURE(Fixture, "a single pooled thread can run all the time")
{
    auto [p0_up, p0] = CreatePooledThread();
    job_thread->AttachPooledThread(std::move(p0_up));

    WHEN("the task runs")
    {
        job_thread->Start("job_pool");

        auto r0 = NAMED_REQUIRE_CALL(*p0, OnActivation()).RETURN(0ms);
        DoRunLoop();
        auto r1 = NAMED_REQUIRE_CALL(*p0, OnActivation()).RETURN(0ms);
        DoRunLoop();

        THEN("OnActivation is called all the time")
        {
            r0 = nullptr;
            r1 = nullptr;
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "a single pooled thread can wait for events")
{
    auto [p0_up, p0] = CreatePooledThread();

    job_thread->AttachPooledThread(std::move(p0_up));

    WHEN("the task runs")
    {
        auto r0 = NAMED_REQUIRE_CALL(*p0, OnActivation()).RETURN(std::nullopt);
        job_thread->Start("job_pool");
        DoRunLoop();

        THEN("it's run once")
        {
            r0 = nullptr;
        }
        AND_THEN("waits for an event")
        {
            FORBID_CALL(*p0, OnActivation());
            DoRunLoop();

            AND_WHEN("an event occurs")
            {
                r0 = NAMED_REQUIRE_CALL(*p0, OnActivation()).RETURN(std::nullopt);
                p0->Awake();
                DoRunLoop();

                THEN("the thread runs again")
                {
                    r0 = nullptr;
                }
            }
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "a single pooled thread can wait for a timeout")
{
    auto [p0_up, p0] = CreatePooledThread();

    job_thread->AttachPooledThread(std::move(p0_up));

    WHEN("the task runs")
    {
        auto r0 = NAMED_REQUIRE_CALL(*p0, OnActivation()).RETURN(10ms);

        job_thread->Start("job_pool");
        DoRunLoop();

        THEN("the thread indicates it want's to sleep")
        {
            r0 = nullptr;
        }

        AND_WHEN("time passes")
        {
            auto r_no_activation = NAMED_FORBID_CALL(*p0, OnActivation());
            AdvanceTimeAndRunLoop(9ms);

            THEN("the thread has not been activated")
            {
                r_no_activation = nullptr;
            }

            WHEN("the time expires")
            {
                auto r_activation = NAMED_REQUIRE_CALL(*p0, OnActivation()).RETURN(std::nullopt);
                AdvanceTimeAndRunLoop(1ms);

                THEN("the thread executes")
                {
                    r_activation = nullptr;
                }
            }
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "a single pooled thread can be removed")
{
    auto [p0_up, p0] = CreatePooledThread();

    job_thread->AttachPooledThread(std::move(p0_up));

    WHEN("the thread is removed before start")
    {
        auto r0 = NAMED_FORBID_CALL(*p0, OnActivation());

        p0->DoStop();
        job_thread->Start("job_pool");
        DoRunLoop();

        THEN("the thread is not run")
        {
            r0 = nullptr;
        }
    }

    WHEN("the thread is removed while waiting for a timeout")
    {
        auto r0 = NAMED_REQUIRE_CALL(*p0, OnActivation()).RETURN(10ms);
        job_thread->Start("job_pool");
        AdvanceTimeAndRunLoop(5ms);

        r0 = NAMED_FORBID_CALL(*p0, OnActivation());
        p0->DoStop();
        AdvanceTimeAndRunLoop(5ms);

        THEN("the thread is not run")
        {
            r0 = nullptr;
        }
    }

    WHEN("the thread is removed after have being woke")
    {
        auto r0 = NAMED_REQUIRE_CALL(*p0, OnActivation()).RETURN(std::nullopt);
        job_thread->Start("job_pool");
        DoRunLoop();

        r0 = NAMED_FORBID_CALL(*p0, OnActivation());
        // Wake via an event
        p0->Awake();
        p0->DoStop();
        DoRunLoop();

        THEN("the thread is not run")
        {
            r0 = nullptr;
        }
    }

    WHEN("the thread is removed during activation")
    {
        auto l_p0 = p0;
        auto r_no_activation = NAMED_FORBID_CALL(*p0, OnActivation());
        REQUIRE_CALL(*p0, OnActivation()).RETURN(0ms).LR_SIDE_EFFECT(l_p0->DoStop());
        job_thread->Start("job_pool");
        DoRunLoop();

        THEN("the thread is not run again")
        {
            r_no_activation = nullptr;
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "timers are used in a single pooled thread")
{
}

TEST_CASE_FIXTURE(Fixture, "two pooled threads are created")
{
    GIVEN("two pooled threads")
    {
        auto [p0_up, p0] = CreatePooledThread();
        auto [p1_up, p1] = CreatePooledThread();

        job_thread->AttachPooledThread(std::move(p0_up));
        job_thread->AttachPooledThread(std::move(p1_up));

        THEN("both are started on startup")
        {
            REQUIRE_CALL(*p0, OnStartup());
            REQUIRE_CALL(*p1, OnStartup());
            REQUIRE_CALL(*p0, OnActivation()).RETURN(std::nullopt);
            REQUIRE_CALL(*p1, OnActivation()).RETURN(std::nullopt);

            job_thread->Start("job_pool");
            DoRunLoop();
        }

        WHEN("one thread waits for event and one on a timer")
        {
            REQUIRE_CALL(*p0, OnActivation()).RETURN(std::nullopt);
            REQUIRE_CALL(*p1, OnActivation()).RETURN(10ms);

            job_thread->Start("job_pool");
            DoRunLoop();

            WHEN("the event occurs")
            {
                auto r_p0_on_event = NAMED_REQUIRE_CALL(*p0, OnActivation()).RETURN(std::nullopt);
                FORBID_CALL(*p1, OnActivation());

                p0->Awake();
                DoRunLoop();

                THEN("only p0 is executed")
                {
                    r_p0_on_event = nullptr;
                }
            }

            WHEN("the timer occurs")
            {
                AdvanceTimeAndRunLoop(9ms);
                auto r_p1_on_timer = NAMED_REQUIRE_CALL(*p1, OnActivation()).RETURN(std::nullopt);
                FORBID_CALL(*p0, OnActivation());

                AdvanceTimeAndRunLoop(1ms);

                THEN("only p1 is executed")
                {
                    r_p1_on_timer = nullptr;
                }
            }
        }

        WHEN("both threads are runnable all the time")
        {
            auto r0 = NAMED_REQUIRE_CALL(*p0, OnActivation()).RETURN(0ms);
            auto r1 = NAMED_REQUIRE_CALL(*p1, OnActivation()).RETURN(0ms);

            job_thread->Start("job_pool");
            DoRunLoop();

            THEN("they are both executed immediately")
            {
                r0 = nullptr;
                r1 = nullptr;
            }
        }

        WHEN("a thread removes the other during startup")
        {
            auto l_p1 = p1; // Bindings not popular with older clangs
            REQUIRE_CALL(*p0, OnActivation()).RETURN(std::nullopt).LR_SIDE_EFFECT(l_p1->DoStop());
            auto r_no_p1 = NAMED_FORBID_CALL(*p1, OnActivation());
            DoRunLoop();

            THEN("p1 isn't executed")
            {
                r_no_p1 = nullptr;
            }
        }

        WHEN("the removal order is reverse, after startup")
        {
            REQUIRE_CALL(*p0, OnActivation()).RETURN(1ms);
            REQUIRE_CALL(*p1, OnActivation()).RETURN(0ms);

            job_thread->Start("job_pool");
            DoRunLoop();

            auto l_p0 = p0;
            REQUIRE_CALL(*p1, OnActivation()).RETURN(std::nullopt).LR_SIDE_EFFECT(l_p0->DoStop());
            auto r_no_p0 = NAMED_FORBID_CALL(*p0, OnActivation());

            AdvanceTimeAndRunLoop(1ms);

            THEN("p1 isn't executed")
            {
                r_no_p0 = nullptr;
            }
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "pooled threads can use notifications")
{
    GIVEN("two pooled threads")
    {
        auto [p0_up, p0] = CreatePooledThread();
        auto [p1_up, p1] = CreatePooledThread();

        REQUIRE_CALL(*p0, OnActivation()).RETURN(std::nullopt);
        REQUIRE_CALL(*p1, OnActivation()).RETURN(std::nullopt);
        job_thread->AttachPooledThread(std::move(p0_up));
        job_thread->AttachPooledThread(std::move(p1_up));

        WHEN("one thread is notified")
        {
            auto as_notifier = static_cast<IEventNotifier*>(p0);

            auto r_activation = NAMED_REQUIRE_CALL(*p0, OnActivation()).RETURN(std::nullopt);

            as_notifier->Notify();
            DoRunLoop();

            THEN("it's activated")
            {
                r_activation = nullptr;
            }
        }
    }
}

TEST_SUITE_END();
