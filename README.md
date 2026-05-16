# UngulaSd

> **High-performance embedded C++ libraries for ESP32, STM32 and other MCUs** — SD card filesystem abstraction (SPI and SDMMC).

> **LLM usage note:** if this library is consumed from a coding AI workflow, explicitly point the agent to `API.md` first. `API.md` is the LLM-facing contract (public API + examples + constraints) and avoids wasting time/tokens scanning source files and this human-oriented README.

Portable SD card filesystem abstraction. Provides a platform-agnostic file
I/O interface (`IFileSystem`, `IFile`) so consumers like emblogx's `SdSink`
never touch platform-specific SD/SPI APIs directly.

See [embLogX logging library](https://github.com/alexconesap/emblogx).

## Table of Contents

- [Interface](#interface)
  - [IFile API](#ifile-api)
- [ESP32 Implementation](#esp32-implementation)
  - [SPI mode](#spi-mode)
  - [SDMMC mode (native peripheral)](#sdmmc-mode-native-peripheral)
- [Testing](#testing)
- [Quick-start: verify SD card on your hardware](#quick-start-verify-sd-card-on-your-hardware)
  - [Example 1 — SDMMC mode, write and read text lines](#example-1-sdmmc-mode-write-and-read-text-lines)
  - [Example 2 — SPI mode, raw byte read/write](#example-2-spi-mode-raw-byte-readwrite)
  - [Example 3 — Integrate with emblogx audit logging](#example-3-integrate-with-emblogx-audit-logging)
  - [Example 4 — General-purpose text file logging](#example-4-general-purpose-text-file-logging)
- [Troubleshooting](#troubleshooting)
- [Acknowledgements](#acknowledgements)
- [License](#license)
- [Arduino CLI symlink note (rarely relevant)](#arduino-cli-symlink-note-rarely-relevant)

## Interface

| Header | Class | Purpose |
| ------ | ----- | ------- |
| `ungula/sd/i_file.h` | `IFile` | Read, write, flush, close. Text helpers included |
| `ungula/sd/i_filesystem.h` | `IFileSystem` | Mount, unmount, open files |

### IFile API

Low-level (virtual, must be implemented by each platform):

- `write(data, len)` — write raw bytes
- `read(buf, len)` — read raw bytes
- `flush()` — force buffered data to disk
- `close()` — release the file handle

High-level text helpers (non-virtual, built on top of the above):

- `write_str(str)` — write a C string without the null terminator
- `write_line(str)` — write a C string followed by `\n`
- `write_line(data, len)` — write bytes followed by `\n`
- `read_line(buf, max_len)` — read until `\n` or EOF, null-terminate

## ESP32 Implementation

Two transport modes are provided. Use whichever matches your board's SD card
wiring.

### SPI mode

For boards that wire the SD slot through generic SPI GPIOs (MOSI/MISO/CLK/CS).

| Header | Class | Purpose |
| ------ | ----- | ------- |
| `ungula/sd/platform/esp/esp_sd_config.h` | `EspSdSpiConfig` | SPI pin + mount config |
| `ungula/sd/platform/esp/esp_sd_filesystem.h` | `EspSdFilesystem` | SPI-SD via ESP-IDF VFS |
| `ungula/sd/platform/esp/esp_sd_file.h` | `EspSdFile` | FILE* wrapper |

### SDMMC mode (native peripheral)

For boards that use the ESP32's native SDMMC peripheral with dedicated
CLK/CMD/D0-D3 lines. Faster than SPI and supports 4-bit bus width.

| Header | Class | Purpose |
| ------ | ----- | ------- |
| `ungula/sd/platform/esp/esp_sdmmc_config.h` | `EspSdmmcConfig` | SDMMC pin + bus config |
| `ungula/sd/platform/esp/esp_sdmmc_filesystem.h` | `EspSdmmcFilesystem` | SDMMC via ESP-IDF VFS |

Both modes reuse the same `EspSdFile` (FILE* wrapper) and share the
`IFileSystem` / `IFile` interfaces, so application code is identical
regardless of transport.

Requires ESP-IDF SDMMC and VFS components (included in the default ESP32
Arduino core).

## Testing

```shell
cd lib_sd/tests
chmod +x *.sh
./1_build.sh
./2_run.sh
```

Host tests use a mock filesystem — no SD hardware needed.

---

## Quick-start: verify SD card on your hardware

The examples below are complete, self-contained Arduino sketches. Create a
new project, paste one of them into the `.ino` file, adjust the pin numbers
to match your board, and upload. Check your EmblogX console sink output to
see the results.

Make sure you have a FAT32-formatted microSD card inserted. On macOS, format
with Disk Utility using "MS-DOS (FAT)" — not exFAT.

### Example 1 — SDMMC mode, write and read text lines

For boards that route the SD slot through the ESP32's native SDMMC
peripheral (dedicated CLK/CMD/D0-D3 lines, not SPI).

```cpp
// Adjust the pin numbers below to match your board's schematic.

#include <emblogx/logger.h>
#include <emblogx/sinks/console_sink.h>
#include <ungula/sd/platform/esp/esp_sdmmc_config.h>
#include <ungula/sd/platform/esp/esp_sdmmc_filesystem.h>
#include <ungula/sd/i_file.h>

static constexpr ungula::sd::EspSdmmcConfig SD_CFG = {
    .pin_clk  = 7,
    .pin_cmd  = 6,
    .pin_d0   = 15,
    .pin_d1   = 16,
    .pin_d2   = 17,
    .pin_d3   = 5,
    .mount_point = "/sdcard",
    .max_files = 2,
    .bus_width_4 = true,
    .format_if_mount_failed = false,
};

static ungula::sd::EspSdmmcFilesystem g_sd(SD_CFG);
static emblogx::ConsoleSink g_console;

void setup() {
    emblogx::register_sink(&g_console);
    emblogx::init();
    log_info("SD card test (SDMMC 4-bit)");

    log_info("Mounting SD card...");
    if (!g_sd.mount()) {
        log_error("Mount failed. Check card and pin mapping.");
        return;
    }
    log_info("Mount OK");

    // Write three lines
    const char* path = "/sdcard/test.txt";
    auto* f = g_sd.open(path, ungula::sd::OpenMode::AppendBinary);
    if (!f) {
        log_error("Could not open file for writing");
        return;
    }

    f->write_line("First line written by ungula::sd");
    f->write_line("Second line");
    f->write_line("Third line — if you see all three, it works");
    f->flush();
    f->close();
    delete f;
    log_info("Wrote 3 lines");

    // Read them back, line by line
    f = g_sd.open(path, ungula::sd::OpenMode::ReadBinary);
    if (!f) {
        log_error("Could not open file for reading");
        return;
    }

    char buf[128];
    int line_num = 0;
    while (true) {
        size_t len = f->read_line(buf, sizeof(buf));
        if (len == 0) break;
        log_info("line %d: %s", ++line_num, buf);
    }
    f->close();
    delete f;

    g_sd.unmount();
    log_info("Done. You can remove the card and check test.txt");
}

void loop() {}
```

### Example 2 — SPI mode, raw byte read/write

Typical boards: generic ESP32 breakouts with SD slot wired through the SPI bus.
This example uses the low-level byte API instead of the text helpers.

```cpp
// Adjust the pin numbers below to match your board's schematic.

#include <emblogx/logger.h>
#include <emblogx/sinks/console_sink.h>
#include <ungula/sd/platform/esp/esp_sd_config.h>
#include <ungula/sd/platform/esp/esp_sd_filesystem.h>
#include <ungula/sd/i_file.h>

static constexpr ungula::sd::EspSdSpiConfig SD_CFG = {
    .pin_miso = 37,
    .pin_mosi = 35,
    .pin_clk  = 36,
    .pin_cs   = 34,
    .mount_point = "/sdcard",
    .max_files = 2,
    .format_if_mount_failed = false,
};

static ungula::sd::EspSdFilesystem g_sd(SD_CFG);
static emblogx::ConsoleSink g_console;

void setup() {
    emblogx::register_sink(&g_console);
    emblogx::init();
    log_info("SD card test (SPI, raw bytes)");

    log_info("Mounting SD card...");
    if (!g_sd.mount()) {
        log_error("Mount failed. Check card and pin mapping.");
        return;
    }
    log_info("Mount OK");

    // Write raw bytes
    const char* path = "/sdcard/raw_test.bin";
    auto* f = g_sd.open(path, ungula::sd::OpenMode::AppendBinary);
    if (!f) {
        log_error("Could not open file for writing");
        return;
    }

    const uint8_t payload[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03 };
    size_t written = f->write(payload, sizeof(payload));
    f->flush();
    f->close();
    delete f;
    log_info("Wrote %u bytes", static_cast<unsigned>(written));

    // Read them back
    f = g_sd.open(path, ungula::sd::OpenMode::ReadBinary);
    if (!f) {
        log_error("Could not open file for reading");
        return;
    }

    uint8_t readback[16] = {};
    size_t got = f->read(readback, sizeof(readback));
    f->close();
    delete f;

    log_info("Read %u bytes", static_cast<unsigned>(got));
    for (size_t i = 0; i < got; ++i) {
        log_info("byte[%u]=0x%02X", static_cast<unsigned>(i), readback[i]);
    }

    g_sd.unmount();
    log_info("Done");
}

void loop() {}
```

### Example 3 — Integrate with emblogx audit logging

Shows how to wire the SD filesystem into emblogx's `SdSink` so that
audit-level log records are persisted to the SD card automatically.

```cpp
// Compile with: -DEMBLOGX_ENABLE_SINK_SD=1

#include <emblogx/logger.h>
#include <emblogx/sinks/console_sink.h>
#include <emblogx/sinks/sd_sink.h>
#include <ungula/sd/platform/esp/esp_sd_config.h>
#include <ungula/sd/platform/esp/esp_sd_filesystem.h>

// Adjust the pin numbers below to match your board's schematic.
static constexpr ungula::sd::EspSdSpiConfig SD_CFG = {
    .pin_miso = 13,
    .pin_mosi = 11,
    .pin_clk  = 12,
    .pin_cs   = 10,
    .mount_point = "/sdcard",
    .max_files = 2,
    .format_if_mount_failed = false,
};

static ungula::sd::EspSdFilesystem g_sd(SD_CFG);
static emblogx::ConsoleSink g_console;
static emblogx::SdSink g_sd_sink(g_sd, "/sdcard/audit.log");

void setup() {
    // Register sinks and initialize
    emblogx::register_sink(&g_console);   // console: LOG target
    emblogx::register_sink(&g_sd_sink);   // SD: AUDIT target only
    emblogx::init();

    log_info("Audit logging to SD card");

    if (g_sd.mount()) {
        log_info("SD card mounted");
    } else {
        log_warn("SD mount failed, audit records will be dropped");
    }

    // LOG target only -> console, not SD
    log_info("System started, this does NOT go to SD");

    // LOG | AUDIT -> console AND SD
    log_audit_info("SYSTEM_BOOT firmware=test");

    // AUDIT only -> SD only
    audit_info("PROGRAM_SELECT idx=3 name=TestProgram");

    // Structured audit events with numeric codes
    audit_event(101, "cycle", "CYCLE_START program=3");
    audit_event(102, "cycle", "CYCLE_COMPLETE duration_ms=45000");
    audit_event(200, "settings", "SETTINGS_CHANGED temp=520 fan=30");
    audit_event(300, "safety", "EMERGENCY_STOP");

    log_info("Done. Remove SD card and check audit.log");
    log_info("Each record is one line, flushed immediately");
    log_info("Power loss mid-write won't corrupt completed lines");
}

void loop() {}
```

Expected console output:

```text
Audit logging to SD card

SD card mounted.
[LOG][INFO] System started, this does NOT go to SD
[LOG][INFO] SYSTEM_BOOT firmware=test
Done. Remove SD card and check audit.log.
```

Expected `audit.log` content on the SD card:

```text
[LOG][INFO] SYSTEM_BOOT firmware=test
[LOG][INFO] PROGRAM_SELECT idx=3 name=TestProgram
[LOG][INFO] code=101 CYCLE_START program=3
[LOG][INFO] code=102 CYCLE_COMPLETE duration_ms=45000
[LOG][INFO] code=200 SETTINGS_CHANGED temp=520 fan=30
[LOG][INFO] code=300 EMERGENCY_STOP
```

### Example 4 — General-purpose text file logging

Write and read a CSV-style log without emblogx, useful for data export
or any structured file I/O.

```cpp
#include <emblogx/logger.h>
#include <emblogx/sinks/console_sink.h>
#include <ungula/sd/platform/esp/esp_sd_config.h>
#include <ungula/sd/platform/esp/esp_sd_filesystem.h>
#include <ungula/sd/i_file.h>

// Adjust the pin numbers below to match your board's schematic.
static constexpr ungula::sd::EspSdSpiConfig SD_CFG = {
    .pin_miso = 13, .pin_mosi = 11, .pin_clk = 12, .pin_cs = 10,
    .mount_point = "/sdcard",
};

static ungula::sd::EspSdFilesystem g_sd(SD_CFG);
static emblogx::ConsoleSink g_console;

void setup() {
    emblogx::register_sink(&g_console);
    emblogx::init();

    if (!g_sd.mount()) {
        log_error("SD mount failed");
        return;
    }

    // Write CSV header + rows
    auto* f = g_sd.open("/sdcard/data.csv", ungula::sd::OpenMode::AppendBinary);
    if (!f) {
        log_error("Could not create file");
        return;
    }

    f->write_line("timestamp,temperature,fan_speed");
    f->write_line("1000,520,30");
    f->write_line("2000,525,31");
    f->write_line("3000,519,30");
    f->flush();
    f->close();
    delete f;

    // Read it back line by line
    f = g_sd.open("/sdcard/data.csv", ungula::sd::OpenMode::ReadBinary);
    if (!f) {
        log_error("Could not open file");
        return;
    }

    char line[128];
    while (f->read_line(line, sizeof(line)) > 0) {
        log_info("%s", line);
    }
    f->close();
    delete f;

    g_sd.unmount();
    log_info("Done");
}

void loop() {}
```

Expected output:

```text
timestamp,temperature,fan_speed
1000,520,30
2000,525,31
3000,519,30
Done.
```

---

## Troubleshooting

| Symptom | Likely cause | Fix |
| ------- | ------------ | --- |
| "FAILED. Check card and pin mapping." | Wrong pin numbers, card not inserted, or card not FAT32 | Verify schematic, re-seat card, reformat as FAT32 |
| Mount succeeds but file open fails | Mount point mismatch | File path must start with the same string as `mount_point` (e.g. `/sdcard/`) |
| Partial writes or empty file | Card full or write-protected | Check capacity, check physical write-protect switch on adapter |
| Compile error: `ungula/sd/platform/esp/...` not found | Library not on include path | Verify `lib_sd` is in your Arduino libraries folder or symlinked |
| `EMBLOGX_ENABLE_SINK_SD` has no effect | Missing `-D` flag | Add `-DEMBLOGX_ENABLE_SINK_SD=1` to your build flags |

## Acknowledgements

Thanks to Claude and ChatGPT for helping on generating this documentation.

## License

MIT License — see [LICENSE](license.txt) file.

---

## Arduino CLI symlink note (rarely relevant)

This library ships a flat forwarder header at `src/ungula_sd.h` that
just `#include`s `ungula/sd.h`. `library.properties` `includes=` points
at the forwarder.

It only exists to work around an Arduino CLI quirk: when the library is
consumed through a symlink, the CLI sometimes fails to discover headers
nested under `src/ungula/`. The flat forwarder fixes that scan.

**Host code keeps including the real header**:

```cpp
#include <ungula/sd.h>
```

PlatformIO, ESP-IDF component builds, and plain CMake setups can ignore
the forwarder.
