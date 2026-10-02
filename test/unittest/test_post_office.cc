#include "post_office.hh"
#include "thread_fixture.hh"
#include "unittest_messages.hh"

namespace
{

class MockEventNotifier : public IEventNotifier
{
public:
    MAKE_MOCK0(Notify, void(), final);

    void NotifyFromIsr() final
    {
        REQUIRE(false); // NotifyFromIsr should not be called by messages
    }
};

template <typename... Messages>
class ReceiverThread
{
public:
    ReceiverThread(PostOffice<MSG::AllMessages>& post_office)
    {
        m_mailbox = post_office.Subscribe<Messages...>(notifier);
    }

    MockEventNotifier notifier;
    std::unique_ptr<Mailbox<MSG::AllMessages>> m_mailbox;
};

class Fixture : public ThreadFixture
{
public:
    MAKE_MOCK0(Samsa, void());
    MAKE_MOCK0(Gregor, void());

    PostOffice<MSG::AllMessages> post_office;
};

} // namespace

TEST_SUITE_BEGIN("post_office");

TEST_CASE_FIXTURE(Fixture, "A message which noone listens to simply drops sent messages")
{
    post_office.Send<MSG::gregor>({"id=15", "name"});

    // Hard to verify, but at least no blue smoke
}

TEST_CASE_FIXTURE(Fixture, "messages are not queued up for late listeners")
{
    post_office.Send<MSG::samsa>({});

    auto samsa_listener = std::make_unique<ReceiverThread<MSG::samsa>>(post_office);
    CHECK(samsa_listener->m_mailbox->Pop() == std::nullopt);
}

TEST_CASE_FIXTURE(Fixture, "listeners get notified when they get messages")
{
    auto samsa_listener = std::make_unique<ReceiverThread<MSG::samsa>>(post_office);
    auto gregor_listener = std::make_unique<ReceiverThread<MSG::gregor>>(post_office);
    auto all_listener = std::make_unique<ReceiverThread<MSG::gregor, MSG::samsa>>(post_office);

    WHEN("a message is sent")
    {
        auto r_samsa = NAMED_REQUIRE_CALL(samsa_listener->notifier, Notify());
        auto r_all = NAMED_REQUIRE_CALL(all_listener->notifier, Notify());
        auto r_no_gregor = NAMED_FORBID_CALL(gregor_listener->notifier, Notify());


        post_office.Send<MSG::samsa>({});

        THEN("listeners are notified")
        {
            r_samsa = nullptr;
            r_all = nullptr;
        }
        AND_THEN("non-listeners aren't notified")
        {
            r_no_gregor = nullptr;
        }
    }

    WHEN("a mailbox is unregistered")
    {
        samsa_listener = nullptr;

        auto r_all = NAMED_REQUIRE_CALL(all_listener->notifier, Notify());
        post_office.Send<MSG::samsa>({});

        THEN("the other is still notified")
        {
            r_all = nullptr;
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "messages can be handled")
{
    auto gregor_listener = std::make_unique<ReceiverThread<MSG::gregor>>(post_office);
    auto samsa_listener = std::make_unique<ReceiverThread<MSG::samsa>>(post_office);
    ALLOW_CALL(gregor_listener->notifier, Notify());
    ALLOW_CALL(samsa_listener->notifier, Notify());

    WHEN("a message is sent to one of the tasks")
    {
        post_office.Send<MSG::gregor>({"id99", "Gregor Samsa"});

        THEN("the queue of the other listener is empty")
        {
            CHECK(samsa_listener->m_mailbox->Pop() == std::nullopt);
        }

        THEN("the queue can be popped on the listener that")
        {
            auto msg = gregor_listener->m_mailbox->Pop();
            REQUIRE(msg != std::nullopt);
            REQUIRE(msg->Is<MSG::gregor>());

            THEN("no messages remain")
            {
                REQUIRE(samsa_listener->m_mailbox->Pop() == std::nullopt);
            }

            AND_THEN("it can be handled")
            {
                auto gregor = msg->As<MSG::gregor>();

                CHECK(gregor->caller_number == "id99");
                CHECK(gregor->caller_name == "Gregor Samsa");
            }
        }
    }

    WHEN("the empty message is sent")
    {
        post_office.Send<MSG::samsa>({});

        THEN("it can be handled")
        {
            auto msg = samsa_listener->m_mailbox->Pop();
            REQUIRE(msg != std::nullopt);
            auto samsa = msg->As<MSG::samsa>();
            REQUIRE(msg->Is<MSG::samsa>());

            CHECK(samsa);
            // No fields in samsa
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "messages can be handled through callbacks")
{
    auto all_listener = std::make_unique<ReceiverThread<MSG::gregor, MSG::samsa>>(post_office);
    ALLOW_CALL(all_listener->notifier, Notify());

    WHEN("a single message comes in")
    {
        post_office.Send<MSG::samsa>({});
        THEN("only that is handled")
        {
            REQUIRE_CALL(*this, Samsa());
            while (auto msg = all_listener->m_mailbox->Pop())
            {
                msg->On<MSG::samsa>([&](auto samsa) { Samsa(); }).On<MSG::gregor>([&](auto gregor) {
                    Gregor();
                });
            }
        }
    }

    WHEN("multiple messages come in")
    {
        post_office.Send<MSG::samsa>({});
        post_office.Send<MSG::gregor>({"id=15", "name"});

        THEN("all are handled")
        {
            trompeloeil::sequence seq;
            REQUIRE_CALL(*this, Samsa()).IN_SEQUENCE(seq);
            REQUIRE_CALL(*this, Gregor()).IN_SEQUENCE(seq);

            while (auto msg = all_listener->m_mailbox->Pop())
            {
                msg->On<MSG::gregor>([&](auto gregor) { Gregor(); })
                    .On<MSG::samsa>([&](auto samsa) { Samsa(); });
            }
        }
    }
}

TEST_SUITE_END();
