#include "touch_esp32.hh"

TouchEsp32::TouchEsp32(esp_lcd_touch_handle_t tp)
    : m_tp(tp)
{
}

std::unique_ptr<ListenerCookie>
TouchEsp32::AttachIrqListener(std::function<void()> on_state_changed)
{
    return nullptr;
}

std::span<const hal::ITouch::Data>
TouchEsp32::GetActiveTouchData()
{
    m_touch_data_buffer.clear();

    if (esp_lcd_touch_read_data(m_tp) != ESP_OK)
    {
        return m_touch_data_buffer;
    }

    const bool pressed_now = m_tp->data.points > 0;

    if (!pressed_now)
    {
        if (m_was_pressed)
        {
            m_touch_data_buffer.push_back(
                {.pressed = false, .was_pressed = true, .x = m_last_x, .y = m_last_y});
        }
        m_was_pressed = false;
        return m_touch_data_buffer;
    }

    for (int i = 0; i < m_tp->data.points; ++i)
    {
        const auto& point = m_tp->data.coords[i];
        m_touch_data_buffer.push_back(
            {.pressed = true, .was_pressed = m_was_pressed, .x = point.x, .y = point.y});

        if (i == 0)
        {
            m_last_x = point.x;
            m_last_y = point.y;
        }
    }

    m_was_pressed = true;

    return m_touch_data_buffer;
}
