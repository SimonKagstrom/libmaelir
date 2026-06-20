#pragma once

#include "hal/i_touch.hh"

#include <driver/i2c_master.h>
#include <esp_lcd_touch.h>
#include <esp_lcd_touch_gt911.h>
#include <etl/vector.h>

class TouchEsp32 : public hal::ITouch
{
public:
    explicit TouchEsp32(const i2c_master_bus_config_t& i2c_mst_config, gpio_num_t interrupt_pin);

private:
    std::unique_ptr<ListenerCookie> AttachIrqListener(std::function<void()> on_state_changed) final;

    std::span<const hal::ITouch::Data> GetActiveTouchData() final;

    const gpio_num_t m_interrupt_pin;
    esp_lcd_touch_handle_t m_tp;
    std::function<void()> m_on_state_changed_callback {[]() {}};
    etl::vector<hal::ITouch::Data, 10> m_touch_data_buffer;
    bool m_was_pressed {false};
    uint16_t m_last_x {0};
    uint16_t m_last_y {0};
};