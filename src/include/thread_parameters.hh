#pragma once

#include <cstdint>

namespace os
{

constexpr auto kDefaultStackSize = 2048;

enum class ThreadPriority : uint8_t
{
    kLow = 1,
    kNormal,
    kHigh,
};

enum class ThreadCore : uint8_t
{
    kCore0 = 0,
    kCore1,
    // Add more if needed
};

} // namespace os