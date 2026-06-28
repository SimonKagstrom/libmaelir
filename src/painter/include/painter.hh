#pragma once

#include "bresenham.hh"
#include "hal/i_display.hh"
#include "image.hh"

namespace painter
{

struct Rect
{
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
};

void Blit(uint16_t* frame_buffer,
          const uint16_t* src_buffer,
          uint32_t src_width,
          uint32_t src_height,
          Rect to);

void Blit(uint16_t* frame_buffer, const Image& image, Rect to);

void ZoomedBlit(
    uint16_t* frame_buffer, uint32_t buffer_width, const Image& image, unsigned factor, Rect to);


template <typename PointType>
inline void
DrawClippedLine(uint16_t* frame_buffer,
                PointType from,
                PointType to,
                uint16_t width,
                uint16_t color)
{
    if (from.x - width / 2 < 0)
    {
        from.x = 0;
    }
    if (from.x + width / 2 >= hal::kDisplayWidth)
    {
        from.x = hal::kDisplayWidth - 1;
    }
    if (from.y - width / 2 < 0)
    {
        from.y = 0;
    }
    if (from.y + width / 2 >= hal::kDisplayHeight)
    {
        from.y = hal::kDisplayHeight - 1;
    }


    auto bresenham = Bresenham<PointType>(from, to);
    auto [dx, dy] = bresenham.GetWidthSlope();

    for (auto& point : bresenham)
    {
        for (int i = -width / 2; i < (width + 1) / 2; ++i)
        {
            PointType offset {dx * i, dy * i};
            auto offset_point = PointType {point.x + offset.x, point.y + offset.y};

            frame_buffer[offset_point.y * hal::kDisplayWidth + offset_point.x] = color;
        }
    }
}

} // namespace painter
