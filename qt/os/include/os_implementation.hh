#pragma once

#include "thread_parameters.hh"

#include <cstdlib>
#include <functional>
#include <memory>
#include <new>

namespace os
{

struct ThreadContext;

using ThreadHandle = ThreadContext*;

namespace detail
{

ThreadHandle GetCurrentThread();

ThreadHandle StartThread(const char* name,
                         ThreadCore core,
                         ThreadPriority priority,
                         uint32_t stack_size,
                         const std::function<void()>& thread_loop);


void AwakeThread(ThreadHandle thread);

void SuspendThread(ThreadHandle thread);

void WaitThreadExit(ThreadHandle thread);

template <typename T>
using OsMemPtr = std::unique_ptr<T, void (*)(T*)>;

template <typename T>
inline OsMemPtr<T> AllocFastMem(unsigned alignment [[maybe_unused]])
{
    const size_t alloc_alignment = alignment > alignof(T) ? alignment : alignof(T);
    auto deleter = +[](T* ptr) {
        if (ptr != nullptr)
        {
            ptr->~T();
            std::free(ptr);
        }
    };

    void* raw_ptr = nullptr;
    if (posix_memalign(&raw_ptr, alloc_alignment, sizeof(T)) != 0)
    {
        return OsMemPtr<T>(nullptr, deleter);
    }

    auto* object = new (raw_ptr) T();
    return OsMemPtr<T>(object, deleter);
}

template <typename T>
inline OsMemPtr<T> AllocSlowMem(unsigned alignment)
{
    // Same on Qt
    return detail::AllocFastMem<T>(alignment);
}


} // namespace detail

} // namespace os
