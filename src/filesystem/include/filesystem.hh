#pragma once


#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief An open file, closed on destruction
 *
 * Either read-only or write-only, see Filesystem::OpenReadOnly() and
 * Filesystem::OpenWritable(). Keep files short-lived, since the number of open files can be
 * limited (e.g., by the FAT mount on the target).
 */
class File
{
public:
    /// Defined by the filesystem implementation
    class Impl;

    explicit File(std::unique_ptr<Impl> impl);
    ~File();

    File(File&& other) noexcept;
    File& operator=(File&& other) noexcept;

    File(const File&) = delete;
    File& operator=(const File&) = delete;

    /**
     * @brief Read from the current position
     *
     * @param out the buffer to read into
     *
     * @return the number of bytes read, 0 at the end of the file or for a write-only file
     */
    size_t Read(std::span<std::byte> out);

    /**
     * @brief Write at the end of the file
     *
     * @param data the data to write
     *
     * @return true if all of it was written, false on errors or for a read-only file
     */
    bool Write(std::span<const std::byte> data);

    /**
     * @brief Store what's written so far on the medium, e.g., before power is lost
     *
     * Closing the file also does this.
     *
     * @return true on success
     */
    bool Sync();

    /**
     * @brief The size of the file, including what's written but not yet synced
     *
     * @return the size in bytes, or nullopt on errors
     */
    std::optional<size_t> Size();

private:
    std::unique_ptr<Impl> m_impl;
};

class Filesystem
{
public:
    enum class WriteMode
    {
        /// Keep the contents, and write at the end
        kAppend,
        /// Start with an empty file
        kTruncate,
    };

    /**
     * @param root_path the directory which all paths are relative to
     */
    explicit Filesystem(std::string_view root_path);

    /**
     * @brief Open an existing file for reading
     *
     * @return the file, or nullopt if it doesn't exist or is a directory
     */
    std::optional<File> OpenReadOnly(std::string_view path) const;

    /**
     * @brief Open a file for writing, creating it and its parent directories if needed
     *
     * @return the file, or nullopt if it can't be opened, e.g., if it's a directory
     */
    std::optional<File> OpenWritable(std::string_view path,
                                     WriteMode mode = WriteMode::kAppend) const;

    /**
     * @brief Read a whole file
     *
     * @return the contents, or nullopt if it can't be read
     */
    std::optional<std::vector<std::byte>> ReadFile(std::string_view path) const;

    /**
     * @brief Replace the contents of a file, creating it and its parent directories if needed
     *
     * @return true on success
     */
    bool WriteFile(std::string_view path, std::span<const std::byte> data) const;

    /// @brief Check if a file or directory exists
    bool FileExists(std::string_view path) const;

    /**
     * @brief Move a file or a directory, creating the parent directories of to if needed
     *
     * @return true on success, false if from doesn't exist or to exists
     */
    bool Move(std::string_view from, std::string_view to) const;

private:
    std::string m_root_path;
};
