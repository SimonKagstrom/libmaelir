#pragma once

#include "hal/i_display_brightness.hh"

#include <driver/gpio.h>
#include <driver/ledc.h>

// Display backlight brightness via LEDC PWM. Starts out off.
class BacklightLedcEsp32 : public hal::IDisplayBrightness
{
public:
    enum class Polarity
    {
        kActiveHigh,
        kActiveLow,

        kValueCount,
    };

    struct Config
    {
        gpio_num_t pin;
        Polarity polarity {Polarity::kActiveHigh};
        uint32_t frequency_hz {5000};
        ledc_timer_t timer {LEDC_TIMER_0};
        ledc_channel_t channel {LEDC_CHANNEL_0};
    };

    explicit BacklightLedcEsp32(const Config& config);

    void SetBrightness(uint8_t percent) final;

private:
    static constexpr auto kSpeedMode = LEDC_LOW_SPEED_MODE;
    static constexpr auto kDutyResolution = LEDC_TIMER_10_BIT;

    const ledc_channel_t m_channel;
};
