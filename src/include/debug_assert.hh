#pragma once

#include <cassert>

#if !defined(NDEBUG)
#define debug_assert(expr) assert(expr)
#else
#define debug_assert(expr) ((void)0)
#endif
