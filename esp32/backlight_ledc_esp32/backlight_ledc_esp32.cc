#include "backlight_ledc_esp32.hh"

#include <algorithm>
#include <esp_err.h>

BacklightLedcEsp32::BacklightLedcEsp32(const Config& config)
    : m_channel(config.channel)
{
    ledc_timer_config_t timer_config {};
    timer_config.speed_mode = kSpeedMode;
    timer_config.duty_resolution = kDutyResolution;
    timer_config.timer_num = config.timer;
    timer_config.freq_hz = config.frequency_hz;
    timer_config.clk_cfg = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

    ledc_channel_config_t channel_config {};
    channel_config.gpio_num = config.pin;
    channel_config.speed_mode = kSpeedMode;
    channel_config.channel = config.channel;
    channel_config.timer_sel = config.timer;
    channel_config.duty = 0;
    channel_config.flags.output_invert = config.polarity == Polarity::kActiveLow;
    ESP_ERROR_CHECK(ledc_channel_config(&channel_config));
}

void
BacklightLedcEsp32::SetBrightness(uint8_t percent)
{
    // A duty of 1 << resolution is fully on
    const auto duty = (std::min<uint32_t>(percent, 100) << kDutyResolution) / 100;

    ledc_set_duty(kSpeedMode, m_channel, duty);
    ledc_update_duty(kSpeedMode, m_channel);
}
