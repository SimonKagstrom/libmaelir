#pragma once

#include <cstdint>

enum class BrightnessMode : uint8_t
{
    kLow,
    kMedium,
    kMax,
    // Depending on the sun elevation at the current position
    kAuto,

    kValueCount,
};
