#include "os_implementation.hh"
#include "time.hh"

#include <QThread>

using namespace os;

namespace os
{
struct ThreadContext
{
    QThread* m_thread;
};
} // namespace os

os::ThreadHandle
os::detail::CreateThread(const std::function<void()> &thread_loop)
{
    auto out = new ThreadContext;
    out->m_thread = QThread::create([thread_loop]() { thread_loop(); });

    return out;
}

void
os::detail::WaitThreadExit(ThreadHandle thread)
{
    thread->m_thread->wait();

    delete thread;
}

void
os::detail::StartThread(ThreadHandle thread, const char* name, ThreadCore, ThreadPriority, uint32_t)
{
    auto qt_thread = thread->m_thread;
    qt_thread->setObjectName(name);
    qt_thread->start();
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
