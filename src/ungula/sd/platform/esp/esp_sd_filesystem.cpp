// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa
// See LICENSE file for details.

#ifdef ESP_PLATFORM

#include "esp_sd_filesystem.h"

#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

namespace ungula::sd
{

    EspSdFilesystem::~EspSdFilesystem()
    {
        unmount();
    }

    bool EspSdFilesystem::mount()
    {
        if (mounted_) {
            return true;
        }

        if (config_.pin_miso < 0 || config_.pin_mosi < 0 || config_.pin_clk < 0) {
            return false;
        }

        // SPI bus configuration
        spi_bus_config_t bus_cfg = {};
        bus_cfg.mosi_io_num = config_.pin_mosi;
        bus_cfg.miso_io_num = config_.pin_miso;
        bus_cfg.sclk_io_num = config_.pin_clk;
        bus_cfg.quadwp_io_num = -1;
        bus_cfg.quadhd_io_num = -1;
        bus_cfg.max_transfer_sz = 4096;

        // Use SPI2_HOST (HSPI). SPI3_HOST is often taken by the display.
        esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            // ESP_ERR_INVALID_STATE means the bus is already initialized (shared
            // with another peripheral) — that is fine, we just attach our device.
            return false;
        }

        // SD SPI device configuration.
        // pin_cs < 0 means CS is managed externally (e.g. I/O expander) —
        // pass GPIO_NUM_NC so the driver does not toggle any GPIO for CS.
        sdspi_device_config_t slot_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
        slot_cfg.gpio_cs = (config_.pin_cs >= 0) ? static_cast<gpio_num_t>(config_.pin_cs) : GPIO_NUM_NC;
        slot_cfg.host_id = SPI2_HOST;

        // VFS FAT mount configuration
        esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {};
        mount_cfg.format_if_mount_failed = config_.format_if_mount_failed;
        mount_cfg.max_files = config_.max_files;
        mount_cfg.allocation_unit_size = 16 * 1024;

        sdmmc_host_t host = SDSPI_HOST_DEFAULT();

        sdmmc_card_t *card = nullptr;
        last_error_ = esp_vfs_fat_sdspi_mount(config_.mount_point, &host, &slot_cfg, &mount_cfg, &card);
        if (last_error_ != ESP_OK) {
            return false;
        }
        card_ = card;

        mounted_ = true;
        return true;
    }

    void EspSdFilesystem::unmount()
    {
        if (!mounted_ || card_ == nullptr) {
            return;
        }
        esp_vfs_fat_sdcard_unmount(config_.mount_point, static_cast<sdmmc_card_t *>(card_));
        spi_bus_free(SPI2_HOST);
        card_ = nullptr;
        mounted_ = false;
    }

} // namespace ungula::sd
#endif // ESP_PLATFORM
