#pragma once

#include "application_state.hh"
#include "hal/i_display_brightness.hh"
#include "pooled_thread_base.hh"

#include <optional>

// The brightness levels used in auto mode, darkest first
enum class BrightnessLevel : uint8_t
{
    kNight,
    kDusk,
    kLowSun,
    kDay,
};

/**
 * @brief the auto brightness level for a sun elevation
 *
 * A ratchet: when the sun is setting, the level never goes up, and when it's rising, never down.
 * Without a current level, it's taken directly from the elevation.
 */
BrightnessLevel
AutoBrightnessLevel(float sun_elevation, bool sun_rising, std::optional<BrightnessLevel> current);

/**
 * Sets the display brightness from AS::brightness_mode. In auto mode, the level follows the sun
 * elevation at the current position (the fallback position until there is a GPS fix), and is
 * re-evaluated once a minute.
 */
class BrightnessController : public PooledThreadBase
{
public:
    /// The display brightness in percent for each level, depending on the display
    struct Levels
    {
        uint8_t night;
        // Also used for BrightnessMode::kLow
        uint8_t dusk;
        // Also used for BrightnessMode::kMedium
        uint8_t low_sun;
        // Also used for BrightnessMode::kMax, and in auto mode before the time is known
        uint8_t day;
    };

    BrightnessController(hal::IDisplayBrightness& display,
                         ApplicationState& state,
                         const Levels& levels,
                         const GpsPosition& fallback_position);

    std::optional<milliseconds> OnActivation() final;

private:
    uint8_t AutoBrightness();

    uint8_t LevelPercent(BrightnessLevel level) const;

    hal::IDisplayBrightness& m_display;
    ApplicationState& m_state;
    const Levels m_levels;
    ListenerCookie m_state_listener;

    GpsPosition m_position;
    std::optional<BrightnessLevel> m_auto_level;
};
