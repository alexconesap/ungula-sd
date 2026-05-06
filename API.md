# UngulaSd

Portable SD card filesystem abstraction for embedded C++17. Defines two
platform-agnostic interfaces (`ungula::sd::IFileSystem`, `ungula::sd::IFile`)
so application code stays free of ESP-IDF / VFS / FAT calls. Ships two ESP32
backends out of the box: SPI (`EspSdFilesystem`) and native 4-bit SDMMC
(`EspSdmmcFilesystem`). Both backends share the same `EspSdFile` (FILE*
wrapper) and the same `IFileSystem` surface, so swapping transports is a
config change.

---

## Usage

### Use case: mount, write text lines, read back, unmount (SDMMC, 4-bit)

```cpp
#include <ungula/sd/platform/esp/esp_sdmmc_config.h>
#include <ungula/sd/platform/esp/esp_sdmmc_filesystem.h>
#include <ungula/sd/i_file.h>

static constexpr ungula::sd::EspSdmmcConfig CFG = {
    .pin_clk = 7, .pin_cmd = 6,
    .pin_d0 = 15, .pin_d1 = 16, .pin_d2 = 17, .pin_d3 = 5,
    .mount_point = "/sdcard",
    .max_files = 2,
    .bus_width_4 = true,
    .format_if_mount_failed = false,
};

static ungula::sd::EspSdmmcFilesystem g_sd(CFG);

void setup() {
    if (!g_sd.mount()) { return; }

    auto* f = g_sd.open("/sdcard/test.txt",
                        ungula::sd::OpenMode::AppendBinary);
    if (f != nullptr) {
        f->write_line("hello sdmmc");
        f->flush();
        f->close();
        delete f;
    }

    f = g_sd.open("/sdcard/test.txt", ungula::sd::OpenMode::ReadBinary);
    if (f != nullptr) {
        char buf[128];
        while (f->read_line(buf, sizeof(buf)) > 0) {
            // process line in buf (newline stripped, null-terminated)
        }
        f->close();
        delete f;
    }

    g_sd.unmount();
}

void loop() {}
```

When to use this: ESP32 boards wired with native CLK/CMD/D0..D3 lines.
Higher throughput than SPI; required for fast bulk writes.

### Use case: SPI-mode SD with raw byte I/O

```cpp
#include <ungula/sd/platform/esp/esp_sd_config.h>
#include <ungula/sd/platform/esp/esp_sd_filesystem.h>
#include <ungula/sd/i_file.h>

static constexpr ungula::sd::EspSdSpiConfig CFG = {
    .pin_miso = 37, .pin_mosi = 35, .pin_clk = 36, .pin_cs = 34,
    .mount_point = "/sdcard",
    .max_files = 2,
    .format_if_mount_failed = false,
};

static ungula::sd::EspSdFilesystem g_sd(CFG);

void setup() {
    if (!g_sd.mount()) { return; }

    auto* f = g_sd.open("/sdcard/raw.bin",
                        ungula::sd::OpenMode::AppendBinary);
    if (f != nullptr) {
        const uint8_t payload[] = { 0xDE, 0xAD, 0xBE, 0xEF };
        f->write(payload, sizeof(payload));
        f->flush();
        f->close();
        delete f;
    }

    uint8_t buf[16] = {};
    f = g_sd.open("/sdcard/raw.bin", ungula::sd::OpenMode::ReadBinary);
    if (f != nullptr) {
        size_t got = f->read(buf, sizeof(buf));
        (void)got;
        f->close();
        delete f;
    }

    g_sd.unmount();
}

void loop() {}
```

When to use this: ESP32 boards that expose the SD slot through generic
SPI GPIOs. Simpler wiring, lower throughput than SDMMC.

### Use case: query free space and enumerate files

```cpp
#include <ungula/sd/platform/esp/esp_sd_config.h>
#include <ungula/sd/platform/esp/esp_sd_filesystem.h>

static ungula::sd::EspSdFilesystem g_sd(
    ungula::sd::EspSdSpiConfig{ .pin_miso = 13, .pin_mosi = 11,
                                .pin_clk = 12, .pin_cs = 10 });

static bool print_entry(const char* path, void* ctx) {
    auto* count = static_cast<int*>(ctx);
    ++(*count);
    // path is e.g. "/sdcard/audit_001.log"
    return true;  // false to stop early
}

void setup() {
    if (!g_sd.mount()) { return; }

    ungula::sd::SpaceInfo info{};
    if (g_sd.free_space(info)) {
        // info.total_bytes, info.free_bytes
    }

    int n = 0;
    g_sd.list_dir("/sdcard", &print_entry, &n);

    g_sd.remove("/sdcard/old.log");
}

void loop() {}
```

