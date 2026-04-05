#pragma once

#include "thread_parameters.hh"
#include "time.hh"

#include <memory>
#include <trompeloeil/mock.hpp>

namespace os
{

class MockThread
{
public:
    MAKE_MOCK0(Awake, void());
    MAKE_MOCK0(Suspend, void());
};

using ThreadHandle = MockThread*;

class MockKernel
{
public:
    MAKE_MOCK1(OnThreadStart, void(ThreadHandle));
};

namespace detail
{

ThreadHandle GetCurrentThread();

void AwakeThread(ThreadHandle thread);

void SuspendThread(ThreadHandle thread);

ThreadHandle StartThread(const char* name,
                         ThreadCore core,
                         ThreadPriority priority,
                         uint32_t stack_size,
                         const std::function<void()>& thread_loop);

void WaitThreadExit(ThreadHandle thread);


// Unit test only
void SetCurrentThread(ThreadHandle thread);
std::shared_ptr<MockKernel> GetKernelMock();

} // namespace detail

} // namespace os
