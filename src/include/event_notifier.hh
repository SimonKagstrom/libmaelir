#pragma once

class IEventNotifier
{
public:
    virtual ~IEventNotifier() = default;

    virtual void Notify() = 0;

    // Returns true if a higher priority task was woken, i.e., the ISR should yield on exit
    virtual bool NotifyFromIsr() = 0;
};
