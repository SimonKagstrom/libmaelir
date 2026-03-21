#pragma once

#include "os_implementation.hh"

#include <cstdint>

namespace os
{

static inline ThreadHandle
GetCurrentThread()
{
    return detail::GetCurrentThread();
}

static inline uint8_t
GetThreadId(ThreadHandle thread)
{
    return detail::GetThreadId(thread);
}

static inline void
AwakeThread(ThreadHandle thread)
{
    detail::AwakeThread(thread);
}

static inline void
SuspendThread(ThreadHandle thread)
{
    detail::SuspendThread(thread);
}

static inline void
TriggerWakeup(milliseconds time_from_now)
{
    detail::TriggerWakeup(time_from_now);
}

} // namespace os
