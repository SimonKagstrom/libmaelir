#pragma once

#include "hal/i_gps.hh"

#include <cstdint>

/**
 * @brief the elevation of the sun center above the horizon, in degrees
 *
 * Geometric, i.e., without atmospheric refraction (which adds ~0.5° at the horizon). The
 * accuracy is well below a degree, which is plenty for deciding between day and night.
 *
 * @param utc the time, seconds since the epoch
 * @param position the observer position
 */
float SunElevation(uint32_t utc, const GpsPosition& position);
