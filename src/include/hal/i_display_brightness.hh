#pragma once

#include <cstdint>

namespace hal
{

class IDisplayBrightness
{
public:
    virtual ~IDisplayBrightness() = default;

    /// @brief set the backlight brightness, in percent (0 = off, 100 = max)
    virtual void SetBrightness(uint8_t percent) = 0;
};

} // namespace hal
