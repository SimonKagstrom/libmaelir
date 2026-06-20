#include "touch_esp32.hh"

#include "i_display_properties.hh"

TouchEsp32::TouchEsp32(const i2c_master_bus_config_t &i2c_mst_config, gpio_num_t interrupt_pin)
    : m_interrupt_pin(interrupt_pin)
{
    // TODO: All this is hardcoded for this particular case...
    i2c_master_bus_handle_t i2c_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &i2c_handle));

    esp_lcd_panel_io_i2c_config_t io_config = {.dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS,
                                               .scl_speed_hz = 100000,
                                               .control_phase_bytes = 1,
                                               .dc_bit_offset = 0,
                                               .lcd_cmd_bits = 16,
                                               .lcd_param_bits = 0,
                                               .on_color_trans_done = nullptr,
                                               .user_ctx = nullptr,
                                               .flags = {
                                                   .dc_low_on_data = 0,
                                                   .disable_control_phase = 1,
                                               }};

    esp_lcd_touch_io_gt911_config_t tp_gt911_config = {
        .dev_addr = static_cast<uint8_t>(io_config.dev_addr),
    };

    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = hal::kDisplayWidth,
        .y_max = hal::kDisplayHeight,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = m_interrupt_pin,
        .levels =
            {
                .reset = 0,
                .interrupt = 0,
            },
        .flags =
            {
                .swap_xy = 1,
                .mirror_x = 0,
                .mirror_y = 1,
            },
        .process_coordinates = nullptr,
        .interrupt_callback =
            [](esp_lcd_touch_handle_t tp) {
                auto p = static_cast<TouchEsp32*>(tp->config.user_data);

                p->m_on_state_changed_callback();
            },
        .user_data = this,
        .driver_data = &tp_gt911_config,
    };
    esp_lcd_panel_io_handle_t tp_io_handle = nullptr;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_handle, &io_config, &tp_io_handle));
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &m_tp));
}

std::unique_ptr<ListenerCookie>
TouchEsp32::AttachIrqListener(std::function<void()> on_state_changed)
{
    if (m_interrupt_pin == GPIO_NUM_NC)
    {
        return nullptr;
    }

    m_on_state_changed_callback = std::move(on_state_changed);
    return std::make_unique<ListenerCookie>([this]() { m_on_state_changed_callback = []() {}; });
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
        auto point = m_tp->data.coords[i];

        std::swap(point.x, point.y);
        point.x = hal::kDisplayWidth - point.x;

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
