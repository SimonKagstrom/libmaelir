#pragma once

#include "../listener_cookie.hh"
#include "semaphore.hh"
#include "time.hh"

#include <etl/vector.h>
#include <optional>

namespace hal
{
class ICan
{
public:
    static constexpr size_t kMaxDataLength = 8;

    struct Frame
    {
        uint32_t id {0};
        milliseconds timestamp {0};

        etl::vector<uint8_t, kMaxDataLength> data;
    };

    virtual ~ICan() = default;

    [[nodiscard]] virtual std::unique_ptr<ListenerCookie> Start(os::binary_semaphore& sem) = 0;
    virtual void Stop() = 0;

    virtual bool SendFrame(uint32_t id, std::span<const uint8_t> data) = 0;

    virtual std::optional<Frame> ReceiveFrame() = 0;
};

} // namespace hal
