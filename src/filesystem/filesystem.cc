// The parts of Filesystem which are common for all implementations (filesystem_common)
#include "filesystem.hh"

std::optional<std::vector<std::byte>>
Filesystem::ReadFile(std::string_view path) const
{
    auto file = OpenReadOnly(path);
    if (!file)
    {
        return std::nullopt;
    }

    auto size = file->Size();
    if (!size)
    {
        return std::nullopt;
    }

    std::vector<std::byte> out(*size);
    auto total = 0uz;
    while (total < out.size())
    {
        auto n = file->Read(std::span(out).subspan(total));
        if (n == 0)
        {
            // Shrunk since the size was taken
            break;
        }
        total += n;
    }
    out.resize(total);

    return out;
}

bool
Filesystem::WriteFile(std::string_view path, std::span<const std::byte> data) const
{
    auto file = OpenWritable(path, WriteMode::kTruncate);
    if (!file)
    {
        return false;
    }

    return file->Write(data) && file->Sync();
}
