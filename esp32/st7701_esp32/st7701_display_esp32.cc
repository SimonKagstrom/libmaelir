#include "st7701_display_esp32.hh"

#include <esp_heap_caps.h>
#include <esp_idf_version.h>
#include <esp_lcd_panel_ops.h>

DisplaySt7701::DisplaySt7701(const st7701::MipiPanel& panel, const BoardConfig& board)
{
    esp_ldo_channel_config_t ldo_config {};
    ldo_config.chan_id = board.phy_ldo_channel;
    ldo_config.voltage_mv = board.phy_ldo_voltage_mv;
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_config, &m_phy_ldo));

    // phy_clk_src is left at default, which esp-idf selects based on the minimum chip revision
    esp_lcd_dsi_bus_config_t bus_config {};
    bus_config.bus_id = 0;
    bus_config.num_data_lanes = panel.num_data_lanes;
    bus_config.lane_bit_rate_mbps = panel.lane_bit_rate_mbps;
    bus_config.flags.clock_lane_force_hs = false;
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &m_dsi_bus));

    esp_lcd_dbi_io_config_t dbi_config {};
    dbi_config.virtual_channel = 0;
    dbi_config.lcd_cmd_bits = 8;
    dbi_config.lcd_param_bits = 8;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(m_dsi_bus, &dbi_config, &m_dbi_io));

    // The frame buffer handling below assumes double-buffered RGB565
    esp_lcd_dpi_panel_config_t dpi_config {};
    dpi_config.virtual_channel = 0;
    dpi_config.dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT;
    dpi_config.dpi_clock_freq_mhz = panel.dpi_clock_freq_mhz;
    dpi_config.in_color_format = LCD_COLOR_FMT_RGB565;
    dpi_config.out_color_format = LCD_COLOR_FMT_RGB565;
    dpi_config.num_fbs = 2;
    dpi_config.video_timing = panel.timing;
    dpi_config.flags.disable_lp = false;

    // The ST7701 driver keeps a pointer to the init commands, the rest is copied
    st7701_vendor_config_t vendor_config {};
    vendor_config.init_cmds = panel.init_cmds.data();
    vendor_config.init_cmds_size = static_cast<uint16_t>(panel.init_cmds.size());
    vendor_config.mipi_config.dsi_bus = m_dsi_bus;
    vendor_config.mipi_config.dpi_config = &dpi_config;
    vendor_config.flags.use_mipi_interface = 1;

    esp_lcd_panel_dev_config_t panel_config {};
    panel_config.reset_gpio_num = board.reset_pin;
    panel_config.bits_per_pixel = 16;
    panel_config.rgb_ele_order = panel.rgb_element_order;
    panel_config.data_endian = LCD_RGB_DATA_ENDIAN_BIG;
    panel_config.vendor_config = &vendor_config;

    ESP_ERROR_CHECK(esp_lcd_new_panel_st7701(m_dbi_io, &panel_config, &m_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(m_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(m_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(m_panel_handle, true));

    ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(
        m_panel_handle, 2, (void**)&m_frame_buffers[0], (void**)&m_frame_buffers[1]));

    // An extra once for the rotation buffer
    m_frame_buffers[2] = reinterpret_cast<uint16_t*>(
        heap_caps_aligned_calloc(CONFIG_CACHE_L2_CACHE_LINE_SIZE,
                                 1,
                                 hal::kDisplayWidth * hal::kDisplayHeight * 2,
                                 MALLOC_CAP_SPIRAM | MALLOC_CAP_CACHE_ALIGNED));

    esp_lcd_dpi_panel_event_callbacks_t callbacks = {};
    auto on_frame_buf_complete =
        [](esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t* edata, void* user_ctx) {
            auto* self = static_cast<DisplaySt7701*>(user_ctx);
            self->OnVsync();
            return false;
        };
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 1, 0)
    callbacks.on_frame_buf_complete = on_frame_buf_complete;
#else
    callbacks.on_refresh_done = on_frame_buf_complete;
#endif

    ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(m_panel_handle, &callbacks, this));

    esp_lcd_dpi_panel_enable_dma2d(m_panel_handle);
}

uint16_t*
DisplaySt7701::GetFrameBuffer(hal::IDisplay::Owner owner)
{
    if (owner == hal::IDisplay::Owner::kRotationBuffer)
    {
        return m_frame_buffers[2];
    }
    if (owner == hal::IDisplay::Owner::kHardware)
    {
        return m_frame_buffers[!m_current_update_frame];
    }

    return m_frame_buffers[m_current_update_frame];
}

void
DisplaySt7701::Flip()
{
    esp_lcd_panel_draw_bitmap(m_panel_handle,
                              0,
                              0,
                              hal::kDisplayWidth,
                              hal::kDisplayHeight,
                              m_frame_buffers[m_current_update_frame]);

    m_current_update_frame = !m_current_update_frame;
    m_flip_requested = true;
    m_vsync_done.acquire();
}

void
DisplaySt7701::SetActive(bool active)
{
    m_active = active;
}

void
DisplaySt7701::OnVsync()
{
    if (m_flip_requested)
    {
        m_flip_requested = false;
        m_vsync_done.release_from_isr();
    }
}
