// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa
// See LICENSE file for details.

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include "sd/i_file.h"
#include "sd/i_filesystem.h"

// ---------------------------------------------------------------------------
// Mock implementations for host-side testing
// ---------------------------------------------------------------------------

namespace {

    // In-memory file that captures writes and supports read-back with a cursor.
    class MockFile : public ::ungula::sd::IFile {
        public:
            std::string data;
            size_t read_pos = 0;
            int flush_count = 0;
            bool closed = false;

            size_t write(const void* buf, size_t len) override {
                if (closed) {
                    return 0;
                }
                data.append(static_cast<const char*>(buf), len);
                return len;
            }

            size_t read(void* buf, size_t len) override {
                if (closed || read_pos >= data.size()) {
                    return 0;
                }
                size_t avail = data.size() - read_pos;
                size_t count = (len < avail) ? len : avail;
                std::memcpy(buf, data.data() + read_pos, count);
                read_pos += count;
                return count;
            }

            bool flush() override {
                if (closed) {
                    return false;
                }
                ++flush_count;
                return true;
            }

            void close() override {
                closed = true;
            }
    };

    // In-memory filesystem that tracks mount state and returns MockFile
    // instances from open().
    class MockFileSystem : public ::ungula::sd::IFileSystem {
        public:
            bool mounted = false;
            bool mount_should_fail = false;
            std::vector<MockFile*> opened_files;

            bool mount() override {
                if (mount_should_fail) {
                    return false;
                }
                mounted = true;
                return true;
            }

            void unmount() override {
                mounted = false;
            }

            bool is_mounted() const override {
                return mounted;
            }

            ::ungula::sd::IFile* open(const char* /*path*/,
                                      ::ungula::sd::OpenMode /*mode*/) override {
                if (!mounted) {
                    return nullptr;
                }
                auto* file = new MockFile();
                opened_files.push_back(file);
                return file;
            }

            bool free_space(::ungula::sd::SpaceInfo& out) const {
                return true;
            }

            // Delete a file by path. Returns true on success, false if the file
            // does not exist or cannot be removed.
            bool remove(const char* path) {
                return true;
            }

            // List files in a directory. Calls cb once per regular file entry.
            // Returns the number of entries visited (0 if dir is empty or missing).
            int list_dir(const char* dir_path, ::ungula::sd::DirEntryCallback cb, void* ctx) {
                return 0;
            }
    };

}  // namespace

// ---------------------------------------------------------------------------
// IFileSystem contract
// ---------------------------------------------------------------------------

TEST(FileSystem, MountAndUnmount) {
    MockFileSystem fs;
    EXPECT_FALSE(fs.is_mounted());

    EXPECT_TRUE(fs.mount());
    EXPECT_TRUE(fs.is_mounted());

    // Second mount is a no-op success
    EXPECT_TRUE(fs.mount());

    fs.unmount();
    EXPECT_FALSE(fs.is_mounted());
}

TEST(FileSystem, MountFailure) {
    MockFileSystem fs;
    fs.mount_should_fail = true;
    EXPECT_FALSE(fs.mount());
    EXPECT_FALSE(fs.is_mounted());
}

TEST(FileSystem, OpenFailsWhenNotMounted) {
    MockFileSystem fs;
    auto* file = fs.open("/test.log", ::ungula::sd::OpenMode::AppendBinary);
    EXPECT_EQ(file, nullptr);
}

TEST(FileSystem, OpenSucceedsWhenMounted) {
    MockFileSystem fs;
    fs.mount();
    auto* file = fs.open("/test.log", ::ungula::sd::OpenMode::AppendBinary);
    ASSERT_NE(file, nullptr);
    delete file;
}

// ---------------------------------------------------------------------------
// Low-level byte I/O
// ---------------------------------------------------------------------------

TEST(File, WriteAndFlush) {
    MockFile file;

    const char* msg = "hello audit";
    size_t written = file.write(msg, std::strlen(msg));
    EXPECT_EQ(written, std::strlen(msg));
    EXPECT_EQ(file.data, "hello audit");

    EXPECT_TRUE(file.flush());
    EXPECT_EQ(file.flush_count, 1);
}

