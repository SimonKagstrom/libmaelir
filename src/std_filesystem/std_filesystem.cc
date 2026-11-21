#include "filesystem.hh"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;

// stdio rather than fstream, since Sync() needs the file descriptor
class File::Impl
{
public:
    Impl(FILE* file, bool writable)
        : m_file(file)
        , m_writable(writable)
    {
    }

    ~Impl()
    {
        fclose(m_file);
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    size_t Read(std::span<std::byte> out)
    {
        if (m_writable)
        {
            return 0;
        }

        return fread(out.data(), 1, out.size(), m_file);
    }

    bool Write(std::span<const std::byte> data)
    {
        if (!m_writable)
        {
            return false;
        }

        return fwrite(data.data(), 1, data.size(), m_file) == data.size();
    }

    bool Sync()
    {
        if (!m_writable)
        {
            // Nothing to store
            return true;
        }

        return fflush(m_file) == 0 && fsync(fileno(m_file)) == 0;
    }

    std::optional<size_t> Size()
    {
        // Include what's buffered by stdio
        if (m_writable && fflush(m_file) != 0)
        {
            return std::nullopt;
        }

        struct stat st;
        if (fstat(fileno(m_file), &st) != 0)
        {
            return std::nullopt;
        }

        return static_cast<size_t>(st.st_size);
    }

private:
    FILE* m_file;
    const bool m_writable;
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
    return m_impl->Sync();
}

std::optional<size_t>
File::Size()
{
    return m_impl->Size();
}


Filesystem::Filesystem(std::string_view root_path)
    : m_root_path(root_path)
{
}

std::optional<File>
Filesystem::OpenReadOnly(std::string_view path) const
{
    fs::path full_path = fs::path(m_root_path) / fs::path(path);

    std::error_code ec;
    if (!fs::is_regular_file(full_path, ec))
    {
        return std::nullopt;
    }

    auto file = fopen(full_path.c_str(), "rb");
    if (!file)
    {
        return std::nullopt;
    }

    return File(std::make_unique<File::Impl>(file, false));
}

std::optional<File>
Filesystem::OpenWritable(std::string_view path, WriteMode mode) const
{
    fs::path full_path = fs::path(m_root_path) / fs::path(path);

    if (full_path.has_parent_path())
    {
        std::error_code ec;
        fs::create_directories(full_path.parent_path(), ec);
        if (ec)
        {
            return std::nullopt;
        }
    }

    auto file = fopen(full_path.c_str(), mode == WriteMode::kAppend ? "ab" : "wb");
    if (!file)
    {
        return std::nullopt;
    }

    return File(std::make_unique<File::Impl>(file, true));
}

bool
Filesystem::FileExists(std::string_view path) const
{
    fs::path full_path = fs::path(m_root_path) / fs::path(path);

    // Check for non-directory, since FAT might mess things up with is_regular_file()
    return fs::exists(full_path);// && !fs::is_directory(full_path);
}

std::vector<Filesystem::DirectoryEntry>
Filesystem::ListDirectory(std::string_view path) const
{
    fs::path full_path = fs::path(m_root_path) / fs::path(path);

    std::vector<DirectoryEntry> out;
    std::error_code ec;
    for (auto it = fs::directory_iterator(full_path, ec); !ec && it != fs::directory_iterator();
         it.increment(ec))
    {
        out.push_back({it->path().filename().string(), it->is_directory(ec)});
    }

    // The iteration order is unspecified
    std::ranges::sort(out, {}, &DirectoryEntry::name);

    return out;
}

bool
Filesystem::Remove(std::string_view path) const
{
    fs::path full_path = fs::path(m_root_path) / fs::path(path);

    std::error_code ec;
    if (!fs::is_regular_file(full_path, ec))
    {
        return false;
    }

    return fs::remove(full_path, ec);
}

bool
Filesystem::Move(std::string_view from, std::string_view to) const
{
    fs::path full_from = fs::path(m_root_path) / fs::path(from);
    fs::path full_to = fs::path(m_root_path) / fs::path(to);

    std::error_code ec;
    if (!fs::exists(full_from, ec) || fs::exists(full_to, ec))
    {
        return false;
    }

    if (full_to.has_parent_path())
    {
        fs::create_directories(full_to.parent_path(), ec);
        if (ec)
        {
            return false;
        }
    }

    fs::rename(full_from, full_to, ec);

    return !ec;
}
