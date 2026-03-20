#pragma once

#include "time.hh"

#include <memory>
#include <trompeloeil/mock.hpp>

namespace os
{

class MockThread
{
public:
    MAKE_MOCK0(GetId, uint8_t());
    MAKE_MOCK0(Awake, void());
    MAKE_MOCK0(Suspend, void());
};

using ThreadHandle = MockThread*;

namespace detail
{

extern ThreadHandle g_current_thread;

ThreadHandle GetCurrentThread();

uint8_t GetThreadId(ThreadHandle thread);

void AwakeThread(ThreadHandle thread);

void SuspendThread(ThreadHandle thread);

void TriggerWakeup(milliseconds time_from_now);

} // namespace detail

} // namespace os
