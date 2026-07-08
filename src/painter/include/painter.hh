#pragma once

#include "bresenham.hh"
#include "debug_assert.hh"
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
DrawClippedLine(
    uint16_t* frame_buffer, PointType from, PointType to, uint16_t thickness, uint16_t color)
{
    auto bresenham = Bresenham<PointType>(from, to);
    auto [dx, dy] = bresenham.GetWidthSlope();

    for (auto& point : bresenham)
    {
        for (int i = -thickness / 2; i < (thickness + 1) / 2; ++i)
        {
            PointType offset {dx * i, dy * i};
            auto offset_point = PointType {point.x + offset.x, point.y + offset.y};

            if (offset_point.x < 0 || offset_point.x >= hal::kDisplayWidth || offset_point.y < 0 ||
                offset_point.y >= hal::kDisplayHeight)
            {
                continue;
            }
            frame_buffer[offset_point.y * hal::kDisplayWidth + offset_point.x] = color;
        }
    }
}

} // namespace painter
