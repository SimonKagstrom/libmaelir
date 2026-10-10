#include "mock_filesystem.hh"

#include <map>
#include <string>

namespace
{

std::map<std::string, std::vector<std::byte>, std::less<>> g_files;

} // namespace

void
mock_filesystem::SetFile(std::string_view path, std::string_view contents)
{
    auto data = std::as_bytes(std::span(contents.data(), contents.size()));

    g_files[std::string(path)] = std::vector<std::byte>(data.begin(), data.end());
}

void
mock_filesystem::Clear()
{
    g_files.clear();
}

Filesystem::Filesystem(std::string_view root_path)
    : m_root_path(root_path)
{
}

std::optional<std::vector<std::byte>>
Filesystem::ReadFile(std::string_view path) const
{
    if (auto it = g_files.find(path); it != g_files.end())
    {
        return it->second;
    }

    return std::nullopt;
}

bool
Filesystem::WriteFile(std::string_view path, std::span<const std::byte> data) const
{
    g_files[std::string(path)] = std::vector<std::byte>(data.begin(), data.end());

    return true;
}

bool
Filesystem::FileExists(std::string_view path) const
{
    return g_files.contains(path);
}

void
Filesystem::Move(std::string_view from, std::string_view to) const
{
    if (auto it = g_files.find(from); it != g_files.end())
    {
        auto data = std::move(it->second);
        g_files.erase(it);
        g_files[std::string(to)] = std::move(data);
    }
}
