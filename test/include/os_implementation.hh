#pragma once

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

class MockKernel
{
public:
    std::shared_ptr<MockKernel> Create();

    MAKE_MOCK1(TriggerWakeup, void(milliseconds));
};

using ThreadHandle = MockThread*;

namespace detail
{

ThreadHandle GetCurrentThread();

void AwakeThread(ThreadHandle thread);

void SuspendThread(ThreadHandle thread);

// Unit test only
void SetCurrentThread(ThreadHandle thread);

} // namespace detail

} // namespace os
