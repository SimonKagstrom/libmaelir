#pragma once

#include "event_notifier.hh"
#include "test.hh"

/**
 * @brief Mock of a notifier, e.g., to check when listeners are notified
 */
class MockEventNotifier : public IEventNotifier
{
public:
    MAKE_MOCK0(Notify, void(), final);

private:
    bool NotifyFromIsr() final
    {
        REQUIRE(false); // Forbidden in unit tests
        return false;
    }
};

/**
 * @brief Notifier which does nothing, for when the notifications don't matter
 */
class NullEventNotifier : public IEventNotifier
{
public:
    void Notify() final
    {
    }

    bool NotifyFromIsr() final
    {
        return false;
    }
};
