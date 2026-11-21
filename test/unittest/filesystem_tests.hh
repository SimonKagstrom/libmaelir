// The Filesystem test cases, included by test_filesystem.cc (the in-memory implementation) and
// test_std_filesystem.cc (the real one), so that both behave the same. The including file
// defines a Fixture with a Filesystem member named filesystem, and the test suite
#pragma once

#include "filesystem.hh"

#include <array>
#include <doctest/doctest.h>
#include <numeric>
#include <string>

namespace
{

std::span<const std::byte>
Bytes(std::string_view str)
{
    return std::as_bytes(std::span(str.data(), str.size()));
}

std::string
String(std::span<const std::byte> data)
{
    return std::string(reinterpret_cast<const char*>(data.data()), data.size());
}

using Entries = std::vector<Filesystem::DirectoryEntry>;

// The whole file as a string, or "(missing)"
std::string
Contents(const Filesystem& filesystem, std::string_view path)
{
    auto data = filesystem.ReadFile(path);

    return data ? String(*data) : "(missing)";
}

} // namespace

TEST_CASE_FIXTURE(Fixture, "a missing file can't be read")
{
    CHECK_FALSE(filesystem.FileExists("missing.txt"));
    CHECK_FALSE(filesystem.ReadFile("missing.txt"));
    CHECK_FALSE(filesystem.OpenReadOnly("missing.txt"));
}

