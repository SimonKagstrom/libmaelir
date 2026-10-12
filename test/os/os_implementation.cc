#include "os_implementation.hh"

#include <cassert>

namespace
{

os::ThreadHandle g_current_thread;
bool g_in_interrupt_context {false};
unsigned g_non_isr_calls_in_interrupt_context {0};
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

void
os::detail::SetInterruptContext(bool in_interrupt)
{
    g_in_interrupt_context = in_interrupt;
}

bool
os::detail::InInterruptContext()
{
    return g_in_interrupt_context;
}

unsigned&
os::detail::NonIsrCallsInInterruptContext()
{
    return g_non_isr_calls_in_interrupt_context;
}
