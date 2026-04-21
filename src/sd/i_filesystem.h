// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include "i_file.h"

namespace ungula::sd {

    // File open mode — kept intentionally minimal. Extend when needed.
    enum class OpenMode : uint8_t {
        AppendBinary,  // "ab" — append-only, binary, create if missing
        ReadBinary,    // "rb" — read-only, binary
    };

    // Storage space information returned by free_space().
    struct SpaceInfo {
            uint64_t total_bytes;
            uint64_t free_bytes;
    };

    // Callback invoked once per directory entry by list_dir().
    // path: full path of the entry (e.g. "/sdcard/audit_001.log").
    // ctx:  opaque pointer passed through from list_dir().
    // Return true to continue listing, false to stop early.
    using DirEntryCallback = bool (*)(const char* path, void* ctx);

    // Platform-agnostic filesystem interface. The host project creates a
    // concrete implementation (e.g. EspSdFilesystem for ESP32 SPI-SD) and
    // injects it into consumers that need file I/O.
    class IFileSystem {
        public:
            virtual ~IFileSystem() = default;

            // Mount the underlying storage. Returns true if the medium is ready.
            // Safe to call multiple times — a second call on an already-mounted
            // filesystem returns true immediately.
            virtual bool mount() = 0;

            // Unmount and release the bus. After this call, open() will fail
            // until mount() is called again.
            virtual void unmount() = 0;

            // True when the medium is mounted and ready for I/O.
            virtual bool is_mounted() const = 0;

            // Open a file. Returns a non-null IFile* on success, nullptr on
            // failure (card removed, path invalid, etc.). Ownership of the
            // returned pointer transfers to the caller.
            virtual IFile* open(const char* path, OpenMode mode) = 0;

            // Query total and free space on the mounted volume.
            // Returns false if the filesystem is not mounted or the query fails.
            virtual bool free_space(SpaceInfo& out) const = 0;

            // Delete a file by path. Returns true on success, false if the file
            // does not exist or cannot be removed.
            virtual bool remove(const char* path) = 0;

            // List files in a directory. Calls cb once per regular file entry.
            // Returns the number of entries visited (0 if dir is empty or missing).
            virtual int list_dir(const char* dir_path, DirEntryCallback cb, void* ctx) = 0;
    };

}  // namespace ungula::sd
