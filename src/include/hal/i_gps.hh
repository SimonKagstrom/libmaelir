#pragma once

#include "event_notifier.hh"

#include <cstdint>
#include <memory>
#include <optional>

struct GpsPosition
{
    float latitude;
    float longitude;

    bool operator==(const GpsPosition& other) const = default;
};
static_assert(sizeof(GpsPosition) == 8);

namespace hal
{

struct RawGpsData
{
    std::optional<GpsPosition> position;
    std::optional<float> heading;
    std::optional<float> speed;
};

class IGps
{
public:
    virtual ~IGps() = default;

    /// @brief block waiting for data and signal the semaphore when it arrives
    virtual std::optional<RawGpsData> WaitForData(IEventNotifier& notifier) = 0;
};

} // namespace hal
