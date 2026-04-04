#include "os_implementation.hh"

#include <cassert>

namespace
{

os::ThreadHandle g_current_thread;

} // namespace

os::ThreadHandle
os::detail::GetCurrentThread()
{
    assert(g_current_thread);

    return g_current_thread;
}

void
os::detail::AwakeThread(os::ThreadHandle thread)
{
    assert(thread);

    thread->Awake();
}

void
os::detail::SuspendThread(os::ThreadHandle thread)
{
    assert(thread);

    thread->Suspend();
}

os::ThreadHandle
os::detail::CreateThread(const std::function<void()>& thread_loop)
{
    auto out = new MockThread;

    return out;
}

void
os::detail::StartThread(ThreadHandle thread,
                        const char* name,
                        ThreadCore core,
                        ThreadPriority priority,
                        uint32_t stack_size)
{
    g_current_thread = thread;
}

void os::detail::WaitThreadExit(ThreadHandle thread)
{
    assert(thread);

    delete thread;
}


void
os::detail::SetCurrentThread(os::ThreadHandle thread)
{
    g_current_thread = thread;
}
