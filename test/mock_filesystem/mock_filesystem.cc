#include "mock_filesystem.hh"

#include <algorithm>
#include <map>
#include <string>

namespace
{

// Only files are stored, so directories exist implicitly while they contain files
std::map<std::string, std::vector<std::byte>, std::less<>> g_files;

// The prefix of everything in a directory, where "" is the root
std::string
DirectoryPrefix(std::string_view path)
{
    return path.empty() ? std::string() : std::string(path) + "/";
}

bool
IsDirectory(std::string_view path)
{
    auto prefix = DirectoryPrefix(path);
    auto it = g_files.lower_bound(prefix);

    return it != g_files.end() && it->first.starts_with(prefix);
}

bool
Exists(std::string_view path)
{
    return g_files.contains(path) || IsDirectory(path);
}

} // namespace

// Looks up the file on every access, so it's not left dangling if the file is moved or the files
// are cleared. Writing then recreates it
class File::Impl
{
public:
    Impl(std::string_view path, bool writable)
        : m_path(path)
        , m_writable(writable)
    {
    }

    size_t Read(std::span<std::byte> out)
    {
        auto it = g_files.find(m_path);
        if (m_writable || it == g_files.end() || m_position >= it->second.size())
        {
            return 0;
        }

        const auto& data = it->second;
        auto n = std::min(out.size(), data.size() - m_position);
        std::copy_n(data.begin() + m_position, n, out.begin());
        m_position += n;

        return n;
    }

    bool Write(std::span<const std::byte> data)
    {
        if (!m_writable)
        {
            return false;
        }

        auto& file = g_files[m_path];
        file.insert(file.end(), data.begin(), data.end());

        return true;
    }

    std::optional<size_t> Size() const
    {
        if (auto it = g_files.find(m_path); it != g_files.end())
        {
            return it->second.size();
        }

        return std::nullopt;
    }

private:
    const std::string m_path;
    const bool m_writable;
    size_t m_position {0};
};

File::File(std::unique_ptr<Impl> impl)
    : m_impl(std::move(impl))
{
}

File::~File() = default;
File::File(File&& other) noexcept = default;
File& File::operator=(File&& other) noexcept = default;

size_t
File::Read(std::span<std::byte> out)
{
    return m_impl->Read(out);
}

bool
File::Write(std::span<const std::byte> data)
{
    return m_impl->Write(data);
}

bool
File::Sync()
{
    // Always stored
    return true;
}

std::optional<size_t>
File::Size()
{
    return m_impl->Size();
}

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

std::optional<File>
Filesystem::OpenReadOnly(std::string_view path) const
{
    if (!g_files.contains(path))
    {
        return std::nullopt;
    }

    return File(std::make_unique<File::Impl>(path, false));
}

std::optional<File>
Filesystem::OpenWritable(std::string_view path, WriteMode mode) const
{
    // Like a real filesystem: the path can't be a directory, or have a file as a parent
    if (IsDirectory(path))
    {
        return std::nullopt;
    }
    for (auto pos = path.find('/'); pos != std::string_view::npos; pos = path.find('/', pos + 1))
    {
        if (g_files.contains(path.substr(0, pos)))
        {
            return std::nullopt;
        }
    }

    auto& file = g_files[std::string(path)];
    if (mode == WriteMode::kTruncate)
    {
        file.clear();
    }

    return File(std::make_unique<File::Impl>(path, true));
}

bool
Filesystem::FileExists(std::string_view path) const
{
    return Exists(path);
}

std::vector<Filesystem::DirectoryEntry>
Filesystem::ListDirectory(std::string_view path) const
{
    auto prefix = DirectoryPrefix(path);

    std::vector<DirectoryEntry> out;
    for (auto it = g_files.lower_bound(prefix); it != g_files.end() && it->first.starts_with(prefix);
         ++it)
    {
        auto name = std::string_view(it->first).substr(prefix.size());

        if (auto slash = name.find('/'); slash != std::string_view::npos)
        {
            // In a subdirectory, which is listed once. Its contents are adjacent in the map
            auto directory = DirectoryEntry {std::string(name.substr(0, slash)), true};
            if (out.empty() || out.back() != directory)
            {
                out.push_back(std::move(directory));
            }
        }
        else
        {
            out.push_back({std::string(name), false});
        }
    }

    // The map is sorted by full path, so e.g., "a.txt" comes before "a/b.txt"
    std::ranges::sort(out, {}, &DirectoryEntry::name);

    return out;
}

bool
Filesystem::Remove(std::string_view path) const
{
    auto it = g_files.find(path);
    if (it == g_files.end())
    {
        return false;
    }

    g_files.erase(it);

    return true;
}

bool
Filesystem::Move(std::string_view from, std::string_view to) const
{
    if (Exists(to))
    {
        return false;
    }

    if (auto it = g_files.find(from); it != g_files.end())
    {
        auto data = std::move(it->second);
        g_files.erase(it);
        g_files[std::string(to)] = std::move(data);

        return true;
    }

    // No directories, so move everything below from/ instead
    auto from_dir = std::string(from) + "/";
    auto to_dir = std::string(to) + "/";

    auto first = g_files.lower_bound(from_dir);
    auto last = first;
    while (last != g_files.end() && last->first.starts_with(from_dir))
    {
        ++last;
    }

    if (first == last || std::ranges::any_of(g_files, [&to_dir](const auto& entry) {
            return entry.first.starts_with(to_dir);
        }))
    {
        return false;
    }

    std::map<std::string, std::vector<std::byte>, std::less<>> moved;
    for (auto it = first; it != last; ++it)
    {
        moved[to_dir + it->first.substr(from_dir.size())] = std::move(it->second);
    }
    g_files.erase(first, last);
    g_files.merge(moved);

    return true;
}
