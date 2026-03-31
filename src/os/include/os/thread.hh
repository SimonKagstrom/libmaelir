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

} // namespace os
