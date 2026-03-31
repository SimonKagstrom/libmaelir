#include "os_implementation.hh"
#include "semaphore.hh"

namespace os
{

struct ThreadContext
{
    binary_semaphore suspend_sem {0};
};

} // namespace os

os::ThreadHandle
os::detail::GetCurrentThread()
{
    static thread_local os::ThreadContext context;

    return &context;
}

void
os::detail::AwakeThread(os::ThreadHandle thread)
{
    thread->suspend_sem.release();
}

void
os::detail::SuspendThread(os::ThreadHandle thread)
{
    thread->suspend_sem.acquire();
}
