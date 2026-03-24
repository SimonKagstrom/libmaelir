#include "os/thread.hh"
#include "semaphore.hh"

#include <vector>

using namespace os;

namespace os
{
struct Impl
{
    Impl(int desired)
        : value(desired)
    {
    }

    int value;
    std::vector<ThreadHandle> waiting_threads;
};

} // namespace os


template <ptrdiff_t least_max_value>
counting_semaphore<least_max_value>::counting_semaphore(ptrdiff_t desired) noexcept
{
    m_impl = std::make_unique<Impl>(desired);
}

template <ptrdiff_t least_max_value>
counting_semaphore<least_max_value>::~counting_semaphore()
{
}

template <ptrdiff_t least_max_value>
void
counting_semaphore<least_max_value>::release(ptrdiff_t update) noexcept
{
    m_impl->value =
        std::clamp(m_impl->value + static_cast<int>(update), 0, static_cast<int>(least_max_value));

    for (auto& thread : m_impl->waiting_threads)
    {
        os::AwakeThread(thread);
    }
    m_impl->waiting_threads.clear();
}

template <ptrdiff_t least_max_value>
bool
counting_semaphore<least_max_value>::release_from_isr(ptrdiff_t update) noexcept
{
    release(update);

    return false;
}

template <ptrdiff_t least_max_value>
void
counting_semaphore<least_max_value>::acquire() noexcept
{
    if (m_impl->value == 0)
    {
        auto thread = os::GetCurrentThread();
        os::SuspendThread(thread);
        m_impl->waiting_threads.push_back(thread);
        return;
    }
    m_impl->value = std::clamp(m_impl->value - 1, 0, static_cast<int>(least_max_value));
}

template <ptrdiff_t least_max_value>
bool
counting_semaphore<least_max_value>::try_acquire() noexcept
{
    if (m_impl->value == 0)
    {
        return false;
    }
    m_impl->value = std::clamp(m_impl->value - 1, 0, static_cast<int>(least_max_value));
    return true;
}

template <ptrdiff_t least_max_value>
bool
counting_semaphore<least_max_value>::try_acquire_for_ms(const milliseconds time)
{
    auto got = try_acquire();
    if (got)
    {
        return true;
    }

    os::SuspendThread(os::GetCurrentThread());

    return false;
}

namespace os
{

template class counting_semaphore<1>;

}
