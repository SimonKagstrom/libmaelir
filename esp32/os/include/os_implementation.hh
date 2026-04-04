#pragma once

#include "thread_parameters.hh"

#include <freertos/FreeRTOS.h>
#include <freertos/idf_additions.h>
#include <freertos/task.h>


namespace os::detail
{

struct ThreadContext
{
    TaskHandle_t m_task;
    std::function<void()> m_thread_loop;
};

} // namespace os::detail

namespace os
{
using ThreadHandle = detail::ThreadContext*;

namespace detail
{

ThreadHandle
GetCurrentThread()
{
    // Get the current FreeRTOS task handle
    TaskHandle_t handle = xTaskGetCurrentTaskHandle();

    // Calculate the ThreadContext pointer using the offset of m_task
    auto context_ptr =
        reinterpret_cast<char*>(handle) - offsetof(os::detail::ThreadContext, m_task);
    return reinterpret_cast<ThreadHandle>(context_ptr);
}

ThreadHandle CreateThread(const std::function<void()>& thread_loop);

void StartThread(ThreadHandle thread,
                 const char* name,
                 ThreadCore core,
                 ThreadPriority priority,
                 uint32_t stack_size);


void
AwakeThread(ThreadHandle thread)
{
    vTaskResume(thread->m_task);
}

void
SuspendThread(ThreadHandle thread)
{
    vTaskSuspend(thread->m_task);
}

void WaitThreadExit(ThreadHandle thread);

} // namespace detail

} // namespace os