TEST(File, ReadBack) {
    MockFile file;
    file.write("abcdef", 6);

    char buf[4] = {};
    EXPECT_EQ(file.read(buf, 3), 3u);
    EXPECT_EQ(std::string(buf, 3), "abc");

    EXPECT_EQ(file.read(buf, 3), 3u);
    EXPECT_EQ(std::string(buf, 3), "def");

    // EOF
    EXPECT_EQ(file.read(buf, 3), 0u);
}

TEST(File, WriteAfterCloseReturnsZero) {
    MockFile file;
    file.write("before", 6);
    file.close();

    size_t written = file.write("after", 5);
    EXPECT_EQ(written, 0u);
    EXPECT_FALSE(file.flush());
}

TEST(File, MultipleWritesAccumulate) {
    MockFile file;
    file.write("line1\n", 6);
    file.write("line2\n", 6);
    EXPECT_EQ(file.data, "line1\nline2\n");
}

// ---------------------------------------------------------------------------
// High-level text helpers
// ---------------------------------------------------------------------------

TEST(TextHelpers, WriteStr) {
    MockFile file;
    file.write_str("hello");
    EXPECT_EQ(file.data, "hello");

    file.write_str(" world");
    EXPECT_EQ(file.data, "hello world");
}

TEST(TextHelpers, WriteStrNullIsNoOp) {
    MockFile file;
    EXPECT_EQ(file.write_str(nullptr), 0u);
    EXPECT_TRUE(file.data.empty());
}

TEST(TextHelpers, WriteLineFromCStr) {
    MockFile file;
    file.write_line("first line");
    file.write_line("second line");
    EXPECT_EQ(file.data, "first line\nsecond line\n");
}

TEST(TextHelpers, WriteLineFromPointerAndLength) {
    MockFile file;
    const char* text = "partial match here";
    file.write_line(text, 7);  // "partial"
    EXPECT_EQ(file.data, "partial\n");
}

TEST(TextHelpers, ReadLine) {
    MockFile file;
    file.data = "first\nsecond\nthird";
    // read_pos starts at 0

    char buf[64] = {};
    size_t len = file.read_line(buf, sizeof(buf));
    EXPECT_EQ(len, 5u);
    EXPECT_STREQ(buf, "first");

    len = file.read_line(buf, sizeof(buf));
    EXPECT_EQ(len, 6u);
    EXPECT_STREQ(buf, "second");

    // Last line has no trailing '\n'
    len = file.read_line(buf, sizeof(buf));
    EXPECT_EQ(len, 5u);
    EXPECT_STREQ(buf, "third");

    // EOF
    len = file.read_line(buf, sizeof(buf));
    EXPECT_EQ(len, 0u);
}

TEST(TextHelpers, ReadLineTruncatesWhenBufferTooSmall) {
    MockFile file;
    file.data = "a very long line\n";

    char buf[6] = {};  // can hold 5 chars + null
    size_t len = file.read_line(buf, sizeof(buf));
    EXPECT_EQ(len, 5u);
    EXPECT_STREQ(buf, "a ver");
}

TEST(TextHelpers, ReadLineNullBufferReturnsZero) {
    MockFile file;
    file.data = "something\n";
    EXPECT_EQ(file.read_line(nullptr, 64), 0u);
}

// ---------------------------------------------------------------------------
// Integration: filesystem + file round-trip
// ---------------------------------------------------------------------------

TEST(Integration, OpenWriteFlushClose) {
    MockFileSystem fs;
    fs.mount();

    auto* file = fs.open("/sdcard/audit.log", ::ungula::sd::OpenMode::AppendBinary);
    ASSERT_NE(file, nullptr);

    file->write_line("[INFO] PROGRAM_SELECT idx=3");
    file->flush();
    file->close();

    ASSERT_EQ(fs.opened_files.size(), 1u);
    EXPECT_EQ(fs.opened_files[0]->data, "[INFO] PROGRAM_SELECT idx=3\n");
    EXPECT_EQ(fs.opened_files[0]->flush_count, 1);
    EXPECT_TRUE(fs.opened_files[0]->closed);

    delete file;
}
