#pragma once

#include "filesystem.hh"

#include <string_view>

// The Filesystem implementation for unit tests keeps the files in memory, shared by all
// Filesystem instances. Use these to setup files for the code under test.
namespace mock_filesystem
{

void SetFile(std::string_view path, std::string_view contents);

// Remove all files, call between tests
void Clear();

} // namespace mock_filesystem
