#include "debug_assert.hh"

#include <cstdio>
#include <cstdlib>

void
maelir::detail::AssertFailed(const char* expr, const char* file, int line)
{
    fprintf(stderr, "%s:%d: debug_assert(%s) failed\n", file, line, expr);
    abort();
}
