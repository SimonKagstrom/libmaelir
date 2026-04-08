#include "os_implementation.hh"
#include "semaphore.hh"
#include "time.hh"

#include <QThread>
#include <QThreadStorage>


namespace
{
QThreadStorage<os::ThreadHandle> g_current_thread;
}


using namespace os;

namespace os
{
struct ThreadContext
{
    QThread* m_thread;
    os::binary_semaphore m_suspend_semaphore {0};
};

} // namespace os


os::ThreadHandle
os::detail::GetCurrentThread()
{
    if (!g_current_thread.hasLocalData())
    {
        return nullptr;
    }

    return g_current_thread.localData();
}

void
os::detail::WaitThreadExit(ThreadHandle thread)
{
    thread->m_thread->wait();

    delete thread;
}

ThreadHandle
os::detail::StartThread(const char* name,
                        ThreadCore,
                        ThreadPriority,
                        uint32_t,
                        const std::function<void()>& thread_loop)
{
    auto out = new ThreadContext;
    out->m_thread = QThread::create([thread_loop, out]() {
        g_current_thread.setLocalData(out);
        thread_loop();
    });

    out->m_thread->setObjectName(name);
    out->m_thread->start();

    return out;
}


void
os::detail::AwakeThread(ThreadHandle thread)
{
    thread->m_suspend_semaphore.release();
}

void
os::detail::SuspendThread(ThreadHandle thread)
{
    auto current_thread = GetCurrentThread();
    assert(current_thread == thread);

    thread->m_suspend_semaphore.acquire();
}


milliseconds
os::GetTimeStamp()
{
    static auto at_start = std::chrono::high_resolution_clock::now();

    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - at_start);
}

uint32_t
os::GetTimeStampRaw()
{
    return GetTimeStamp().count();
}

void
os::Sleep(milliseconds delay)
{
    QThread::msleep(delay.count());
}
