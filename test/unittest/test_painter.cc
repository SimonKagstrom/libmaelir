#include "painter.hh"
#include "test.hh"

#include <random>
#include <vector>

namespace
{

constexpr auto kWidth = static_cast<int>(hal::kDisplayWidth);
constexpr auto kHeight = static_cast<int>(hal::kDisplayHeight);
// To catch writes outside the frame buffer
constexpr auto kGuardSize = 4 * kWidth;
constexpr uint16_t kGuardValue = 0xdead;

struct Point
{
    int32_t x;
    int32_t y;

    bool operator==(const Point& other) const = default;
};

// The straightforward version, with bounds checks for every pixel
void
ReferenceLine(uint16_t* frame_buffer, Point from, Point to, int thickness, uint16_t color)
{
    auto bresenham = Bresenham<Point>(from, to);
    auto [dx, dy] = bresenham.GetWidthSlope();

    for (auto& point : bresenham)
    {
        for (int i = -thickness / 2; i < (thickness + 1) / 2; ++i)
        {
            auto x = point.x + dx * i;
            auto y = point.y + dy * i;

            if (x < 0 || x >= kWidth || y < 0 || y >= kHeight)
            {
                continue;
            }
            frame_buffer[y * kWidth + x] = color;
        }
    }
}

class FrameBuffer
{
public:
    FrameBuffer()
        : m_data(kWidth * kHeight + 2 * kGuardSize, kGuardValue)
    {
        std::fill(Pixels(), Pixels() + kWidth * kHeight, 0);
    }

    uint16_t* Pixels()
    {
        return m_data.data() + kGuardSize;
    }

    bool GuardsIntact() const
    {
        return std::all_of(m_data.begin(),
                           m_data.begin() + kGuardSize,
                           [](auto v) { return v == kGuardValue; }) &&
               std::all_of(m_data.end() - kGuardSize, m_data.end(), [](auto v) {
                   return v == kGuardValue;
               });
    }

    bool operator==(const FrameBuffer& other) const = default;

private:
    std::vector<uint16_t> m_data;
};

std::vector<std::pair<Point, Point>>
TestLines()
{
    constexpr auto kRight = kWidth - 1;
    constexpr auto kBottom = kHeight - 1;

    std::vector<std::pair<Point, Point>> out {
        // Along the edges
        {{0, 0}, {kRight, 0}},
        {{0, kBottom}, {kRight, kBottom}},
        {{0, 0}, {0, kBottom}},
        {{kRight, 0}, {kRight, kBottom}},
        // Diagonals, corner to corner
        {{0, 0}, {kRight, kBottom}},
        {{kRight, 0}, {0, kBottom}},
        // Single points, at the corners and in the middle
        {{0, 0}, {0, 0}},
        {{kRight, kBottom}, {kRight, kBottom}},
        {{kWidth / 2, kHeight / 2}, {kWidth / 2, kHeight / 2}},
        // Just inside the edges
        {{1, 1}, {1, 50}},
        {{2, 10}, {80, 12}},
        {{kRight - 1, 10}, {kRight - 2, 300}},
        {{10, kBottom - 2}, {400, kBottom - 1}},
        // Short lines in the middle
        {{100, 100}, {104, 100}},
        {{100, 100}, {100, 97}},
        {{100, 100}, {97, 103}},
    };

    // Random lines within the display, as from the clipping in the callers
    std::mt19937 rng(4711);
    std::uniform_int_distribution<int> x_dist(0, kRight);
    std::uniform_int_distribution<int> y_dist(0, kBottom);
    for (auto i = 0; i < 500; i++)
    {
        out.push_back({{x_dist(rng), y_dist(rng)}, {x_dist(rng), y_dist(rng)}});
    }

    return out;
}

template <uint16_t thickness>
void
RequireSameAsReference()
{
    FrameBuffer reference;
    FrameBuffer optimized;

    auto color = uint16_t {1};
    for (const auto& [from, to] : TestLines())
    {
        ReferenceLine(reference.Pixels(), from, to, thickness, color);
        painter::DrawClippedLine<Point, thickness>(optimized.Pixels(), from, to, color);

        color++;
    }

    REQUIRE(optimized.GuardsIntact());
    REQUIRE(reference.GuardsIntact());
    REQUIRE(optimized == reference);
}

// Per-pixel distance check, in doubled coordinates to handle even diameters
void
ReferenceCircle(uint16_t* frame_buffer, Point center, int diameter, uint16_t color)
{
    const auto first = -diameter / 2;
    const auto last = (diameter + 1) / 2 - 1;

    for (auto dy = first; dy <= last; ++dy)
    {
        for (auto dx = first; dx <= last; ++dx)
        {
            auto x = center.x + dx;
            auto y = center.y + dy;
            auto ddx = 2 * dx - (first + last);
            auto ddy = 2 * dy - (first + last);

            if (ddx * ddx + ddy * ddy > diameter * diameter || x < 0 || x >= kWidth || y < 0 ||
                y >= kHeight)
            {
                continue;
            }
            frame_buffer[y * kWidth + x] = color;
        }
    }
}

template <uint16_t diameter>
void
RequireCircleSameAsReference()
{
    FrameBuffer reference;
    FrameBuffer optimized;

    std::vector<Point> centers {
        {0, 0},
        {kWidth - 1, kHeight - 1},
        {kWidth / 2, kHeight / 2},
        // Partly or fully outside the display
        {-2, 10},
        {kWidth + 1, 10},
        {10, -3},
        {10, kHeight + 2},
        {-100, -100},
    };
    std::mt19937 rng(4711);
    std::uniform_int_distribution<int> x_dist(-20, kWidth + 20);
    std::uniform_int_distribution<int> y_dist(-20, kHeight + 20);
    for (auto i = 0; i < 500; i++)
    {
        centers.push_back({x_dist(rng), y_dist(rng)});
    }

    auto color = uint16_t {1};
    for (const auto& center : centers)
    {
        ReferenceCircle(reference.Pixels(), center, diameter, color);
        painter::DrawFilledCircle<Point, diameter>(optimized.Pixels(), center, color);

        color++;
    }

    REQUIRE(optimized.GuardsIntact());
    REQUIRE(reference.GuardsIntact());
    REQUIRE(optimized == reference);
}

} // namespace


