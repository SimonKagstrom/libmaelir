#pragma once

#include "thread_parameters.hh"

#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/idf_additions.h>
#include <freertos/task.h>
#include <functional>
#include <memory>
#include <new>

namespace os::detail
{

struct ThreadContext
{
    struct Data
    {
        std::function<void()> m_thread_loop;
    };

    TaskHandle_t m_task;

    Data* m_private_data;
};

} // namespace os::detail

namespace os
{
using ThreadHandle = detail::ThreadContext*;

namespace detail
{

inline ThreadHandle
GetCurrentThread()
{
    // Get the current FreeRTOS task handle
    TaskHandle_t handle = xTaskGetCurrentTaskHandle();

    // Calculate the ThreadContext pointer using the offset of m_task
    auto context_ptr =
        reinterpret_cast<char*>(handle) - offsetof(os::detail::ThreadContext, m_task);
    return reinterpret_cast<ThreadHandle>(context_ptr);
}

ThreadHandle StartThread(const char* name,
                         ThreadCore core,
                         ThreadPriority priority,
                         uint32_t stack_size,
                         const std::function<void()>& thread_loop);


inline void
AwakeThread(ThreadHandle thread)
{
    vTaskResume(thread->m_task);
}

inline void
SuspendThread(ThreadHandle thread)
{
    vTaskSuspend(thread->m_task);
}

void WaitThreadExit(ThreadHandle thread);


template <typename T>
using OsMemPtr = std::unique_ptr<T, void (*)(T*)>;

template <typename T>
inline OsMemPtr<T>
AllocEsp32Mem(auto alignment, auto flags)
{
    const size_t alloc_alignment = alignment > alignof(T) ? alignment : alignof(T);
    void* raw_ptr = heap_caps_aligned_calloc(alloc_alignment, 1, sizeof(T), flags);
    auto deleter = +[](T* ptr) {
        if (ptr != nullptr)
        {
            ptr->~T();
            heap_caps_free(ptr);
        }
    };

    if (raw_ptr == nullptr)
    {
        return OsMemPtr<T>(nullptr, deleter);
    }

    auto* object = new (raw_ptr) T();
    return OsMemPtr<T>(object, deleter);
}


template <typename T>
inline OsMemPtr<T>
AllocFastMem(unsigned alignment)
{
    return AllocEsp32Mem<T>(alignment, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

template <typename T>
inline OsMemPtr<T>
AllocSlowMem(unsigned alignment)
{
    return AllocEsp32Mem<T>(alignment, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

} // namespace detail

} // namespace os
