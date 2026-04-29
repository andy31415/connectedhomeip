# Implementation Plan: Configurable Device Subset

Tracks execution of the design in [DESIGN_ALL_DEV_SPLIT.md](DESIGN_ALL_DEV_SPLIT.md).

Each phase leaves the tree in a buildable, testable state. GN (POSIX) and CMake (ESP32)
are worked in parallel where independent; they converge in Phase 3 before any C++ changes
land.

---

## Phase 1 — CMake: extract device list into `enabled_devices.cmake`

**Goal:** Single authoritative device-list file for the CMake build. No behavior change.

### Tasks

- [x] Create `all-devices-common/devices/enabled_devices.cmake`
  - Define `ALL_DEVICES_DEVICE_SRCDIRS` list containing every device module path
    currently inlined in `esp32/main/CMakeLists.txt`
- [x] Update `esp32/main/CMakeLists.txt`
  - `include(...)` the new file
  - Replace the inline entries in `SRC_DIRS_LIST` with `${ALL_DEVICES_DEVICE_SRCDIRS}`

### Milestone verification

- `idf.py build` succeeds for the ESP32 target with no warnings introduced.
- The compiled binary is byte-for-byte identical to the pre-change build (or diff is
  limited to build metadata such as timestamps).
- `SRC_DIRS_LIST` in `CMakeLists.txt` no longer contains any device module paths
  directly — they all come from `enabled_devices.cmake`.

---

## Phase 2 — Parallel: wire config-header generation in both build systems

Both tracks must complete before Phase 3 can land. Neither track touches `DeviceFactory.h`
yet — the header is generated but nothing includes it.

### Track A — CMake

- [x] Create `all-devices-common/devices/enabled_devices_config.h.in`
  - One `#cmakedefine01 ALL_DEVICES_ENABLE_<UPPER_DEVICE_NAME>` line per factory
    registry key (e.g. `contact-sensor` → `ALL_DEVICES_ENABLE_CONTACT_SENSOR`,
    `on-off-light` → `ALL_DEVICES_ENABLE_ON_OFF_LIGHT`,
    `water-leak-detector` → `ALL_DEVICES_ENABLE_WATER_LEAK_DETECTOR`, etc.)
- [x] Extend `enabled_devices.cmake`:
  - Declare `ALL_DEVICES_ENABLED_DEVICES` variable (empty list = all devices enabled)
  - When empty: set all individual `ALL_DEVICES_ENABLE_xxx` CMake variables to `1`
  - When non-empty: set only the listed devices to `1`, rest default to `0`
  - Call `configure_file(enabled_devices_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/app_config/enabled_devices.h)`
  - Guard with `if(NOT CMAKE_BUILD_EARLY_EXPANSION)` to skip during the IDF
    requirements phase (when CMAKE is run in a temporary build tree)
- [x] `esp32/main/CMakeLists.txt`: append `${CMAKE_CURRENT_BINARY_DIR}` to
  `PRIV_INCLUDE_DIRS_LIST`

### Track B — GN

- [x] Create `all-devices-common/devices/enabled_devices.gni`
  - `declare_args()` block:
    - `all_devices_enabled_devices = []` (empty = all)
    - `all_devices_app_name = ""` (empty = auto)
  - Logic: compute per-device 0/1 variables from the list using the same
    `foo` → `ALL_DEVICES_ENABLE_FOO` naming convention
  - Export `_all_devices_header_lines` list — the verbatim content for the header
- [x] Update `all-devices-common/devices/device-factory/BUILD.gn`
  - `import("${chip_root}/build/chip/buildconfig_header.gni")`
  - `import("../enabled_devices.gni")`
  - Create `buildconfig_header("enabled_devices_buildconfig")` with
    `header = "enabled_devices.h"`, `header_dir = "app_config"`, and a `defines`
    list referencing the boolean per-device variables from the gni.
    `buildconfig_header` converts GN `true`/`false` → `1`/`0` automatically.
    Output lands at `${root_gen_dir}/include/app_config/enabled_devices.h`,
    which is already on the global include path — no extra config needed.
  - Add `":enabled_devices_buildconfig"` to `device-factory`'s `public_deps`

### Milestone verification (both tracks independently)

- **CMake:** `idf.py build` succeeds. Inspect build tree — file
  `app_config/enabled_devices.h` exists under the CMake binary dir. Every
  `ALL_DEVICES_ENABLE_xxx` macro is `1`. `DeviceFactory.h` is not yet modified.
- **GN:** `gn gen + ninja` for the Linux POSIX target succeeds. Inspect gen output —
  `app_config/enabled_devices.h` exists. Every macro is `1`. `DeviceFactory.h` is
  not yet modified.
- Confirm both headers define **identical macro names** for the same device (cross-check
  a few entries manually).

---

## Phase 3 — C++: add `#include` and `#if` guards; add `mDefaultDevice`

**Prerequisite:** Phase 2 both tracks complete — both build systems generate the header.

**Goal:** The C++ sources are guarded by the generated header. With all macros `1` (the
default), runtime behavior is identical to today.

### Tasks

