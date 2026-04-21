// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa
// See LICENSE file for details.

#ifdef ESP_PLATFORM

#include "esp_sdmmc_filesystem.h"

#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

namespace ungula {
    namespace sd {

        EspSdmmcFilesystem::~EspSdmmcFilesystem() {
            unmount();
        }

        bool EspSdmmcFilesystem::mount() {
            if (mounted_) {
                return true;
            }

            if (config_.pin_clk < 0 || config_.pin_cmd < 0 || config_.pin_d0 < 0) {
                last_error_ = -1;  // invalid pin configuration
                return false;
            }

            sdmmc_host_t host = SDMMC_HOST_DEFAULT();

            sdmmc_slot_config_t slot_cfg = SDMMC_SLOT_CONFIG_DEFAULT();
            slot_cfg.clk = static_cast<gpio_num_t>(config_.pin_clk);
            slot_cfg.cmd = static_cast<gpio_num_t>(config_.pin_cmd);
            slot_cfg.d0 = static_cast<gpio_num_t>(config_.pin_d0);

            if (config_.bus_width_4 && config_.pin_d1 >= 0 && config_.pin_d2 >= 0 &&
                config_.pin_d3 >= 0) {
                slot_cfg.d1 = static_cast<gpio_num_t>(config_.pin_d1);
                slot_cfg.d2 = static_cast<gpio_num_t>(config_.pin_d2);
                slot_cfg.d3 = static_cast<gpio_num_t>(config_.pin_d3);
                slot_cfg.width = 4;
            } else {
                slot_cfg.width = 1;
            }

            esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {};
            mount_cfg.format_if_mount_failed = config_.format_if_mount_failed;
            mount_cfg.max_files = config_.max_files;
            mount_cfg.allocation_unit_size = 16 * 1024;

            sdmmc_card_t* card = nullptr;
            last_error_ = esp_vfs_fat_sdmmc_mount(config_.mount_point, &host, &slot_cfg, &mount_cfg,
                                                  &card);
            if (last_error_ != ESP_OK) {
                return false;
            }
            card_ = card;

            mounted_ = true;
            return true;
        }

        void EspSdmmcFilesystem::unmount() {
            if (!mounted_ || card_ == nullptr) {
                return;
            }
            esp_vfs_fat_sdcard_unmount(config_.mount_point, static_cast<sdmmc_card_t*>(card_));
            card_ = nullptr;
            mounted_ = false;
        }

    }  // namespace sd
}  // namespace ungula

#endif  // ESP_PLATFORM
