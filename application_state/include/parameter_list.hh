#pragma once

namespace AS
{

/**
 * @brief A list of parameters, e.g., parameter_list<AS::speed, AS::odometer>
 *
 * Only the types are used, so it doesn't contain any values. The parameters and groups have their
 * list in T::parameters, which is all parameters in a group, or only itself for a parameter.
 */
template <class... T>
struct parameter_list
{
    // Class<T...>, e.g., parameter_list<AS::speed>::apply_to<Writer> is Writer<AS::speed>
    template <template <class...> class Class>
    using apply_to = Class<T...>;
};

// Concatenate two lists
template <class... A, class... B>
consteval parameter_list<A..., B...>
operator+(parameter_list<A...>, parameter_list<B...>)
{
    return {};
}

/**
 * @brief Class<T...>, with the groups in T... replaced by their parameters
 *
 * E.g., with the parameters "url" and "id" in the group AS::server,
 *
 *   expand<Writer, AS::speed, AS::server>
 *
 * is Writer<AS::speed, AS::server::url, AS::server::id>.
 */
template <template <class...> class Class, class... T>
using expand = typename decltype((typename T::parameters {} + ...))::template apply_to<Class>;

} // namespace AS
