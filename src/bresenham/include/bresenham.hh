#pragma once

#include <algorithm>
#include <iterator>

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

        bool operator!=(const Iterator& other) const
        {
            return !(*this == other);
        }

        bool operator==(const Iterator& other) const
        {
            if (m_is_end && other.m_is_end)
            {
                return true;
            }

            return (m_is_end == other.m_is_end) && (m_current == other.m_current);
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
        // and the next increment moves to the past-the-end sentinel.
        Iterator& operator++()
        {
            if (m_is_end)
            {
                return *this;
            }

            if (m_current == m_to)
            {
                m_is_end = true;
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
        Iterator(const PointType& from, const PointType& to, bool is_end = false)
            : m_current(from)
            , m_to(to)
            , m_is_end(is_end)
            , m_dx(std::abs(to.x - from.x))
            , m_dy(-std::abs(to.y - from.y))
            , m_sx(from.x < to.x ? 1 : -1)
            , m_sy(from.y < to.y ? 1 : -1)
            , m_err(m_dx + m_dy)
        {
        }

        PointType m_current;
        PointType m_to;
        bool m_is_end {false};

        int m_dx {0};
        int m_dy {0};
        int m_sx {0};
        int m_sy {0};
        int m_err {0};
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
        return Iterator(m_to, m_to, true);
    }

    uint16_t GetHeading() const
    {
        const int dx = m_to.x - m_from.x;
        const int dy = m_to.y - m_from.y;

        // atan2 returns the angle in radians between the positive x-axis and the point (dx, dy)
        // We convert it to degrees and normalize it to [0, 360)
        double angle_rad = std::atan2(dy, dx);
        double angle_deg = std::fmod((angle_rad * 180.0 / M_PI) + 360.0, 360.0);

        return static_cast<uint16_t>(angle_deg);
    }

private:
    PointType m_from;
    PointType m_to;
};
