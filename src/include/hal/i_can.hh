#pragma once

#include "../listener_cookie.hh"
#include "semaphore.hh"
#include "time.hh"

#include <array>
#include <optional>
#include <ranges>
#include <span>

namespace hal
{
class ICan
{
public:
    static constexpr size_t kMaxDataLength = 8;

    class Frame
    {
    public:
        // For readers
        auto Data() const
        {
            return std::span<const uint8_t> {m_data, m_length};
        }

        bool IsInvalid() const
        {
            return m_length == 0;
        }

        uint32_t Id() const
        {
            return m_id;
        }


        // For the driver
        void Set(uint32_t id, uint8_t length)
        {
            m_id = id;
            m_length = length;
        }

        void Invalidate()
        {
            m_length = 0;
        }

        uint8_t* RawData()
        {
            return m_data;
        }

    private:
        uint32_t m_id;
        uint8_t m_length;
        uint8_t m_data[kMaxDataLength];
    };

    virtual ~ICan() = default;

    [[nodiscard]] virtual std::unique_ptr<ListenerCookie> Start(os::binary_semaphore& sem) = 0;
    virtual void Stop() = 0;

    virtual bool SendFrame(uint32_t id, std::span<const uint8_t> data) = 0;

    virtual std::optional<Frame> ReceiveFrame() = 0;
};

} // namespace hal
