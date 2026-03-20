#include "os_implementation.hh"

#include <cassert>

namespace os::detail
{

ThreadHandle g_current_thread;

ThreadHandle
GetCurrentThread()
{
    assert(g_current_thread);

    return g_current_thread;
}

uint8_t
GetThreadId(ThreadHandle thread)
{
    assert(thread);

    return thread->GetId();
}

void
AwakeThread(ThreadHandle thread)
{
    assert(thread);

    thread->Awake();
}

void
SuspendThread(ThreadHandle thread)
{
    assert(thread);

    thread->Suspend();
}

} // namespace os::detail

using namespace os::detail;
