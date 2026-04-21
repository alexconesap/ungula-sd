// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#ifdef ESP_PLATFORM

#include <cstdint>

namespace ungula {
    namespace sd {

        // SDMMC pin configuration for boards that use the native SDMMC peripheral
        // with dedicated CLK/CMD/D0-D3 lines instead of SPI.
        struct EspSdmmcConfig {
                int8_t pin_clk = -1;  // CLK
                int8_t pin_cmd = -1;  // CMD
                int8_t pin_d0 = -1;   // DATA0
                int8_t pin_d1 = -1;   // DATA1 (-1 to use 1-bit mode)
                int8_t pin_d2 = -1;   // DATA2 (-1 to use 1-bit mode)
                int8_t pin_d3 = -1;   // DATA3 (-1 to use 1-bit mode)

                // VFS mount point. Must start with '/' and be short (ESP-IDF limit
                // is 8 characters including the leading slash).
                const char* mount_point = "/sdcard";

                // Maximum number of open files.
                int max_files = 2;

                // Use 4-bit bus width. When false, uses 1-bit mode (only D0).
                bool bus_width_4 = true;

                // Format the card if mount fails (first use with a blank card).
                bool format_if_mount_failed = false;
        };

    }  // namespace sd
}  // namespace ungula

#endif  // ESP_PLATFORM
