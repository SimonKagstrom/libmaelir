#include "post_office.hh"
#include "thread_fixture.hh"
#include "unittest_messages.hh"

namespace
{

class MockEventNotifier : public IEventNotifier
{
public:
    MAKE_MOCK0(Notify, void(), final);

    bool NotifyFromIsr() final
    {
        REQUIRE(false); // NotifyFromIsr should not be called by messages
        return false;
    }
};

template <typename... Messages>
class ReceiverThread
{
public:
    ReceiverThread(PostOffice<MSG::UnittestMessages>& post_office)
    {
        m_mailbox = post_office.Subscribe<Messages...>(notifier);
    }

    MockEventNotifier notifier;
    std::unique_ptr<Mailbox<MSG::UnittestMessages>> m_mailbox;
};

class Fixture : public ThreadFixture
{
public:
    MAKE_MOCK0(Samsa, void());
    MAKE_MOCK2(Gregor, void(std::string number, std::string name));

    void Collect(Mailbox<MSG::UnittestMessages>& mailbox)
    {
        mailbox.Collect()
            .On<MSG::samsa>([this]() { Samsa(); })
            .On<MSG::gregor>([this](auto gregor) {
                Gregor(gregor->caller_number, gregor->caller_name);
            });
    }

    PostOffice<MSG::UnittestMessages> post_office;
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
    post_office.Send<MSG::samsa>();

    auto samsa_listener = std::make_unique<ReceiverThread<MSG::samsa>>(post_office);
    FORBID_CALL(*this, Samsa());
    Collect(*samsa_listener->m_mailbox);
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


        post_office.Send<MSG::samsa>();

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
        post_office.Send<MSG::samsa>();

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

        THEN("the other listener doesn't receive it")
        {
            FORBID_CALL(*this, Samsa());
            FORBID_CALL(*this, Gregor(trompeloeil::_, trompeloeil::_));
            Collect(*samsa_listener->m_mailbox);
        }

        THEN("the listener receives it, with contents")
        {
            REQUIRE_CALL(*this, Gregor("id99", "Gregor Samsa"));
            Collect(*gregor_listener->m_mailbox);

            AND_THEN("it's not received twice")
            {
                FORBID_CALL(*this, Gregor(trompeloeil::_, trompeloeil::_));
                Collect(*gregor_listener->m_mailbox);
            }
        }
    }

    WHEN("the empty message is sent")
    {
        post_office.Send<MSG::samsa>();

        THEN("the message can be passed to the handler")
        {
            std::shared_ptr<const MSG::samsa> samsa;

            samsa_listener->m_mailbox->Collect().On<MSG::samsa>([&samsa](auto msg) { samsa = msg; });
            CHECK(samsa);
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "multiple messages are handled in order")
{
    auto all_listener = std::make_unique<ReceiverThread<MSG::gregor, MSG::samsa>>(post_office);
    ALLOW_CALL(all_listener->notifier, Notify());
    trompeloeil::sequence seq;

    WHEN("messages of different types come in")
    {
        post_office.Send<MSG::gregor>({"id=15", "name"});
        post_office.Send<MSG::samsa>();

        THEN("all are handled, regardless of the order of the handlers")
        {
            REQUIRE_CALL(*this, Gregor("id=15", "name")).IN_SEQUENCE(seq);
            REQUIRE_CALL(*this, Samsa()).IN_SEQUENCE(seq);

            Collect(*all_listener->m_mailbox);
        }
    }

    WHEN("several messages of the same type come in")
    {
        post_office.Send<MSG::gregor>({"1", "first"});
        post_office.Send<MSG::samsa>();
        post_office.Send<MSG::gregor>({"2", "second"});

        THEN("each is handled with its own contents")
        {
            REQUIRE_CALL(*this, Gregor("1", "first")).IN_SEQUENCE(seq);
            REQUIRE_CALL(*this, Samsa()).IN_SEQUENCE(seq);
            REQUIRE_CALL(*this, Gregor("2", "second")).IN_SEQUENCE(seq);

            Collect(*all_listener->m_mailbox);
        }
    }

    WHEN("a message has no handler")
    {
        post_office.Send<MSG::gregor>({"id=15", "name"});
        post_office.Send<MSG::samsa>();

        THEN("the others are still handled")
        {
            REQUIRE_CALL(*this, Samsa());
            all_listener->m_mailbox->Collect().On<MSG::samsa>([this]() { Samsa(); });

            AND_THEN("the unhandled message is dropped")
            {
                FORBID_CALL(*this, Gregor(trompeloeil::_, trompeloeil::_));
                Collect(*all_listener->m_mailbox);
            }
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "a full mailbox drops new messages")
{
    auto gregor_listener = std::make_unique<ReceiverThread<MSG::gregor>>(post_office);
    ALLOW_CALL(gregor_listener->notifier, Notify());

    for (auto i = 0; i < kMailboxSize + 2; i++)
    {
        post_office.Send<MSG::gregor>({std::to_string(i), "name"});
    }

    THEN("the oldest messages are kept")
    {
        trompeloeil::sequence seq;
        std::vector<std::unique_ptr<trompeloeil::expectation>> expectations;
        for (auto i = 0; i < kMailboxSize; i++)
        {
            expectations.push_back(
                NAMED_REQUIRE_CALL(*this, Gregor(std::to_string(i), "name")).IN_SEQUENCE(seq));
        }

        Collect(*gregor_listener->m_mailbox);
    }

    AND_THEN("new messages fit once it has been drained")
    {
        ALLOW_CALL(*this, Gregor(trompeloeil::_, trompeloeil::_));
        Collect(*gregor_listener->m_mailbox);

        post_office.Send<MSG::gregor>({"new", "name"});

        REQUIRE_CALL(*this, Gregor("new", "name"));
        Collect(*gregor_listener->m_mailbox);
    }
}

TEST_SUITE_END();
