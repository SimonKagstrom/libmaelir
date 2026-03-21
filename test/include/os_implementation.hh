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

ThreadHandle GetCurrentThread();

uint8_t GetThreadId(ThreadHandle thread);

void AwakeThread(ThreadHandle thread);

void SuspendThread(ThreadHandle thread);

void TriggerWakeup(milliseconds time_from_now);


// Unit test only
void SetCurrentThread(ThreadHandle thread);

} // namespace detail

} // namespace os
