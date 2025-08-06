#include "gpio_host.hh"

void
GpioHost::SetState(bool state)
{
    m_state = state;

    for (auto& listener : m_listeners)
    {
        listener(state);
    }
}

bool
GpioHost::GetState() const
{
    return m_state;
}

std::unique_ptr<ListenerCookie>
GpioHost::AttachIrqListener(std::function<void(bool)> on_state_change)
{
    m_listeners.push_back(std::move(on_state_change));

    return std::make_unique<ListenerCookie>([this, on_state_change]() {
        // TODO: Erase
    });
}
