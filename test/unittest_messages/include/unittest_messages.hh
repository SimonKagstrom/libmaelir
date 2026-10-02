// The list of all messages for unittests
#pragma once

#include "message_helpers.hh"

#include <cstddef>
#include <string>

namespace MSG
{

struct gregor
{
    std::string caller_number;
    std::string caller_name;
};

struct samsa
{
};

// Als AllMessages Eines Morgens...
using AllMessages = std::tuple<gregor, samsa>;

constexpr auto kMessageCount = std::tuple_size_v<AllMessages>;

template <typename T>
consteval uint8_t
IndexOf()
{
    return detail::IndexOfImpl<T, AllMessages>::Get();
}

} // namespace MSG
