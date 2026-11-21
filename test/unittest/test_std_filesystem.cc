// The real filesystem, in a temporary directory
#include "filesystem.hh"

#include <doctest/doctest.h>
#include <filesystem>
#include <string>
#include <unistd.h>

namespace
{

class Fixture
{
public:
    Fixture()
    {
        std::filesystem::remove_all(m_root);
        std::filesystem::create_directories(m_root);
    }

    ~Fixture()
    {
        std::error_code ec;
        std::filesystem::remove_all(m_root, ec);
    }

private:
    static inline unsigned m_count {0};

    // Unique per test, and per process if several run in parallel
    const std::filesystem::path m_root {
        std::filesystem::temp_directory_path() /
        ("test_std_filesystem_" + std::to_string(getpid()) + "_" + std::to_string(m_count++))};

public:
    Filesystem filesystem {m_root.string()};
};

} // namespace

TEST_SUITE_BEGIN("std_filesystem");

#include "filesystem_tests.hh"

TEST_SUITE_END();