When to use this: housekeeping (rotation, capacity checks, deleting
old logs), filesystem maintenance.

### Use case: dependency-injected `IFileSystem` consumer

```cpp
#include <ungula/sd/i_filesystem.h>
#include <ungula/sd/i_file.h>

// Library code stays platform-agnostic — no ESP-IDF includes here.
class AuditWriter {
    public:
        explicit AuditWriter(ungula::sd::IFileSystem& fs) : fs_(fs) {}

        bool append(const char* path, const char* line) {
            auto* f = fs_.open(path, ungula::sd::OpenMode::AppendBinary);
            if (f == nullptr) { return false; }
            f->write_line(line);
            bool ok = f->flush();
            f->close();
            delete f;
            return ok;
        }

    private:
        ungula::sd::IFileSystem& fs_;
};
```

When to use this: any library or service that needs file persistence
without coupling to a specific MCU. The host project picks the backend
(`EspSdFilesystem`, `EspSdmmcFilesystem`, or a host-side mock) and
injects it.

---

## API

### Public types

#### `ungula::sd::OpenMode` (enum class, uint8_t)

File open mode. Intentionally minimal — extend when needed.

- `AppendBinary` — `"ab"`. Append-only, binary, creates the file if missing.
- `ReadBinary`   — `"rb"`. Read-only, binary.

There is no truncating-write, no read-write, and no seek. To overwrite,
`remove()` first then `open(AppendBinary)`.

#### `ungula::sd::SpaceInfo`

Returned by `IFileSystem::free_space()`.

```cpp
struct SpaceInfo {
    uint64_t total_bytes;
    uint64_t free_bytes;
};
```

#### `ungula::sd::DirEntryCallback`

```cpp
using DirEntryCallback = bool (*)(const char* path, void* ctx);
```

Invoked once per regular file entry by `list_dir()`. `path` is the full
path (e.g. `/sdcard/audit_001.log`). `ctx` is the opaque pointer passed
into `list_dir()`. Return `true` to continue, `false` to stop early.

#### `ungula::sd::EspSdSpiConfig`

ESP32 SPI-mode pin + mount config. Aggregate-initialised.

| Field | Type | Default | Meaning |
| ----- | ---- | ------- | ------- |
| `pin_miso` | `int8_t` | `-1` | MISO GPIO |
| `pin_mosi` | `int8_t` | `-1` | MOSI GPIO |
| `pin_clk`  | `int8_t` | `-1` | CLK GPIO |
| `pin_cs`   | `int8_t` | `-1` | CS GPIO; `-1` = externally managed (caller must hold CS LOW before `mount()`) |
| `mount_point` | `const char*` | `"/sdcard"` | VFS prefix; must start with `/`, max 8 chars including the slash (ESP-IDF limit) |
| `max_files` | `int` | `2` | Max simultaneously open files |
| `format_if_mount_failed` | `bool` | `false` | Format the card on first failed mount |

#### `ungula::sd::EspSdmmcConfig`

ESP32 native-SDMMC pin + bus config.

| Field | Type | Default | Meaning |
| ----- | ---- | ------- | ------- |
| `pin_clk` | `int8_t` | `-1` | CLK |
| `pin_cmd` | `int8_t` | `-1` | CMD |
| `pin_d0`  | `int8_t` | `-1` | DATA0 |
| `pin_d1`  | `int8_t` | `-1` | DATA1; `-1` forces 1-bit mode |
| `pin_d2`  | `int8_t` | `-1` | DATA2; `-1` forces 1-bit mode |
| `pin_d3`  | `int8_t` | `-1` | DATA3; `-1` forces 1-bit mode |
| `mount_point` | `const char*` | `"/sdcard"` | VFS prefix; same rules as SPI config |
| `max_files` | `int` | `2` | Max simultaneously open files |
| `bus_width_4` | `bool` | `true` | `true` = 4-bit, `false` = 1-bit (only D0) |
| `format_if_mount_failed` | `bool` | `false` | Format the card on first failed mount |

### Public interface: `ungula::sd::IFile`

Header: `ungula/sd/i_file.h`. Abstract base. Subclasses implement four virtuals;
the text helpers are non-virtual and built on top of them.

#### Virtual primitives

- `virtual size_t write(const void* data, size_t len) = 0;`
  Writes `len` bytes. Returns bytes actually written; less than `len`
  signals error or disk-full. No partial-write retry is performed
  internally.
- `virtual size_t read(void* buf, size_t len) = 0;`
  Reads up to `len` bytes. Returns 0 on EOF or error (the two are not
  distinguished).
