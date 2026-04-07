#pragma once

class IEventNotifier
{
public:
    virtual ~IEventNotifier() = default;

    virtual void Notify() = 0;

    virtual void NotifyFromIsr() = 0;
};
