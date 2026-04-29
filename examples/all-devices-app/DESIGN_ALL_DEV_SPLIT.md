# All-Devices App: Configurable Device Subset Design

## Problem Statement

The `all-devices-app` currently compiles every supported device type unconditionally.
As the number of supported device types grows (targeting 20-30+), the binary grows with
it — which is problematic for constrained embedded targets (ESP32 and future MCUs) where
binary size matters. There is also no way to build a focused artifact (e.g. a "light app")
from this source tree without forking it.

Additionally, the device source list is currently duplicated between the GN build
(`device-factory/BUILD.gn`) and the ESP32 CMake build (`esp32/main/CMakeLists.txt`),
meaning new devices must be registered in multiple places.

## Goals

- Allow a build to compile and link only a declared subset of device types.
- Preserve the current "everything included" behavior when no subset is declared (zero
  friction for existing users and CI targets).
- Keep the source tree as a clean copy-paste blueprint for developers on both GN and CMake
  build systems — neither should feel like a workaround.
- Scale naturally: adding a new device type should not require opting out of it in every
  constrained build.

## Non-Goals (for now)

- Reducing compile time (each device compiles in O(seconds); 20-30 devices is acceptable).
- Trimming cluster server RAM allocations
- Enforcing that platform overrides respect the enabled-device set mechanically.

## Decisions

### 1. One source tree, configurable output name

There is one source tree. The output binary is named `all-devices-app` by default (when
all devices are enabled) and `example-device-app` when some device enabling is set.
the user can override the binary name via a build arg:

- **GN:** `all_devices_app_name = ""` — empty means default (`all-devices-app` or `example-device-app`);
  any non-empty value overrides it (e.g. `"my-light-app"`).
- **CMake:** equivalent variable with the same semantics.

No proliferation of named targets (`light-app`, `sensor-app`, etc.) in the source tree.

### 2. Opt-in device selection (empty = all)

Enabled devices are declared explicitly. An empty list means "all devices" — preserving
current behavior with no changes required for existing users.

- **GN:** `all_devices_enabled_devices = []` (empty = all)
- **CMake:** equivalent list variable

When a non-empty list is provided, only the listed device types are registered at runtime.
LTO eliminates unreachable device code from the binary.

### 3. Compile everything; LTO handles binary size

All device source files are always compiled regardless of the enabled-device list. The
`#if` guards (see §4) only gate the `RegisterCreator` calls in the factory constructor —
not `#include` directives and not GN/CMake dependencies. This means:

- BUILD.gn and CMakeLists.txt dependency lists remain unconditional (no conditional deps).
- LTO eliminates any device implementation not reachable from a registered creator.
- Meyer's singletons must be used for any device-level global state, so that unreferenced
  singletons are also eliminated by LTO.
- CI always validates compilation of all device code.

### 4. `#if` guards in DeviceFactory driven by a generated config header

A build-system-generated header provides `#define` macros for each enabled device type.
`DeviceFactory.h` includes this header and guards each `RegisterCreator` call with the
corresponding `#if`.

**GN path:**

- `all-devices-common/devices/enabled_devices.gni` — declares `declare_args` and drives
  a `build_config_header` target that generates the header.

**CMake path:**

- `all-devices-common/devices/enabled_devices.cmake` — declares equivalent variables and
  drives a `configure_file()` call that generates a header with identical `#define` names.

Both build systems produce a header with the same macro names (naming convention TBD at
implementation time, e.g. `ALL_DEVICES_ENABLE_ON_OFF_LIGHT`). `DeviceFactory.h` and
platform override files include this header — the `#if` guards are identical regardless
of which build system was used.

### 5. `mDefaultDevice` — first registered device wins

The `DeviceFactory` tracks a `mDefaultDevice` string. It is set once, on the first
`RegisterCreator` call, and never overwritten. The order of calls in the constructor
defines which device is the default. Platform overrides register after the constructor
runs, so they cannot steal the default.

### 6. `--device` flag stays, becomes optional

The `--device` CLI flag is kept for all build variants. Its behavior:

- If not specified, the app starts `mDefaultDevice` on endpoint 1.
- If specified, only device types present in the registry (i.e. compiled-in and enabled)
  are accepted; others produce an error.
- The flag may be specified multiple times for multi-endpoint configurations, even in
  single-device-type builds (e.g. `--device light:1 --device light:2`).
- `--help` lists only the compiled-in device types.

### 7. Platform overrides must respect the enabled-device set

`DeviceFactoryPlatformOverride` files (e.g. Linux's `PosixChimeDevice` registration) must
wrap their `RegisterCreator` calls in the same `#if` guards as the base factory. This is
enforced by documentation and code review, not mechanically — a platform that ignores the
guards will unconditionally compile that device in, which is its prerogative but not the
intended pattern.

### 8. Dual build system — GN and CMake maintained in parallel

The device configuration lives in two parallel files under
`all-devices-common/devices/`:

```
all-devices-common/devices/enabled_devices.gni    # GN
all-devices-common/devices/enabled_devices.cmake  # CMake
```

These are kept in sync manually (same device names, same generated macro names, same
semantics). This duplication is accepted because:

- Examples are copy-paste blueprints; CMake developers should not need GN.
- LLM assistance makes dual build system maintenance significantly less burdensome.
- Attempting to use GN as the single source of truth for CMake targets would feel like a
  workaround to CMake-native developers.

## Summary of Key Files Affected

| File | Change |
|---|---|
| `all-devices-common/devices/enabled_devices.gni` | **New** — GN args + build_config_header |
| `all-devices-common/devices/enabled_devices.cmake` | **New** — CMake equivalent |
| `all-devices-common/devices/device-factory/DeviceFactory.h` | Add `#if` guards around each `RegisterCreator` call; add `mDefaultDevice` tracking |
| `all-devices-common/devices/device-factory/BUILD.gn` | Import `enabled_devices.gni`; add `build_config_header` target |
| `posix/linux/DeviceFactoryPlatformOverride.cpp` | Wrap `RegisterCreator` calls in `#if` guards |
| `esp32/main/CMakeLists.txt` | Include `enabled_devices.cmake`; use generated header |
| Platform `BUILD.gn` / `CMakeLists.txt` | Accept `all_devices_app_name` / `all_devices_enabled_devices` args |
