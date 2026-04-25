#pragma once

#include <cstdint>

namespace hal
{
enum class Rotation : uint8_t
{
    k0,
    k90,
    k180,
    k270,

    kValueCount,
};
}