#pragma once

#include <memory>

#include "os_implementation.hh"

namespace os
{

template <typename T>
auto AllocFastMem(unsigned alignment = 4)
{
    return detail::AllocFastMem<T>(alignment);
}

// psram
template <typename T>
auto AllocSlowMem(unsigned alignment = 4)
{
    return detail::AllocSlowMem<T>(alignment);
}

}