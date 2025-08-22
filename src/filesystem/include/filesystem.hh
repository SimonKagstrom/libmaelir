#pragma once


#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class Filesystem
{
public:
    Filesystem(std::string_view root_path);

    std::optional<std::vector<std::byte>> ReadFile(std::string_view path) const;

    bool WriteFile(std::string_view path, std::span<const std::byte> data) const;

    bool FileExists(std::string_view path) const;

private:
    std::string m_root_path;
};
