#include "job_pool_thread.hh"
#include "test.hh"
#include "thread_fixture.hh"


namespace
{

class PooledThread : public PooledThreadBase
{
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

    std::unique_ptr<JobPoolThread> job_thread {std::make_unique<JobPoolThread>()};
};

} // namespace

TEST_SUITE_BEGIN("job_pool_thread");

TEST_CASE_FIXTURE(Fixture, "OnStartup is called for all pooled threads on start")
{
    auto p0 = std::make_unique<PooledThread>();
    auto p1 = std::make_unique<PooledThread>();

    ALLOW_CALL(*p0, OnActivation()).RETURN(std::nullopt);
    ALLOW_CALL(*p1, OnActivation()).RETURN(std::nullopt);

    job_thread->AttachPooledThread(p0.get());
    job_thread->AttachPooledThread(p1.get());

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

        p0 = nullptr;
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
    auto p0 = std::make_unique<PooledThread>();
    job_thread->AttachPooledThread(p0.get());

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
    auto p0 = std::make_unique<PooledThread>();

    job_thread->AttachPooledThread(p0.get());

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
    auto p0 = std::make_unique<PooledThread>();

    job_thread->AttachPooledThread(p0.get());

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
    auto p0 = std::make_unique<PooledThread>();

    job_thread->AttachPooledThread(p0.get());

    WHEN("the thread is removed before start")
    {
        auto r0 = NAMED_FORBID_CALL(*p0, OnActivation());

        p0 = nullptr;
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
        p0 = nullptr;
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
        p0 = nullptr;
        DoRunLoop();

        THEN("the thread is not run")
        {
            r0 = nullptr;
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "two pooled threads are created")
{
}


TEST_SUITE_END();
