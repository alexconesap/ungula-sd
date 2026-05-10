// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#ifdef ESP_PLATFORM

#include "../../i_file.h"

#include <unistd.h> // fsync, fileno
#include <cstdio>

namespace ungula::sd
{

    // IFile implementation backed by a standard FILE* handle. On ESP-IDF the
    // VFS layer routes fopen/fwrite/fread/fflush to the FAT driver transparently
    // once the SD card is mounted.
    class EspSdFile : public IFile {
    public:
        explicit EspSdFile(FILE *fp)
                : fp_(fp)
        {
        }

        ~EspSdFile() override
        {
            close();
        }

        size_t write(const void *data, size_t len) override
        {
            if (fp_ == nullptr) {
                return 0;
            }
            return std::fwrite(data, 1, len, fp_);
        }

        size_t read(void *buf, size_t len) override
        {
            if (fp_ == nullptr) {
                return 0;
            }
            return std::fread(buf, 1, len, fp_);
        }

        bool flush() override
        {
            if (fp_ == nullptr) {
                return false;
            }
            if (std::fflush(fp_) != 0) {
                return false;
            }
            // fflush only pushes the C stdio buffer to the VFS layer.
            // fsync forces the FAT filesystem to write its block cache
            // to the physical SD card — without this, data survives in
            // memory but is lost on power cut or card removal.
            return fsync(fileno(fp_)) == 0;
        }

        void close() override
        {
            if (fp_ != nullptr) {
                std::fclose(fp_);
                fp_ = nullptr;
            }
        }

    private:
        FILE *fp_;
    };

} // namespace ungula::sd
#endif // ESP_PLATFORM
