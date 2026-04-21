// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#ifdef ESP_PLATFORM

#include "../../i_filesystem.h"

namespace ungula {
    namespace sd {

        // Shared base for ESP32 SD filesystem implementations. The VFS file
        // operations (open, free_space, remove, list_dir) are identical for both
        // SPI and SDMMC — only mount/unmount differ because they configure
        // different host peripherals. This base avoids duplicating that code.
        class EspSdBaseFilesystem : public IFileSystem {
            public:
                bool is_mounted() const override;
                IFile* open(const char* path, OpenMode mode) override;
                bool free_space(SpaceInfo& out) const override;
                bool remove(const char* path) override;
                int list_dir(const char* dir_path, DirEntryCallback cb, void* ctx) override;

                /// Last ESP-IDF error code from mount(). 0 (ESP_OK) on success.
                int last_error() const {
                    return last_error_;
                }

            protected:
                explicit EspSdBaseFilesystem(const char* mount_point) : mount_point_(mount_point) {}

                const char* mount_point_;
                void* card_ = nullptr;  // sdmmc_card_t* — opaque to avoid ESP-IDF header leak
                bool mounted_ = false;
                int last_error_ = 0;
        };

    }  // namespace sd
}  // namespace ungula

#endif  // ESP_PLATFORM