TEST_CASE_FIXTURE(Fixture, "a written file can be read back")
{
    REQUIRE(filesystem.WriteFile("file.txt", Bytes("hello world")));

    THEN("it exists, with the same contents")
    {
        CHECK(filesystem.FileExists("file.txt"));
        CHECK(Contents(filesystem, "file.txt") == "hello world");
    }

    AND_WHEN("it's written again with shorter contents")
    {
        REQUIRE(filesystem.WriteFile("file.txt", Bytes("bye")));

        THEN("the contents are replaced")
        {
            CHECK(Contents(filesystem, "file.txt") == "bye");
        }
    }

    AND_WHEN("it's written again with nothing")
    {
        REQUIRE(filesystem.WriteFile("file.txt", {}));

        THEN("it's empty, but still exists")
        {
            CHECK(filesystem.FileExists("file.txt"));
            CHECK(Contents(filesystem, "file.txt") == "");
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "files can be written in subdirectories")
{
    REQUIRE(filesystem.WriteFile("a/b/file.txt", Bytes("nested")));

    CHECK(Contents(filesystem, "a/b/file.txt") == "nested");

    // A directory isn't a file
    CHECK_FALSE(filesystem.OpenReadOnly("a/b"));
    CHECK_FALSE(filesystem.ReadFile("a"));
}

TEST_CASE_FIXTURE(Fixture, "files and directories can't take each other's places")
{
    REQUIRE(filesystem.WriteFile("dir/file.txt", Bytes("in a directory")));

    SUBCASE("a directory can't be written as a file")
    {
        CHECK_FALSE(filesystem.OpenWritable("dir"));
        CHECK_FALSE(filesystem.WriteFile("dir", Bytes("nope")));
    }

    SUBCASE("a file can't be used as a directory")
    {
        CHECK_FALSE(filesystem.OpenWritable("dir/file.txt/below.txt"));
        CHECK_FALSE(filesystem.WriteFile("dir/file.txt/deeper/below.txt", Bytes("nope")));
    }

    SUBCASE("a name which only starts like the directory is fine")
    {
        CHECK(filesystem.WriteFile("di", Bytes("short")));
        CHECK(filesystem.WriteFile("dir.txt", Bytes("longer")));
        CHECK(Contents(filesystem, "di") == "short");
        CHECK(Contents(filesystem, "dir.txt") == "longer");
    }

    CHECK(Contents(filesystem, "dir/file.txt") == "in a directory");
}

TEST_CASE_FIXTURE(Fixture, "binary and large files are kept intact")
{
    SUBCASE("all byte values")
    {
        std::vector<std::byte> data(256);
        std::iota(reinterpret_cast<uint8_t*>(data.data()),
                  reinterpret_cast<uint8_t*>(data.data()) + data.size(),
                  0);

        REQUIRE(filesystem.WriteFile("binary.bin", data));
        CHECK(filesystem.ReadFile("binary.bin") == data);
    }

    SUBCASE("larger than the read buffer")
    {
        std::vector<std::byte> data(10000);
        for (auto i = 0u; i < data.size(); i++)
        {
            data[i] = static_cast<std::byte>(i * 7);
        }

        REQUIRE(filesystem.WriteFile("large.bin", data));
        CHECK(filesystem.ReadFile("large.bin") == data);
    }
}

TEST_CASE_FIXTURE(Fixture, "a file is read in pieces")
{
    REQUIRE(filesystem.WriteFile("file.txt", Bytes("hello world")));

    auto file = filesystem.OpenReadOnly("file.txt");
    REQUIRE(file);

    std::array<std::byte, 4> buffer;
    auto Read = [&]() { return String(std::span(buffer).first(file->Read(buffer))); };

    CHECK(Read() == "hell");
    CHECK(Read() == "o wo");
    CHECK(Read() == "rld");

    // And then the end of the file
    CHECK(Read() == "");
    CHECK(Read() == "");
}

TEST_CASE_FIXTURE(Fixture, "a writable file is appended to")
{
    {
        auto file = filesystem.OpenWritable("log.txt");
        REQUIRE(file);
        CHECK(file->Write(Bytes("first,")));
        CHECK(file->Write(Bytes("second,")));
    }

    THEN("a new file is created")
    {
        CHECK(Contents(filesystem, "log.txt") == "first,second,");
    }

    AND_WHEN("it's opened again")
    {
        {
            auto file = filesystem.OpenWritable("log.txt");
            REQUIRE(file);
            CHECK(file->Write(Bytes("third")));
        }

        THEN("the old contents are kept")
        {
            CHECK(Contents(filesystem, "log.txt") == "first,second,third");
        }
    }

    AND_WHEN("it's opened again for truncation")
    {
        {
            auto file = filesystem.OpenWritable("log.txt", Filesystem::WriteMode::kTruncate);
            REQUIRE(file);

            // Emptied directly
            CHECK(Contents(filesystem, "log.txt") == "");

            CHECK(file->Write(Bytes("new")));
        }

        THEN("only the new contents are kept")
        {
            CHECK(Contents(filesystem, "log.txt") == "new");
        }
    }

    AND_WHEN("it's opened in a new subdirectory")
    {
        {
            auto file = filesystem.OpenWritable("dir/log.txt");
            REQUIRE(file);
            CHECK(file->Write(Bytes("nested")));
        }

        THEN("the directory is created")
        {
            CHECK(Contents(filesystem, "dir/log.txt") == "nested");
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "the size of a file is known")
{
    REQUIRE(filesystem.WriteFile("file.txt", Bytes("contents")));

    SUBCASE("for a read-only file, also after reading")
    {
        auto file = filesystem.OpenReadOnly("file.txt");
        REQUIRE(file);
        CHECK(file->Size() == 8);

        std::array<std::byte, 4> buffer;
        REQUIRE(file->Read(buffer) == 4);
        CHECK(file->Size() == 8);
    }

    SUBCASE("for a writable file, including what's not synced")
    {
        auto file = filesystem.OpenWritable("file.txt");
        REQUIRE(file);
        CHECK(file->Size() == 8);

        REQUIRE(file->Write(Bytes(" and more")));
        CHECK(file->Size() == 17);
    }

    SUBCASE("for a truncated file")
    {
        auto file = filesystem.OpenWritable("file.txt", Filesystem::WriteMode::kTruncate);
        REQUIRE(file);
        CHECK(file->Size() == 0);
    }
}

TEST_CASE_FIXTURE(Fixture, "synced data can be read while the file is still open")
{
    auto file = filesystem.OpenWritable("log.txt");
    REQUIRE(file);

    REQUIRE(file->Write(Bytes("line 1\n")));
    REQUIRE(file->Sync());
    CHECK(Contents(filesystem, "log.txt") == "line 1\n");

    REQUIRE(file->Write(Bytes("line 2\n")));
    REQUIRE(file->Sync());
    CHECK(Contents(filesystem, "log.txt") == "line 1\nline 2\n");
}

TEST_CASE_FIXTURE(Fixture, "files are either read-only or write-only")
{
    REQUIRE(filesystem.WriteFile("file.txt", Bytes("contents")));

    SUBCASE("a read-only file can't be written")
    {
        auto file = filesystem.OpenReadOnly("file.txt");
        REQUIRE(file);

        CHECK_FALSE(file->Write(Bytes("more")));
        CHECK(file->Sync());
    }

    SUBCASE("a writable file can't be read")
    {
        auto file = filesystem.OpenWritable("file.txt");
        REQUIRE(file);

        std::array<std::byte, 4> buffer;
        CHECK(file->Read(buffer) == 0);
    }

    CHECK(Contents(filesystem, "file.txt") == "contents");
}

TEST_CASE_FIXTURE(Fixture, "an open file can be moved")
{
    auto first = filesystem.OpenWritable("first.txt");
    auto second = filesystem.OpenWritable("second.txt");
    REQUIRE(first);
    REQUIRE(second);

    {
        // Move construction
        File file = std::move(*first);
        CHECK(file.Write(Bytes("moved")));

        // Move assignment, which closes the second file
        *second = std::move(file);
        CHECK(second->Write(Bytes(" twice")));

        // Destroying the moved-from files is fine
        first.reset();
    }
    second.reset();

    CHECK(Contents(filesystem, "first.txt") == "moved twice");
    CHECK(Contents(filesystem, "second.txt") == "");
}

TEST_CASE_FIXTURE(Fixture, "files and directories exist")
{
    REQUIRE(filesystem.WriteFile("dir/file.txt", Bytes("contents")));

    CHECK(filesystem.FileExists("dir/file.txt"));
    CHECK(filesystem.FileExists("dir"));

    CHECK_FALSE(filesystem.FileExists("di"));
    CHECK_FALSE(filesystem.FileExists("dir/file"));
    CHECK_FALSE(filesystem.FileExists("dir/other.txt"));
}

TEST_CASE_FIXTURE(Fixture, "directories can be listed")
{
    REQUIRE(filesystem.WriteFile("dir/b.txt", Bytes("b")));
    REQUIRE(filesystem.WriteFile("dir/a.txt", Bytes("a")));
    REQUIRE(filesystem.WriteFile("dir/sub/deeper.txt", Bytes("deeper")));
    REQUIRE(filesystem.WriteFile("dir/sub/deepest/file.txt", Bytes("deepest")));
    // Sorts before and after "sub/" by full path
    REQUIRE(filesystem.WriteFile("dir/sub.txt", Bytes("sub")));
    REQUIRE(filesystem.WriteFile("dir/sub0", Bytes("sub0")));
    REQUIRE(filesystem.WriteFile("top.txt", Bytes("top")));

    THEN("the entries are listed by name, without the contents of subdirectories")
    {
        CHECK(filesystem.ListDirectory("dir") == Entries {
                                                     {"a.txt", false},
                                                     {"b.txt", false},
                                                     {"sub", true},
                                                     {"sub.txt", false},
                                                     {"sub0", false},
                                                 });
        CHECK(filesystem.ListDirectory("dir/sub") == Entries {
                                                         {"deeper.txt", false},
                                                         {"deepest", true},
                                                     });
    }

    THEN("the root can be listed")
    {
        CHECK(filesystem.ListDirectory("") == Entries {
                                                  {"dir", true},
                                                  {"top.txt", false},
                                              });
    }

    THEN("a missing directory, or a file, gives nothing")
    {
        CHECK(filesystem.ListDirectory("missing").empty());
        CHECK(filesystem.ListDirectory("di").empty());
        CHECK(filesystem.ListDirectory("top.txt").empty());
    }
}

TEST_CASE_FIXTURE(Fixture, "files can be removed")
{
    REQUIRE(filesystem.WriteFile("dir/a.txt", Bytes("a")));
    REQUIRE(filesystem.WriteFile("dir/b.txt", Bytes("b")));

    WHEN("a file is removed")
    {
        REQUIRE(filesystem.Remove("dir/a.txt"));

        THEN("it's gone, but the others are kept")
        {
            CHECK_FALSE(filesystem.FileExists("dir/a.txt"));
            CHECK_FALSE(filesystem.ReadFile("dir/a.txt"));
            CHECK(filesystem.ListDirectory("dir") == Entries {{"b.txt", false}});
        }

        AND_WHEN("it's removed again")
        {
            THEN("it fails")
            {
                CHECK_FALSE(filesystem.Remove("dir/a.txt"));
            }
        }
    }

    WHEN("a directory is removed")
    {
        THEN("it fails, and the files are kept")
        {
            CHECK_FALSE(filesystem.Remove("dir"));
            CHECK(Contents(filesystem, "dir/a.txt") == "a");
            CHECK(Contents(filesystem, "dir/b.txt") == "b");
        }
    }

    WHEN("a missing file is removed")
    {
        THEN("it fails")
        {
            CHECK_FALSE(filesystem.Remove("dir/missing.txt"));
            CHECK_FALSE(filesystem.Remove("missing/a.txt"));
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "files and directories can be moved")
{
    REQUIRE(filesystem.WriteFile("from/a.txt", Bytes("a")));
    REQUIRE(filesystem.WriteFile("from/sub/b.txt", Bytes("b")));
    REQUIRE(filesystem.WriteFile("other/c.txt", Bytes("c")));

    WHEN("a file is moved to a new directory")
    {
        REQUIRE(filesystem.Move("from/a.txt", "to/renamed.txt"));

        THEN("it's only in the new place")
        {
            CHECK_FALSE(filesystem.FileExists("from/a.txt"));
            CHECK(Contents(filesystem, "to/renamed.txt") == "a");
        }
    }

    WHEN("a directory is moved")
    {
        REQUIRE(filesystem.Move("from", "to/moved"));

        THEN("its contents are moved with it")
        {
            CHECK_FALSE(filesystem.FileExists("from/a.txt"));
            CHECK(Contents(filesystem, "to/moved/a.txt") == "a");
            CHECK(Contents(filesystem, "to/moved/sub/b.txt") == "b");
        }
    }

    WHEN("the destination exists")
    {
        THEN("it fails, and nothing is changed")
        {
            CHECK_FALSE(filesystem.Move("from/a.txt", "other/c.txt"));
            CHECK_FALSE(filesystem.Move("from/a.txt", "other"));
            CHECK_FALSE(filesystem.Move("from", "other"));

            CHECK(Contents(filesystem, "from/a.txt") == "a");
            CHECK(Contents(filesystem, "from/sub/b.txt") == "b");
            CHECK(Contents(filesystem, "other/c.txt") == "c");
        }
    }

    WHEN("the source is missing")
    {
        THEN("it fails")
        {
            CHECK_FALSE(filesystem.Move("from/missing.txt", "to/missing.txt"));
            CHECK_FALSE(filesystem.Move("missing", "to"));
            CHECK_FALSE(filesystem.FileExists("to"));
        }
    }
}
