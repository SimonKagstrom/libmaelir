#pragma once

#include "hal/i_touch.hh"

#include <esp_lcd_touch.h>
#include <etl/vector.h>

class TouchEsp32 : public hal::ITouch
{
public:
    explicit TouchEsp32(esp_lcd_touch_handle_t tp);

private:
    std::unique_ptr<ListenerCookie> AttachIrqListener(std::function<void()> on_state_changed) final;

    std::span<const hal::ITouch::Data> GetActiveTouchData() final;

    esp_lcd_touch_handle_t m_tp;
    etl::vector<hal::ITouch::Data, 10> m_touch_data_buffer;
    bool m_was_pressed {false};
    uint16_t m_last_x {0};
    uint16_t m_last_y {0};
};