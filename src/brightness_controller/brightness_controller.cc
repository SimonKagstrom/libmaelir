#include "brightness_controller.hh"

#include "sun_elevation.hh"

namespace
{

// How far ahead to look to see if the sun is rising or setting
constexpr auto kSunDirectionSeconds = 5 * 60;

BrightnessLevel
LevelFromElevation(float sun_elevation)
{
    if (sun_elevation < -6)
    {
        // Darker than civil twilight
        return BrightnessLevel::kNight;
    }
    if (sun_elevation < 0)
    {
        return BrightnessLevel::kDusk;
    }
    if (sun_elevation < 10)
    {
        return BrightnessLevel::kLowSun;
    }

    return BrightnessLevel::kDay;
}

} // namespace

BrightnessLevel
AutoBrightnessLevel(float sun_elevation, bool sun_rising, std::optional<BrightnessLevel> current)
{
    const auto target = LevelFromElevation(sun_elevation);

    if (!current)
    {
        return target;
    }

    return sun_rising ? std::max(*current, target) : std::min(*current, target);
}


BrightnessController::BrightnessController(hal::IDisplayBrightness& display,
                                           ApplicationState& state,
                                           const Levels& levels,
                                           const GpsPosition& fallback_position)
    : m_display(display)
    , m_state(state)
    , m_levels(levels)
    , m_state_listener(m_state.AttachListener<AS::gps_status, AS::brightness_mode>(*this))
    , m_position(fallback_position)
{
}

std::optional<milliseconds>
BrightnessController::OnActivation()
{
    if (m_state.Get<AS::gps_status>() == GpsStatus::kPositionValid)
    {
        // Kept also when the fix is lost
        m_position = m_state.Get<AS::position>()->position;
    }

    const auto mode = m_state.Get<AS::brightness_mode>();
    if (mode != BrightnessMode::kAuto)
    {
        // Start over when auto is selected again
        m_auto_level = std::nullopt;
    }

    uint8_t percent = m_levels.day;
    switch (mode)
    {
    case BrightnessMode::kLow:
        percent = m_levels.dusk;
        break;
    case BrightnessMode::kMedium:
        percent = m_levels.low_sun;
        break;
    case BrightnessMode::kMax:
    case BrightnessMode::kValueCount:
        break;
    case BrightnessMode::kAuto:
        percent = AutoBrightness();
        break;
    }

    m_display.SetBrightness(percent);

    return 1min;
}

uint8_t
BrightnessController::AutoBrightness()
{
    const auto utc = m_state.Get<AS::clock>();
    if (utc == 0)
    {
        // The time is not yet known
        return m_levels.day;
    }

    const auto elevation = SunElevation(utc, m_position);
    const auto sun_rising = SunElevation(utc + kSunDirectionSeconds, m_position) > elevation;

    m_auto_level = AutoBrightnessLevel(elevation, sun_rising, m_auto_level);

    return LevelPercent(*m_auto_level);
}

uint8_t
BrightnessController::LevelPercent(BrightnessLevel level) const
{
    switch (level)
    {
    case BrightnessLevel::kNight:
        return m_levels.night;
    case BrightnessLevel::kDusk:
        return m_levels.dusk;
    case BrightnessLevel::kLowSun:
        return m_levels.low_sun;
    case BrightnessLevel::kDay:
        break;
    }

    return m_levels.day;
}
