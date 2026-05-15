// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ungula::sd
{

// Handle to an open file. Callers obtain one from IFileSystem::open()
// and must call close() when done (or let the destructor handle it).
class IFile {
    public:
        virtual ~IFile() = default;

        // -- Low-level byte I/O ------------------------------------------------

        // Write len bytes from data. Returns the number of bytes actually
        // written; anything less than len indicates an error or disk-full.
        virtual size_t write(const void *data, size_t len) = 0;

        // Read up to len bytes into buf. Returns the number of bytes read,
        // 0 on EOF or error.
        virtual size_t read(void *buf, size_t len) = 0;

        // Force buffered data to the physical medium. Returns true on success.
        virtual bool flush() = 0;

        // Close the file and release the underlying handle. After this call
        // the object is unusable — the caller should null or delete the pointer.
        virtual void close() = 0;

        // -- High-level text helpers -------------------------------------------
        // Non-virtual convenience methods built on top of the primitives above.
        // Implementations only need to override write/read/flush/close.

        // Write a null-terminated C string (without the null terminator).
        size_t write_str(const char *str)
        {
                if (str == nullptr) {
                        return 0;
                }
                return write(str, std::strlen(str));
        }

        // Write a text line: the string followed by '\n'.
        // Returns total bytes written (string + newline).
        size_t write_line(const char *str)
        {
                size_t n = write_str(str);
                const char nl = '\n';
                n += write(&nl, 1);
                return n;
        }

        // Write a text line from a pointer + length, followed by '\n'.
        size_t write_line(const char *data, size_t len)
        {
                size_t n = write(data, len);
                const char nl = '\n';
                n += write(&nl, 1);
                return n;
        }

        // Read one line of text (up to max_len - 1 characters). Stops at '\n',
        // EOF, or when the buffer is full. The trailing '\n' is NOT included
        // in the output. The buffer is always null-terminated.
        // Returns the number of characters placed in buf (excluding the null).
        size_t read_line(char *buf, size_t max_len)
        {
                if (buf == nullptr || max_len == 0) {
                        return 0;
                }
                size_t pos = 0;
                while (pos < max_len - 1) {
                        char ch = 0;
                        size_t got = read(&ch, 1);
                        if (got == 0) {
                                break; // EOF
                        }
                        if (ch == '\n') {
                                break; // end of line (not stored)
                        }
                        buf[pos++] = ch;
                }
                buf[pos] = '\0';
                return pos;
        }
};

} // namespace ungula::sd
