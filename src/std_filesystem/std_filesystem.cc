#include "filesystem.hh"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

Filesystem::Filesystem(std::string_view root_path)
    : m_root_path(root_path)
{
}

std::optional<std::vector<std::byte>>
Filesystem::ReadFile(std::string_view path) const
{
    fs::path full_path = fs::path(m_root_path) / fs::path(path);

    if (!fs::exists(full_path) || !fs::is_regular_file(full_path))
    {
        return std::nullopt;
    }

    std::ifstream file(full_path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        return std::nullopt;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<std::byte> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
    {
        return std::nullopt;
    }

    return buffer;
}

bool
Filesystem::WriteFile(std::string_view path, std::span<const std::byte> data) const
{
    namespace fs = std::filesystem;
    fs::path full_path = fs::path(m_root_path) / fs::path(path);

    // Ensure parent directory exists
    if (full_path.has_parent_path())
    {
        std::error_code ec;
        fs::create_directories(full_path.parent_path(), ec);
        if (ec)
        {
            return false;
        }
    }

    std::ofstream file(full_path, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        return false;
    }

    file.write(reinterpret_cast<const char*>(data.data()), data.size());

    return file.good();
}

bool
Filesystem::FileExists(std::string_view path) const
{
    fs::path full_path = fs::path(m_root_path) / fs::path(path);
    return fs::exists(full_path) && fs::is_regular_file(full_path);
}
