// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include <cstdint>

namespace ungula::sd {


// SPI pin configuration for the SD card slot. Values are GPIO numbers
// specific to the board in use — the host project fills this in from
// its device_config.h.
//
// pin_cs = -1 means "CS managed externally" (e.g. via an I/O expander).
// The caller must assert CS LOW before calling mount().
struct EspSdSpiConfig {
        int8_t pin_miso = -1;
        int8_t pin_mosi = -1;
        int8_t pin_clk = -1;
        int8_t pin_cs = -1;  // -1 = externally managed CS

        // VFS mount point. Must start with '/' and be short (ESP-IDF limit
        // is 8 characters including the leading slash).
        const char* mount_point = "/sdcard";

        // Maximum number of open files. 2 is enough for audit log + one
        // concurrent reader (e.g. HTTP download of the log file).
        int max_files = 2;

        // Format the card if mount fails (first use with a blank card).
        bool format_if_mount_failed = false;
};

    }  // namespace ungula::sd
