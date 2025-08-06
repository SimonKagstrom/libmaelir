#pragma once

#include "hal/i_gpio.hh"

class GpioHost final : public hal::IGpio
{
public:
    GpioHost() = default;

    void SetState(bool state) override;
    bool GetState() const override;

    std::unique_ptr<ListenerCookie>
    AttachIrqListener(std::function<void(bool)> on_state_change) override;

private:
    bool m_state {false};
    std::vector<std::function<void(bool)>> m_listeners;
};
