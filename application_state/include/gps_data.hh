#pragma once

#include "hal/i_gps.hh"

#include <cstdint>

enum class GpsStatus : uint8_t
{
    kSilent,
    kNoFix,
    kPositionValid,

    kValueCount,
};

struct GpsData
{
    GpsPosition position;

    float speed;   ///< Speed in knots
    float heading; ///< Heading in degrees

    // Add time, height, etc.
    bool operator==(const GpsData& other) const = default;
};
