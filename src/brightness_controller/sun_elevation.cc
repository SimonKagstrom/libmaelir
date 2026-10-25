#include "sun_elevation.hh"

#include <cmath>
#include <numbers>

namespace
{

// 2000-01-01 12:00 UTC
constexpr int64_t kJ2000 = 946728000;
constexpr int64_t kSecondsPerDay = 24 * 60 * 60;
constexpr float kFullCircle = 360;

constexpr float
ToRadians(float degrees)
{
    return degrees * std::numbers::pi_v<float> / 180;
}

constexpr float
ToDegrees(float radians)
{
    return radians * 180 / std::numbers::pi_v<float>;
}

} // namespace

// The approximate solar coordinates from the US Naval Observatory (accurate to ~1 arcminute)
float
SunElevation(uint32_t utc, const GpsPosition& position)
{
    // Days since J2000. The fraction of the day is kept separately, since the sidereal time
    // below grows by ~361° per day, which would lose too much precision in a float
    const auto seconds = static_cast<int64_t>(utc) - kJ2000;
    const auto day_fraction = static_cast<float>(seconds % kSecondsPerDay) / kSecondsPerDay;
    const auto d = static_cast<float>(seconds / kSecondsPerDay) + day_fraction;

    const auto mean_anomaly = ToRadians(std::fmod(357.529f + 0.98560028f * d, kFullCircle));
    const auto mean_longitude = std::fmod(280.459f + 0.98564736f * d, kFullCircle);
    const auto ecliptic_longitude = ToRadians(mean_longitude + 1.915f * std::sin(mean_anomaly) +
                                              0.020f * std::sin(2 * mean_anomaly));
    const auto obliquity = ToRadians(23.439f - 0.00000036f * d);

    const auto right_ascension = std::atan2(std::cos(obliquity) * std::sin(ecliptic_longitude),
                                            std::cos(ecliptic_longitude));
    const auto declination = std::asin(std::sin(obliquity) * std::sin(ecliptic_longitude));

    // Greenwich mean sidereal time, in degrees: 280.46061837 + 360.98564736629 * d, where the
    // 360° per whole day is dropped
    const auto gmst =
        std::fmod(280.46061837f + 0.98564736629f * d + kFullCircle * day_fraction, kFullCircle);
    const auto hour_angle = ToRadians(gmst + position.longitude) - right_ascension;

    const auto latitude = ToRadians(position.latitude);
    const auto sin_elevation = std::sin(latitude) * std::sin(declination) +
                               std::cos(latitude) * std::cos(declination) * std::cos(hour_angle);

    return ToDegrees(std::asin(sin_elevation));
}
