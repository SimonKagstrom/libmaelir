#pragma once
#include "hal/i_display.hh"
#include "semaphore.hh"
#include "st7701_mipi_panel.hh"

#include <atomic>
#include <driver/gpio.h>
#include <esp_lcd_mipi_dsi.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_ldo_regulator.h>

class DisplaySt7701 : public hal::IDisplay
{
public:
    // How the panel is connected on the board
    struct BoardConfig
    {
        gpio_num_t reset_pin;
        // The LDO channel powering VDD_MIPI_DPHY
        int phy_ldo_channel;
        int phy_ldo_voltage_mv;
    };

    DisplaySt7701(const st7701::MipiPanel& panel, const BoardConfig& board);

    uint16_t* GetFrameBuffer(hal::IDisplay::Owner owner) final;
    void Flip() final;

private:
    void SetActive(bool active) final;

    void OnVsync();

    esp_ldo_channel_handle_t m_phy_ldo {nullptr};
    esp_lcd_dsi_bus_handle_t m_dsi_bus {nullptr};
    esp_lcd_panel_io_handle_t m_dbi_io {nullptr};
    esp_lcd_panel_handle_t m_panel_handle {nullptr};
    uint16_t* m_frame_buffers[3] {nullptr, nullptr, nullptr};
    std::atomic<uint8_t> m_current_update_frame {0};
    uint8_t m_current_rotation_frame {0};

    std::atomic_bool m_flip_requested {false};
    std::atomic_bool m_vsync_requested {false};
    std::atomic_bool m_active {true};

    os::binary_semaphore m_vsync_done {0};
};