TEST_SUITE_BEGIN("painter");

TEST_CASE("clipped lines are drawn like with per-pixel bounds checks")
{
    SUBCASE("thickness 1")
    {
        RequireSameAsReference<1>();
    }

    SUBCASE("even thickness")
    {
        RequireSameAsReference<4>();
    }

    SUBCASE("odd thickness")
    {
        RequireSameAsReference<5>();
    }

    SUBCASE("thick lines")
    {
        RequireSameAsReference<16>();
    }
}

TEST_CASE("a thick line at the edge is clipped")
{
    FrameBuffer fb;

    // Horizontal, so the width is vertical: 2 pixels above are outside the display
    painter::DrawClippedLine<Point, 5>(fb.Pixels(), {10, 0}, {20, 0}, 0xffff);

    REQUIRE(fb.GuardsIntact());
    REQUIRE(fb.Pixels()[0 * kWidth + 15] == 0xffff);
    REQUIRE(fb.Pixels()[2 * kWidth + 15] == 0xffff);
    REQUIRE(fb.Pixels()[3 * kWidth + 15] == 0);
}

TEST_CASE("filled circles are drawn like with per-pixel bounds checks")
{
    SUBCASE("diameter 1")
    {
        RequireCircleSameAsReference<1>();
    }

    SUBCASE("even diameter")
    {
        RequireCircleSameAsReference<8>();
    }

    SUBCASE("odd diameter")
    {
        RequireCircleSameAsReference<5>();
    }

    SUBCASE("large diameter")
    {
        RequireCircleSameAsReference<16>();
    }
}

TEST_CASE("a filled circle covers the same pixels across as a line of the same thickness")
{
    FrameBuffer line;
    FrameBuffer circle;

    // Horizontal line, so the width is vertical. Compare the column at the end point
    painter::DrawClippedLine<Point, 8>(line.Pixels(), {100, 100}, {150, 100}, 1);
    painter::DrawFilledCircle<Point, 8>(circle.Pixels(), Point {150, 100}, 1);

    for (auto y = 90; y < 110; ++y)
    {
        REQUIRE(circle.Pixels()[y * kWidth + 150] == line.Pixels()[y * kWidth + 150]);
    }
    // ... and the same horizontally through the center
    REQUIRE(circle.Pixels()[100 * kWidth + 145] == 0);
    REQUIRE(circle.Pixels()[100 * kWidth + 146] == 1);
    REQUIRE(circle.Pixels()[100 * kWidth + 153] == 1);
    REQUIRE(circle.Pixels()[100 * kWidth + 154] == 0);
}

TEST_SUITE_END();
