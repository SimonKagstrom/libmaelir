#pragma once

#include <array>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>

namespace detail
{

template <typename T, typename Tuple>
struct IndexOfImpl;

template <typename T, typename... Ts>
struct IndexOfImpl<T, std::tuple<Ts...>>
{
    static consteval uint8_t Get()
    {
        static_assert((std::is_same_v<T, Ts> + ... + 0) == 1,
                      "The message must be in AllMessages exactly once");

        constexpr std::array<bool, sizeof...(Ts)> matches {std::is_same_v<T, Ts>...};
        for (std::size_t index = 0; index < matches.size(); ++index)
        {
            if (matches[index])
            {
                return static_cast<uint8_t>(index);
            }
        }

        return 0;
    }
};

} // namespace detail

namespace MSG
{

// Combine message tuples into one, e.g., the common messages and the application ones
template <typename... Tuples>
using ConcatMessages = decltype(std::tuple_cat(std::declval<Tuples>()...));

} // namespace MSG
