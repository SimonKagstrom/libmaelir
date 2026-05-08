#pragma once
#include "hal/i_display.hh"
#include "semaphore.hh"

#include <atomic>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_st7701.h>

class DisplaySt7701 : public hal::IDisplay
{
public:
    DisplaySt7701(esp_lcd_panel_io_handle_t io_handle,
                  const esp_lcd_panel_dev_config_t& panel_config);

    uint16_t* GetFrameBuffer(hal::IDisplay::Owner owner) final;
    void Flip() final;

private:
    void SetActive(bool active) final;

    void OnVsync();

    esp_lcd_panel_handle_t m_panel_handle {nullptr};
    uint16_t* m_frame_buffers[3] {nullptr, nullptr, nullptr};
    std::atomic<uint8_t> m_current_update_frame {0};
    uint8_t m_current_rotation_frame {0};

    std::atomic_bool m_flip_requested {false};
    std::atomic_bool m_vsync_requested {false};
    std::atomic_bool m_active {true};

    os::binary_semaphore m_vsync_done {0};
};
