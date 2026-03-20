#pragma once

#include "os_implementation.hh"
#include <cstdint>

namespace os
{

ThreadHandle
GetCurrentThread()
{
    return detail::GetCurrentThread();
}

uint8_t
GetThreadId(ThreadHandle thread)
{
    return detail::GetThreadId(thread);
}

void
AwakeThread(ThreadHandle thread)
{
    detail::AwakeThread(thread);
}

void
SuspendThread(ThreadHandle thread)
{
    detail::SuspendThread(thread);
}

void
TriggerWakeup(milliseconds time_from_now)
{
    detail::TriggerWakeup(time_from_now);
}

} // namespace os
