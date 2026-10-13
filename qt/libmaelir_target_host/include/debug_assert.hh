#pragma once

namespace maelir::detail
{

// Prints the failed expression and aborts
[[noreturn]] void AssertFailed(const char* expr, const char* file, int line);

} // namespace maelir::detail

// Note: The expression must not have side effects, since it's not evaluated on target
#define debug_assert(expr)                                                                         \
    ((expr) ? (void)0 : ::maelir::detail::AssertFailed(#expr, __FILE__, __LINE__))
