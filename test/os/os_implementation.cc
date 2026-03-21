#include "os_implementation.hh"

#include <cassert>

namespace
{

os::ThreadHandle g_current_thread;
std::weak_ptr<os::MockKernel> g_kernel;

} // namespace

std::shared_ptr<os::MockKernel>
os::MockKernel::Create()
{
    auto out = std::make_shared<os::MockKernel>();

    g_kernel = out;

    return out;
}


os::ThreadHandle
os::detail::GetCurrentThread()
{
    assert(g_current_thread);

    return g_current_thread;
}

uint8_t
os::detail::GetThreadId(os::ThreadHandle thread)
{
    assert(thread);

    return thread->GetId();
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

void
os::detail::TriggerWakeup(milliseconds time_from_now)
{
    auto kernel = g_kernel.lock();
    assert(kernel);

    kernel->TriggerWakeup(time_from_now);
}


void
os::detail::SetCurrentThread(os::ThreadHandle thread)
{
    g_current_thread = thread;
}