#pragma once

#include "../listener_cookie.hh"

#include <functional>
#include <memory>
#include <span>

namespace hal
{

class ITouch
{
public:
    struct Data
    {
        bool pressed;
        bool was_pressed;
        uint16_t x;
        uint16_t y;
    };

    virtual ~ITouch() = default;

    virtual std::unique_ptr<ListenerCookie>
    AttachIrqListener(std::function<void()> on_state_changed) = 0;

    virtual std::span<const hal::ITouch::Data> GetActiveTouchData() = 0;
};

} // namespace hal