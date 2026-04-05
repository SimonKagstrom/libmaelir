#include "os_implementation.hh"

#include <cassert>

namespace
{

os::ThreadHandle g_current_thread;
std::weak_ptr<os::MockKernel> g_kernel_mock;

} // namespace

std::shared_ptr<os::MockKernel> os::detail::GetKernelMock()
{
    if (auto p = g_kernel_mock.lock())
    {
        return p;
    }
    else
    {
        auto new_mock = std::make_shared<MockKernel>();
        g_kernel_mock = new_mock;
        return new_mock;
    }
}


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
os::detail::StartThread(const char* name,
                        ThreadCore core,
                        ThreadPriority priority,
                        uint32_t stack_size,
                        const std::function<void()>& thread_loop)
{
    auto out = new MockThread;
    g_current_thread = out;
    GetKernelMock()->OnThreadStart(out);

    return out;
}

void
os::detail::WaitThreadExit(ThreadHandle thread)
{
    assert(thread);

    delete thread;
}


void
os::detail::SetCurrentThread(os::ThreadHandle thread)
{
    g_current_thread = thread;
}
