#pragma once

#include <esp_lcd_st7701.h>
#include <esp_lcd_types.h>
#include <span>

namespace st7701
{

// The properties of a specific panel module (glass + ST7701 controller). These are given by the
// panel vendor, and are not interchangeable between modules even if they use the same controller
struct MipiPanel
{
    // Gamma, power and gate timing (GIP) setup for the glass
    std::span<const st7701_lcd_init_cmd_t> init_cmds;

    // Must match the porch settings in the init commands
    esp_lcd_video_timing_t timing;
    uint32_t dpi_clock_freq_mhz;

    uint8_t num_data_lanes;
    uint32_t lane_bit_rate_mbps;

    lcd_rgb_element_order_t rgb_element_order {LCD_RGB_ELEMENT_ORDER_RGB};
};

} // namespace st7701
