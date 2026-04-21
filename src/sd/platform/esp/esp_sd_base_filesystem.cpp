// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa
// See LICENSE file for details.

#ifdef ESP_PLATFORM

#include "esp_sd_base_filesystem.h"
#include "esp_sd_file.h"

#include <dirent.h>
#include <cstdio>

#include "esp_vfs_fat.h"

namespace ungula {
    namespace sd {

        bool EspSdBaseFilesystem::is_mounted() const {
            return mounted_;
        }

        IFile* EspSdBaseFilesystem::open(const char* path, OpenMode mode) {
            if (!mounted_ || path == nullptr) {
                return nullptr;
            }

            const char* mode_str = nullptr;
            switch (mode) {
                case OpenMode::AppendBinary:
                    mode_str = "ab";
                    break;
                case OpenMode::ReadBinary:
                    mode_str = "rb";
                    break;
            }

            FILE* fp = std::fopen(path, mode_str);
            if (fp == nullptr) {
                return nullptr;
            }

            return new EspSdFile(fp);
        }

        bool EspSdBaseFilesystem::free_space(SpaceInfo& out) const {
            if (!mounted_) {
                return false;
            }
            uint64_t total = 0, free_bytes = 0;
            if (esp_vfs_fat_info(mount_point_, &total, &free_bytes) != ESP_OK) {
                return false;
            }
            out.total_bytes = total;
            out.free_bytes = free_bytes;
            return true;
        }

        bool EspSdBaseFilesystem::remove(const char* path) {
            if (!mounted_ || path == nullptr) {
                return false;
            }
            return ::remove(path) == 0;
        }

        int EspSdBaseFilesystem::list_dir(const char* dir_path, DirEntryCallback cb, void* ctx) {
            if (!mounted_ || dir_path == nullptr || cb == nullptr) {
                return 0;
            }
            DIR* dir = opendir(dir_path);
            if (dir == nullptr) {
                return 0;
            }
            int count = 0;
            char full_path[272];  // mount(8) + '/' + d_name(255) + NUL
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                // Skip directories
                if (entry->d_type == DT_DIR) {
                    continue;
                }

                // ESP-IDF's FAT VFS reports DT_UNKNOWN for all entries, so filtering on DT_REG
                // alone would miss every file. We simply avoid filtering by type and accept all
                // what the VFS gives us.
                snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
                ++count;
                if (!cb(full_path, ctx)) {
                    break;
                }
            }
            closedir(dir);
            return count;
        }

    }  // namespace sd
}  // namespace ungula

#endif  // ESP_PLATFORM
