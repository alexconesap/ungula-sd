// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#ifdef ESP_PLATFORM

#include "esp_sd_base_filesystem.h"
#include "esp_sdmmc_config.h"

namespace ungula::sd {


// ESP32 native SDMMC filesystem. Wraps esp_vfs_fat_sdmmc_mount() using
// the SDMMC peripheral (not SPI). Faster than SPI mode and uses the
// dedicated SDMMC pins.
//
// Usage:
//   EspSdmmcConfig cfg{ .pin_clk=7, .pin_cmd=6, .pin_d0=15,
//                        .pin_d1=16, .pin_d2=17, .pin_d3=5 };
//   EspSdmmcFilesystem sd(cfg);
//   sd.mount();
//   auto* f = sd.open("/sdcard/audit.log", OpenMode::AppendBinary);
class EspSdmmcFilesystem : public EspSdBaseFilesystem {
    public:
        explicit EspSdmmcFilesystem(const EspSdmmcConfig& config)
            : EspSdBaseFilesystem(config.mount_point), config_(config) {}
        ~EspSdmmcFilesystem() override;

        bool mount() override;
        void unmount() override;

    private:
        EspSdmmcConfig config_;
};

    }  // namespace ungula::sd
#endif  // ESP_PLATFORM
