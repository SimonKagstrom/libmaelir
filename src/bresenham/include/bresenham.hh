#pragma once

#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <numbers>

template <typename PointType>
class Bresenham
{
public:
    class Iterator
    {
    public:
        friend class Bresenham;

        using iterator_category = std::forward_iterator_tag;
        using value_type = PointType;
        using difference_type = std::ptrdiff_t;
        using pointer = const PointType*;
        using reference = const PointType&;

        bool operator==(const Iterator& other) const
        {
            return m_done == other.m_done && (m_done || m_current == other.m_current);
        }

        bool operator==(std::default_sentinel_t) const
        {
            return m_done;
        }

        reference operator*() const
        {
            return m_current;
        }

        pointer operator->() const
        {
            return &m_current;
        }

        // Advance by one Bresenham step. The point at m_to is yielded once,
        // and the next increment moves past the end.
        Iterator& operator++()
        {
            if (m_current == m_to)
            {
                m_done = true;
                return *this;
            }

            const int twice_error = m_err * 2;
            if (twice_error >= m_dy)
            {
                m_err += m_dy;
                m_current.x += m_sx;
            }
            if (twice_error <= m_dx)
            {
                m_err += m_dx;
                m_current.y += m_sy;
            }

            return *this;
        }

    private:
        Iterator(const PointType& from, const PointType& to)
            : m_current(from)
            , m_to(to)
            , m_dx(std::abs(static_cast<int>(to.x) - static_cast<int>(from.x)))
            , m_dy(-std::abs(static_cast<int>(to.y) - static_cast<int>(from.y)))
            , m_sx(from.x < to.x ? 1 : -1)
            , m_sy(from.y < to.y ? 1 : -1)
            , m_err(m_dx + m_dy)
        {
        }

        PointType m_current;
        PointType m_to;

        int m_dx;
        int m_dy;
        int m_sx;
        int m_sy;
        int m_err;
        bool m_done {false};
    };

    Bresenham(const PointType& from, const PointType& to)
        : m_from(from)
        , m_to(to)
    {
    }
    Bresenham(const Bresenham&) = default;
    Bresenham() = delete;

    auto begin() const
    {
        return Iterator(m_from, m_to);
    }

    auto end() const
    {
        return std::default_sentinel;
    }

    bool IsMostlyVerticalSlope() const
    {
        return std::abs(m_to.y - m_from.y) > std::abs(m_to.x - m_from.x);
    }

    auto GetWidthSlope() const
    {
        if (IsMostlyVerticalSlope())
        {
            return std::make_pair(1, 0);
        }
        else
        {
            return std::make_pair(0, 1);
        }
    }

    uint16_t GetHeading() const
    {
        const int dx = m_to.x - m_from.x;
        const int dy = m_to.y - m_from.y;

        // atan2 returns the angle in radians between the positive x-axis and the point (dx, dy)
        // We convert it to degrees and normalize it to [0, 360)
        float angle_rad = atan2f(dy, dx);
        float angle_deg = fmod((angle_rad * 180.0f / std::numbers::pi_v<float>)+360.0f, 360.0f);

        return static_cast<uint16_t>(angle_deg);
    }

private:
    PointType m_from;
    PointType m_to;
};
