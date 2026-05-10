// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#ifdef ESP_PLATFORM

#include "esp_sd_base_filesystem.h"
#include "esp_sd_config.h"

namespace ungula::sd
{

    // ESP32 SPI-mode SD card filesystem. Wraps esp_vfs_fat_sdspi_mount()
    // and exposes file I/O through the IFileSystem / IFile interfaces.
    //
    // Usage:
    //   EspSdSpiConfig cfg{ .pin_miso=37, .pin_mosi=35, .pin_clk=36, .pin_cs=34 };
    //   EspSdFilesystem sd(cfg);
    //   sd.mount();
    //   auto* f = sd.open("/sdcard/audit.log", OpenMode::AppendBinary);
    class EspSdFilesystem : public EspSdBaseFilesystem {
    public:
        explicit EspSdFilesystem(const EspSdSpiConfig &config)
                : EspSdBaseFilesystem(config.mount_point)
                , config_(config)
        {
        }
        ~EspSdFilesystem() override;

        bool mount() override;
        void unmount() override;

    private:
        EspSdSpiConfig config_;
    };

} // namespace ungula::sd
#endif // ESP_PLATFORM
