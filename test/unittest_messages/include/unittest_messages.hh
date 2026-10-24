// The messages specific to the unittests
#pragma once

#include <string>
#include <tuple>

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

// Als UnittestMessages Eines Morgens...
using UnittestMessages = std::tuple<gregor, samsa>;

} // namespace MSG
