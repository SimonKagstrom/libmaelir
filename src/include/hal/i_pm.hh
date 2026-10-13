#pragma once

#include "../listener_cookie.hh"

#include <memory>

namespace hal
{

class IPm
{
public:
    class ILock
    {
    public:
        virtual ~ILock() = default;

        // From thread context
        ListenerCookie FullPower()
        {
            Lock();
            return ListenerCookie([this]() { Unlock(); });
        }


        // From ISR
        virtual void Lock() = 0;
        virtual void Unlock() = 0;
    };

    virtual ~IPm() = default;

    virtual std::unique_ptr<ILock> CreateFullPowerLock() = 0;
};

} // namespace hal
