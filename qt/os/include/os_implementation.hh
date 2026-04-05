#pragma once

#include "thread_parameters.hh"

#include <functional>

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

} // namespace detail

} // namespace os
