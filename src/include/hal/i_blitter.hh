#pragma once

#include <cstdint>
#include <span>

namespace hal
{

enum Rotation
{
    k0,
    k90,
    k180,
    k270,

    kValueCount,
};

struct BlitOperation
{
    const uint16_t* src_data;
    int16_t src_width;
    int16_t src_height;
    int16_t src_offset_x;
    int16_t src_offset_y;
    int16_t dst_offset_x;
    int16_t dst_offset_y;
    int16_t width;
    int16_t height;
    Rotation rotation;
};

class IBlitter
{
public:
    virtual ~IBlitter() = default;

    virtual void BlitOperations(uint16_t* frame_buffer, std::span<const hal::BlitOperation> operations) = 0;
};

} // namespace hal
