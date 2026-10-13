#pragma once

#include "debug_assert.hh"

#include <doctest/doctest.h>
#include <doctest/trompeloeil.hpp>

/*
 * Handles failed debug_asserts while alive, instead of failing the test. Use to test that code
 * asserts, e.g.,
 *
 *   MockAssertHandler asserts;
 *   REQUIRE_CALL(asserts, OnAssert(_, _, _));
 *
 * Note that execution continues after a failed debug_assert.
 */
class MockAssertHandler
{
public:
    MockAssertHandler();
    ~MockAssertHandler();

    MockAssertHandler(const MockAssertHandler&) = delete;
    MockAssertHandler& operator=(const MockAssertHandler&) = delete;

    MAKE_MOCK3(OnAssert, void(const char* expr, const char* file, int line));

private:
    MockAssertHandler* m_previous {nullptr};
};