- [x] `DeviceFactory.h`
  - Add `#include <app_config/enabled_devices.h>` (no fallback — missing header is a
    compile error by design)
  - Wrap each `mRegistry["key"] = …` block with
    `#if ALL_DEVICES_ENABLE_<KEY> … #endif`
  - Keep all `#include` directives at the top of the file unconditional (per design §3)
  - Add `std::string mDefaultDevice` member (initialized to `""`)
  - In `RegisterCreator`: `if (mDefaultDevice.empty()) mDefaultDevice = deviceTypeArg;`
    before the map insert
  - Add `const std::string & GetDefaultDevice() const { return mDefaultDevice; }`
- [x] `posix/linux/DeviceFactoryPlatformOverride.cpp`
  - Add `#include <app_config/enabled_devices.h>`
  - Wrap `RegisterCreator("chime", …)` with `#if ALL_DEVICES_ENABLE_CHIME … #endif`
- [x] `posix/darwin/DeviceFactoryPlatformOverride.cpp`
  - Same changes as the Linux file

### Milestone verification

- **GN Linux:** `ninja` succeeds. `all-devices-app --help` lists all eight device types.
  `all-devices-app --device on-off-light` starts successfully.
- **ESP32:** `idf.py build` succeeds with no new warnings.
- **`GetDefaultDevice()` check:** add a temporary log line (or use a debugger) to confirm
  the value is `"contact-sensor"` — the first key registered in the constructor. Remove
  the log before merging.
- **Guard spot-check:** temporarily define `ALL_DEVICES_ENABLE_ON_OFF_LIGHT 0` in a local
  edit and confirm compilation still succeeds (the `mRegistry["on-off-light"]` block
  disappears). Revert before merging.

---

## Phase 4 — Selective enabling: end-to-end on both build systems

**Goal:** A non-empty device list produces a binary that only registers the listed devices.

### Tasks — CMake

- [ ] Verify the non-empty path in `enabled_devices.cmake` works correctly (logic was
  written in Phase 2; this phase exercises it)
- [ ] Test build: set `ALL_DEVICES_ENABLED_DEVICES` to `"on-off-light;dimmable-light"`
  in `enabled_devices.cmake` (or via `-D` on the cmake invocation); build ESP32

### Tasks — GN

- [ ] Verify the non-empty path in `enabled_devices.gni` works correctly
- [ ] Test build: `gn gen --args='all_devices_enabled_devices=["on-off-light"]'`; build
  Linux POSIX target

### Milestone verification (repeat for each build system)

- Generated `app_config/enabled_devices.h` has only the selected devices as `1`; all
  others are `0`.
- `--help` output lists only the selected device types — no others appear.
- `--device on-off-light` (or whichever was selected) starts the app on endpoint 1.
- `--device chime` (a device not in the list) prints an error and exits non-zero.
- `GetDefaultDevice()` returns the first device in the enabled list (matches constructor
  registration order, not list order).
- When chime is excluded: `DeviceFactoryPlatformOverride.cpp` compiles without the
  chime `RegisterCreator` call (verify by checking the compiled binary has no chime
  symbol, or by reading the preprocessed output).
- Revert the test values before merging — default remains empty (all enabled).

---

## Phase 5 — Binary naming

**Goal:** Output binary is named `all-devices-app` when all devices are enabled and
`example-device-app` when a subset is declared. User override works in both modes.

### Tasks — CMake

- [ ] `enabled_devices.cmake`: compute `ALL_DEVICES_APP_NAME`
  - Empty `ALL_DEVICES_ENABLED_DEVICES` → `"all-devices-app"`
  - Non-empty → `"example-device-app"`
  - Non-empty `ALL_DEVICES_APP_NAME` (user-supplied) → use as-is regardless
- [ ] `esp32/CMakeLists.txt` (top-level): use `${ALL_DEVICES_APP_NAME}` for the project
  name / output artifact name

### Tasks — GN

- [ ] `enabled_devices.gni`: add computed name variable with the same three-way logic
- [ ] `posix/BUILD.gn`:
  - `import("../all-devices-common/devices/enabled_devices.gni")`
  - Set `output_name` on the `executable` target using the computed name

### Milestone verification

| Scenario | Expected binary name |
|---|---|
| Default (empty list, no override) | `all-devices-app` |
| Non-empty list, no override | `example-device-app` |
| Non-empty list, `all_devices_app_name = "my-light-app"` | `my-light-app` |
| Empty list, `all_devices_app_name = "my-light-app"` | `my-light-app` |

Verify each row for both GN and CMake. Confirm the old hardcoded `"all-devices-app"`
target name no longer appears in either build file as a string literal.

---

## Macro naming reference

| Factory key | Generated macro |
|---|---|
| `contact-sensor` | `ALL_DEVICES_ENABLE_CONTACT_SENSOR` |
| `water-leak-detector` | `ALL_DEVICES_ENABLE_WATER_LEAK_DETECTOR` |
| `occupancy-sensor` | `ALL_DEVICES_ENABLE_OCCUPANCY_SENSOR` |
| `chime` | `ALL_DEVICES_ENABLE_CHIME` |
| `dimmable-light` | `ALL_DEVICES_ENABLE_DIMMABLE_LIGHT` |
| `on-off-light` | `ALL_DEVICES_ENABLE_ON_OFF_LIGHT` |
| `speaker` | `ALL_DEVICES_ENABLE_SPEAKER` |
| `soil-sensor` | `ALL_DEVICES_ENABLE_SOIL_SENSOR` |

Rule: replace every `-` with `_`, uppercase the result, prefix `ALL_DEVICES_ENABLE_`.
New devices added to the factory in future follow the same rule automatically.
