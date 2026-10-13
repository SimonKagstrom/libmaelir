#include "mock_debug_assert.hh"

#include <format>

namespace
{

MockAssertHandler* g_handler {nullptr};

} // namespace

MockAssertHandler::MockAssertHandler()
    : m_previous(g_handler)
{
    g_handler = this;
}

MockAssertHandler::~MockAssertHandler()
{
    g_handler = m_previous;
}

void
maelir::detail::AssertFailed(const char* expr, const char* file, int line)
{
    if (g_handler)
    {
        g_handler->OnAssert(expr, file, line);
        return;
    }

    // Not throwing, since debug_asserts are also used in destructors and noexcept functions.
    // Reported at the location of the assert
    ADD_FAIL_CHECK_AT(file, line, std::format("debug_assert({}) failed", expr));
}
