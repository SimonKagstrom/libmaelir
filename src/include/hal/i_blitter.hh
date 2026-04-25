#pragma once

#include "i_display.hh"

#include <cstdint>
#include <span>

namespace hal
{

struct BlitOperation
{
    const uint16_t* src_data;
    uint16_t* dst_data;
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

    virtual void BlitOperations(std::span<const hal::BlitOperation> operations) = 0;
};

} // namespace hal
