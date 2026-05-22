#pragma once

#include "thread_parameters.hh"
#include "time.hh"

#include <memory>
#include <trompeloeil/mock.hpp>

namespace os
{

class MockThread
{
public:
    MAKE_MOCK0(Awake, void());
    MAKE_MOCK0(Suspend, void());
};

using ThreadHandle = MockThread*;

class MockKernel
{
public:
    MAKE_MOCK1(OnThreadStart, void(ThreadHandle));
};

namespace detail
{
template <typename T>
using mem_unique_ptr = std::unique_ptr<T, void (*)(T*)>;

ThreadHandle GetCurrentThread();

void AwakeThread(ThreadHandle thread);

void SuspendThread(ThreadHandle thread);

ThreadHandle StartThread(const char* name,
                         ThreadCore core,
                         ThreadPriority priority,
                         uint32_t stack_size,
                         const std::function<void()>& thread_loop);

void WaitThreadExit(ThreadHandle thread);


// Unit test only
void SetCurrentThread(ThreadHandle thread);
std::shared_ptr<MockKernel> GetKernelMock();

template <typename T>
inline mem_unique_ptr<T>
AllocFastMem(unsigned alignment [[maybe_unused]])
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
        return mem_unique_ptr<T>(nullptr, deleter);
    }

    auto* object = new (raw_ptr) T();
    return mem_unique_ptr<T>(object, deleter);
}

template <typename T>
inline mem_unique_ptr<T>
AllocSlowMem(unsigned alignment)
{
    // Same on Qt
    return detail::AllocFastMem<T>(alignment);
}


} // namespace detail

} // namespace os
