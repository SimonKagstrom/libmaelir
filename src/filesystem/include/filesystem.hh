#pragma once


#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class Filesystem
{
public:
    explicit Filesystem(std::string_view root_path);

    std::optional<std::vector<std::byte>> ReadFile(std::string_view path) const;

    bool WriteFile(std::string_view path, std::span<const std::byte> data) const;

    bool FileExists(std::string_view path) const;

    // Move a file or a directory. The parent directory of to is created if needed. Fails if to
    // exists
    bool Move(std::string_view from, std::string_view to) const;

private:
    std::string m_root_path;
};
