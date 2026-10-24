// Messages used by libmaelir itself, which all applications include in their AllMessages
#pragma once

#include <cstdint>
#include <optional>
#include <tuple>

namespace MSG
{

// Time from the phone (e.g., Gadgetbridge)
struct network_time
{
    // Seconds since the epoch
    uint32_t utc;
    // Offset from UTC (including DST), if sent
    std::optional<int16_t> timezone_minutes;
};

using CommonMessages = std::tuple<network_time>;

} // namespace MSG
