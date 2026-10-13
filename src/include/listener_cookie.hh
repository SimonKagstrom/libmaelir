#pragma once

#include <functional>
#include <utility>

/*
 * A handle to an attached listener, which calls the detach function when destroyed. Move-only,
 * and empty when default-constructed, moved-from or reset.
 */
class ListenerCookie
{
public:
    ListenerCookie() = default;

    explicit ListenerCookie(std::function<void()> on_destruction)
        : m_on_destruction(std::move(on_destruction))
    {
    }

    ListenerCookie(const ListenerCookie&) = delete;
    ListenerCookie& operator=(const ListenerCookie&) = delete;

    ListenerCookie(ListenerCookie&& other) noexcept
        // A moved-from std::function is in an unspecified state, so explicitly clear it
        : m_on_destruction(std::exchange(other.m_on_destruction, nullptr))
    {
    }

    ListenerCookie& operator=(ListenerCookie&& other) noexcept
    {
        if (this != &other)
        {
            Reset();
            m_on_destruction = std::exchange(other.m_on_destruction, nullptr);
        }

        return *this;
    }

    ~ListenerCookie()
    {
        Reset();
    }

    // Detach the listener now
    void Reset()
    {
        if (auto on_destruction = std::exchange(m_on_destruction, nullptr); on_destruction)
        {
            on_destruction();
        }
    }

    // True if a listener is attached
    explicit operator bool() const
    {
        return static_cast<bool>(m_on_destruction);
    }

private:
    std::function<void()> m_on_destruction;
};
