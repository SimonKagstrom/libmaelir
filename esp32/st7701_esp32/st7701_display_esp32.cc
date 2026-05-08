#include "st7701_display_esp32.hh"

#include <esp_heap_caps.h>
#include <esp_lcd_panel_ops.h>

DisplaySt7701::DisplaySt7701(esp_lcd_panel_io_handle_t io_handle,
                             const esp_lcd_panel_dev_config_t& panel_config)
{
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7701(io_handle, &panel_config, &m_panel_handle));
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
    callbacks.on_refresh_done =
        [](esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t* edata, void* user_ctx) {
            auto* self = static_cast<DisplaySt7701*>(user_ctx);
            self->OnVsync();
            return false;
        };

    ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(m_panel_handle, &callbacks, this));
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
