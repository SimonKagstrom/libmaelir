#pragma once

// No asserts on target. Note: The expression must not have side effects, since it's evaluated
// on the host and in unit tests
#define debug_assert(expr) ((void)0)