- `virtual bool flush() = 0;`
  Forces buffered data to the physical medium. On ESP backends this calls
  `fflush` followed by `fsync` — without it, data lives in the FAT block
  cache and is lost on power cut or card removal.
- `virtual void close() = 0;`
  Releases the underlying handle. The object must not be used after this
  call. The destructor calls `close()` automatically.

#### Non-virtual text helpers

- `size_t write_str(const char* str)` — writes the C string without the
  null terminator. Returns 0 if `str == nullptr`.
- `size_t write_line(const char* str)` — writes the C string then a
  single `'\n'`. Returns total bytes (string + newline).
- `size_t write_line(const char* data, size_t len)` — writes `len` bytes
  then `'\n'`.
- `size_t read_line(char* buf, size_t max_len)` — reads until `'\n'`,
  EOF, or `max_len - 1` characters. The trailing `'\n'` is consumed but
  not stored. The buffer is always null-terminated. Returns the number
  of characters placed in `buf` (excluding the null). Returns 0 if
  `buf == nullptr` or `max_len == 0`, and 0 at EOF on an empty line.

### Public interface: `ungula::sd::IFileSystem`

Header: `ungula/sd/i_filesystem.h`. All methods are pure virtual.

- `virtual bool mount() = 0;`
  Mounts the underlying storage. Returns `true` if the medium is ready.
  Idempotent: calling on an already-mounted filesystem returns `true`
  immediately.
- `virtual void unmount() = 0;`
  Unmounts and releases the bus. After this, `open()` fails until the
  next `mount()`.
- `virtual bool is_mounted() const = 0;`
- `virtual IFile* open(const char* path, OpenMode mode) = 0;`
  Returns a non-null `IFile*` on success; `nullptr` on failure (card
  removed, path invalid, max_files exceeded, etc.). Ownership transfers
  to the caller — call `close()` then `delete` (or rely on the
  destructor when the pointer goes out of scope, after `delete`).
- `virtual bool free_space(SpaceInfo& out) const = 0;`
  Fills `out` with total/free bytes. Returns `false` if not mounted or
  the query fails.
- `virtual bool remove(const char* path) = 0;`
  Deletes a regular file by full path. Returns `false` if missing or
  cannot be removed.
- `virtual int list_dir(const char* dir_path, DirEntryCallback cb, void* ctx) = 0;`
  Calls `cb` once per regular file. Returns the number of entries
  visited (0 if the directory is empty or missing). Stops early when
  `cb` returns `false`.

### Public class: `ungula::sd::EspSdFile`

Header: `ungula/sd/platform/esp/esp_sd_file.h` (compiled only when
`ESP_PLATFORM` is defined).

```cpp
explicit EspSdFile(FILE* fp);
```

`IFile` implementation backed by a standard `FILE*`. Used internally by
both ESP backends; rarely instantiated directly. `flush()` calls
`fflush` then `fsync(fileno(fp))`.

### Public class: `ungula::sd::EspSdFilesystem`

Header: `ungula/sd/platform/esp/esp_sd_filesystem.h` (requires `ESP_PLATFORM`).

```cpp
explicit EspSdFilesystem(const EspSdSpiConfig& config);
```

ESP32 SPI-mode backend. Wraps `esp_vfs_fat_sdspi_mount()`. Inherits
all `IFileSystem` operations from `EspSdBaseFilesystem`; only
`mount()` / `unmount()` are SPI-specific.

### Public class: `ungula::sd::EspSdmmcFilesystem`

Header: `ungula/sd/platform/esp/esp_sdmmc_filesystem.h` (requires `ESP_PLATFORM`).

```cpp
explicit EspSdmmcFilesystem(const EspSdmmcConfig& config);
```

ESP32 native-SDMMC backend. Wraps `esp_vfs_fat_sdmmc_mount()`. Inherits
the same `IFileSystem` operations as the SPI backend; only mount logic
differs.

### Public method on both ESP backends: `last_error()`

```cpp
int last_error() const;  // inherited from EspSdBaseFilesystem
```

Returns the last ESP-IDF error code from `mount()`. `0` (`ESP_OK`) on
success. Useful for diagnostics when `mount()` returns `false`.

---

## Lifecycle

1. Construct the backend with a config struct (constexpr-friendly).
2. Call `mount()`. Check the boolean return; on failure inspect
   `last_error()` for the ESP-IDF code.
3. Open files with `open(path, OpenMode)`. The path **must** start with
   the configured `mount_point` prefix (e.g. `"/sdcard/..."`).
