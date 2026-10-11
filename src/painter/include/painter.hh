#pragma once

#include "bresenham.hh"
#include "debug_assert.hh"
#include "hal/i_display.hh"
#include "image.hh"
namespace painter
{

enum class LineStyle
{
    kSolid,
    kDashed,

    kValueCount,
};

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


// The line itself is assumed to be clipped to the display, but the thickness can extend outside
template <typename PointType, uint16_t thickness>
inline void
DrawClippedLine(uint16_t* frame_buffer, PointType from, PointType to, uint16_t color)
{
    static_assert(thickness > 0, "Thickness must be at least 1");

    // The pixels of a point are offset by kFirst..kLast across the line
    constexpr auto kFirst = -static_cast<int>(thickness) / 2;
    constexpr auto kLast = (static_cast<int>(thickness) + 1) / 2 - 1;

    auto bresenham = Bresenham<PointType>(from, to);
    auto [dx, dy] = bresenham.GetWidthSlope();
    // One pixel across the line in the frame buffer
    const auto step = dy * static_cast<int>(hal::kDisplayWidth) + dx;

    for (auto& point : bresenham)
    {
        // The common case, the whole width is within the display (conservatively in both directions)
        if (point.x + kFirst >= 0 && point.x + kLast < static_cast<int>(hal::kDisplayWidth) &&
            point.y + kFirst >= 0 && point.y + kLast < static_cast<int>(hal::kDisplayHeight))
        {
            auto pixel = &frame_buffer[(point.y + kFirst * dy) * hal::kDisplayWidth + point.x +
                                       kFirst * dx];

            for (auto i = 0; i < thickness; ++i)
            {
                pixel[i * step] = color;
            }
            continue;
        }

        // Near the edges
        for (auto i = kFirst; i <= kLast; ++i)
        {
            auto x = point.x + dx * i;
            auto y = point.y + dy * i;

            if (x < 0 || x >= static_cast<int>(hal::kDisplayWidth) || y < 0 ||
                y >= static_cast<int>(hal::kDisplayHeight))
            {
                continue;
            }
            frame_buffer[y * hal::kDisplayWidth + x] = color;
        }
    }
}

template <typename PointType, uint16_t thickness>
inline void
DrawClippedVerticalLine(uint16_t* frame_buffer,
                        PointType from,
                        decltype(PointType::y) to_y,
                        uint16_t color)
{
    uint16_t line[16];

    if (to_y < from.y)
    {
        std::swap(from.y, to_y);
    }

    to_y = std::clamp(to_y,
                      static_cast<decltype(PointType::y)>(0),
                      static_cast<decltype(PointType::y)>(hal::kDisplayHeight - 1));

    // TODO for the future: Handle lines above 16 in a good way
    static_assert(thickness <= 16, "Thickness must be <= 16 for now");
    if constexpr (thickness > 1)
    {
        // Copy pre-filled lines
        std::fill(line, line + thickness, color);

        for (int y = from.y; y <= to_y; ++y)
        {
            memcpy(
                &frame_buffer[y * hal::kDisplayWidth + from.x], line, thickness * sizeof(uint16_t));
        }
    }
    else
    {
        for (int x = from.x; x < from.x + thickness; ++x)
        {
            for (int y = from.y; y <= to_y; ++y)
            {
                frame_buffer[y * hal::kDisplayWidth + x] = color;
            }
        }
    }
}

template <typename PointType, uint16_t thickness, LineStyle style = LineStyle::kSolid>
inline void
DrawClippedHorizontalLine(uint16_t* frame_buffer,
                          PointType from,
                          decltype(PointType::x) to_x,
                          uint16_t color)
{
    // Assumed to be clipped, panic if not
    debug_assert(from.y >= 0 && from.y < hal::kDisplayHeight);
    debug_assert(to_x >= 0 && to_x < hal::kDisplayWidth);

    for (auto y = from.y; y < from.y + thickness; ++y)
    {
        for (auto x = from.x; x <= to_x; ++x)
        {
            if constexpr (style == LineStyle::kDashed)
            {
                if ((x - from.x) % 8 < 4)
                {
                    continue;
                }
            }

            frame_buffer[y * hal::kDisplayWidth + x] = color;
        }
    }
}

} // namespace painter
