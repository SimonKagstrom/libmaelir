#pragma once

namespace os
{

struct ThreadContext;

using ThreadHandle = ThreadContext*;

namespace detail
{

ThreadHandle GetCurrentThread();

void AwakeThread(ThreadHandle thread);

void SuspendThread(ThreadHandle thread);

} // namespace detail

} // namespace os