4. Use `read` / `write` / `write_line` / `read_line`.
5. Call `flush()` before `close()` if durability matters (audit logs,
   power-loss-tolerant data).
6. `close()` then `delete` the `IFile*`. The destructor also closes,
   but freeing the pointer is the caller's job.
7. `unmount()` when shutting down or before sleeping the bus.

Calling `open()` while not mounted returns `nullptr`. Calling `mount()`
twice is safe.

---

## Error handling

No exceptions, no error enums — just status returns:

| Call | Failure indicator |
| ---- | ----------------- |
| `mount()` | returns `false`; `last_error()` holds ESP-IDF code |
| `open()` | returns `nullptr` |
| `write()` | bytes-written `< len` |
| `read()` | returns `0` (EOF and error are indistinguishable) |
| `flush()` | returns `false` |
| `free_space()` | returns `false` |
| `remove()` | returns `false` |
| `list_dir()` | returns `0` for empty / missing dir |

There is no `errno`-style channel. If you need richer diagnostics from
ESP backends, read `last_error()` immediately after a failed `mount()`.

Common causes of `mount()` failure: wrong pin mapping, card not
inserted, card not FAT32, CS not asserted (when `pin_cs == -1`). Set
`format_if_mount_failed = true` only when initialising a known-blank
card — it will erase any existing filesystem.

---

## Threading / timing / hardware notes

- All calls are **blocking**. SD writes can stall for tens of
  milliseconds during FAT block flush; do not call from ISRs or
  real-time control loops.
- The library is **not thread-safe**. One filesystem instance per
  card slot, accessed from a single task. If multiple tasks need
  access, serialize externally.
- `flush()` is the only durability barrier. Without it, data sits
  in the FAT block cache and is lost on power loss or card removal.
- ESP backends require the ESP-IDF VFS + FAT components (included by
  default in Arduino-ESP32). The non-ESP target gates the platform
  headers behind `#ifdef ESP_PLATFORM`.
- `mount_point` is constrained by ESP-IDF to ≤ 8 characters including
  the leading `'/'`.
- File paths passed to `open()` / `remove()` / `list_dir()` must include
  the mount prefix — there is no implicit cwd.
- `EspSdSpiConfig::pin_cs == -1` means the host owns CS (e.g. via an
  I/O expander); CS must be LOW before `mount()` is called.

---

## Internals not part of the public API

Do not call these directly from application code:

- `ungula::sd::EspSdBaseFilesystem` (header
  `ungula/sd/platform/esp/esp_sd_base_filesystem.h`) — shared base class for
  the two ESP backends. Use `EspSdFilesystem` or `EspSdmmcFilesystem`
  instead. Only `last_error()` is meant to be called through the derived
  class.
- The protected member `EspSdBaseFilesystem::card_` is a `void*` aliasing
  ESP-IDF's `sdmmc_card_t*` to keep that header out of the public ABI.
  Do not reinterpret-cast it.
- `.cpp` translation units under `ungula/sd/platform/esp/` — backend
  implementation. Application code includes only the public headers.
- The umbrella header `ungula/sd.h` exists for Arduino's
  `#include`-based library discovery. Direct includes of `ungula/sd/i_file.h`
  / `ungula/sd/i_filesystem.h` / `ungula/sd/platform/esp/...` are equally valid.

---

## LLM usage rules

- Use only the symbols documented above. The public surface is
  `IFile`, `IFileSystem`, `OpenMode`, `SpaceInfo`, `DirEntryCallback`,
  the two `*Config` structs, and the two ESP backend classes (plus
  `EspSdFile` if hand-rolling another backend).
- Always check `mount()`'s return before opening files.
- Always pair `open()` with `close()` + `delete` (or stack a unique_ptr
  with a custom deleter — but do not let the `IFile*` leak).
- Call `flush()` before `close()` when the data must survive a power
  cut.
- File paths must start with the configured `mount_point`.
- Do not `seek`, `truncate`, or open in read-write mode — those modes
  do not exist in this API. To rewrite a file, `remove()` then `open()`
  with `AppendBinary`.
- Do not include ESP-IDF / `SD.h` / `FS.h` headers alongside this
  library — the whole point is to keep them encapsulated.
- Do not call `Arduino`'s `millis()` / `delay()` from code that uses
  this library; use the project's `ungula::TimeControl` if timing is
  needed (project-wide rule, not specific to this library).
- Do not depend on `EspSdBaseFilesystem` directly. Construct a
  concrete `EspSdFilesystem` or `EspSdmmcFilesystem`.
- Read errors and EOF look identical (`read()` returns 0). If the
  distinction matters, track expected length yourself.
