// The in-memory filesystem used by the other unit tests
#include "mock_filesystem.hh"

#include <doctest/doctest.h>

namespace
{

class Fixture
{
public:
    Fixture()
    {
        mock_filesystem::Clear();
    }

    ~Fixture()
    {
        mock_filesystem::Clear();
    }

    Filesystem filesystem {"/sdcard"};
};

} // namespace

TEST_SUITE_BEGIN("filesystem");

#include "filesystem_tests.hh"

TEST_SUITE_END();
