#include "gpio_esp32.hh"

GpioEsp32::GpioEsp32(uint8_t pin, GpioEsp32::Polarity polarity)
    : m_pin(static_cast<gpio_num_t>(pin))
    , m_polarity(polarity == Polarity::kActiveHigh ? 0 : 1)
{
    gpio_isr_handler_add(m_pin, GpioEsp32::StaticButtonIsr, static_cast<void*>(this));
}

void
GpioEsp32::SetState(bool state)
{
    gpio_set_level(m_pin, state ^ m_polarity);
}

bool
GpioEsp32::GetState() const
{
    return gpio_get_level(m_pin) ^ m_polarity;
}

std::unique_ptr<ListenerCookie>
GpioEsp32::AttachIrqListener(std::function<void(bool)> on_state_change)
{
    m_on_state_change = std::move(on_state_change);

    return std::make_unique<ListenerCookie>([this]() { m_on_state_change = [](auto) {}; });
}

void
GpioEsp32::ButtonIsr()
{
    auto state = GetState();
    m_on_state_change(state);
}

void
GpioEsp32::StaticButtonIsr(void* arg)
{
    auto self = static_cast<GpioEsp32*>(arg);
    self->ButtonIsr();
}
