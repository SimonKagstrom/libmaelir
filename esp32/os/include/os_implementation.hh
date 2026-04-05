#pragma once

#include "thread_parameters.hh"

#include <freertos/FreeRTOS.h>
#include <freertos/idf_additions.h>
#include <freertos/task.h>
#include <functional>

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

} // namespace detail

} // namespace os
