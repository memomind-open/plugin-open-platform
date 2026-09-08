# GM Plugin ABI

This document defines the compatibility and lifecycle rules for native GM
plugins. The public headers under `include/` are the canonical source.

## Compatibility baseline

ABI 1.0 is the initial compatibility baseline. Firmware build versions and
plugin ABI versions are independent: an internal firmware change does not bump
the plugin ABI. Capability bits, event IDs, existing field offsets, function
signatures, and published behavior are immutable.

The core ABI does not accept additive changes after publication. New optional
modules use separately identified extension tables. Changed extension behavior
requires a new entry or extension ID while the Host keeps serving the old
interface. Future package formats are added beside GMP v1 rather than replacing
it.

Use `GM_PLUGIN_ABI_MIN_VERSION`, `GM_PLUGIN_HOST_API_MIN_SIZE`, and
`GM_PLUGIN_DESCRIPTOR_MIN_SIZE` to validate the frozen baseline. The
cross-platform build driver compiles fixed RV32 layout assertions. Repository CI additionally
compares public layouts and the wire contract against a frozen snapshot.

## Lifecycle

```text
gm_plugin_entry
  -> on_load
  -> zero or more on_start/on_resume/on_suspend/on_stop cycles
  -> on_unload
```

- `gm_plugin_entry` validates the Host and fills the Host-owned descriptor. It
  must not allocate or acquire resources because a rejected descriptor has no
  validated cleanup callback.
- `on_load` acquires non-UI resources that live for the loaded image.
- If `on_load` returns an error, the Host still calls `on_unload` before
  releasing the image. `on_unload` must safely clean up a partially initialized
  context. Do not free the same resources in both failure paths.
- `on_start` creates UI and starts one visible application cycle. A display root
  does not exist during `on_load`.
- If `on_start` returns an error, `on_stop` is not called. The failed callback
  must clean up its partial start/UI state.
- `on_stop` releases resources belonging to the visible cycle.
- `on_unload` releases load-lifetime resources before executable memory is
  freed.

Callbacks execute serially on the display task. They must not block, sleep,
spin, or retain borrowed event payload pointers.

## Entry and Host validation

- Plugins export only `gm_plugin_entry`.
- Validate the Host ABI version, table size, capabilities, and every function
  pointer used before storing the Host table.
- Preserve the descriptor capacity supplied by the Host.
- Store the Host function table and access firmware services only through it.
- Normal plugins should populate lifecycle callbacks at runtime. The packer can
  record base relocations needed by third-party libraries with static pointers.
- Do not use TLS, GOT imports, exceptions, RTTI, static constructors, dynamic
  allocation before `gm_plugin_entry`, or undefined symbols.

## Core and extensions

Core 1.0 and every published extension table are immutable. Incompatible
extension growth receives a new extension ID. Extension IDs are allocated only
in `include/gm_plugin_extensions.h`, so new modules do not change the frozen
core layout or consume capability bits.

The graphics aggregate points to its own versioned LVGL table. LVGL 1.1 keeps
the complete 1.0 prefix unchanged and appends image and frame-animation
functions. Plugins must check `api_version` together with the size macro for the
specific LVGL minor version they use; `GM_PLUGIN_LVGL_API_MIN_SIZE` continues to
name the frozen 1.0 prefix, while `GM_PLUGIN_LVGL_API_1_1_SIZE` includes the
image entries.

`manifest.json` contains package-time identity and version metadata only.
Runtime support is determined by Host ABI validation, capabilities, and
`extension_get` results.

`GM_PLUGIN_EXTENSION_RANDOM` exposes a non-cryptographic 32-bit pseudo-random
number service for UI, games, and similar non-security uses.

`GM_PLUGIN_EXTENSION_LZ4` exposes fixed one-shot raw LZ4 block operations.
It does not implement the LZ4 frame format or carry the original size in the
compressed payload. Plugins must keep both block sizes in their own protocol.

`GM_PLUGIN_EXTENSION_LIBC` exposes the Host's common memory, byte-string, and
bounded formatting operations. It lets freestanding plugins reuse optimized
firmware routines without importing libc symbols or embedding a private C
runtime. Table members deliberately retain recognizable C/POSIX names such as
`memcpy`, `strstr`, and `snprintf`; plugins call them as `libc->memcpy(...)`,
`libc->strstr(...)`, and `libc->snprintf(...)`. Each function retains its usual
pointer, overlap, termination, and capacity requirements. Resolve the table
with `gm_plugin_libc_get()` and see `gm_plugin_extensions.h` for each function's
complete contract.

Allocation remains in the tracked core `host->alloc`/`host->free` API. The
extension intentionally omits allocating string helpers, global-state
tokenization, and unbounded formatting functions so plugin unload accounting
and buffer bounds remain explicit.

## Runtime and memory

Plugins do not link firmware libraries, LVGL, libc, or FreeRTOS. The Host owns
shared runtime services. The `log` entry accepts a `printf`-style format and
arguments; the Host performs logging without a plugin-side persistent buffer.

The packer enforces the current 200 KiB package/runtime ceiling. GMP is padded
only when its allocation must cover the runtime image and BSS. On commit, the
Host converts the received allocation into the executable image in place. It
does not retain both a package buffer and a second runtime buffer.

Only one plugin is supported. Receiving a replacement unloads the previous
image before allocating the incoming package.

For firmware development leak checks, enable `GM_PLUGIN_ALLOC_LEAK_CHECK=1`.
The default production configuration points plugin allocation calls directly at
the firmware allocator without tracking headers or state.

## Events and services

- Bluetooth payload pointers are valid only for the duration of `on_event`.
  Copy any data required later.
- Button IDs, actions, and IMU gestures are stable `GM_PLUGIN_*` values, not
  internal firmware event numbers.
- Raw IMU is pull-based. Enable `GM_PLUGIN_IMU_ENABLE_RAW`, then call `imu_read`
  from `on_loop`. Disable IMU use when it is no longer needed.
- `app_exit` returns to the previous glasses application. Return promptly after
  calling it from a callback.
- Display controls use typed Host getters and setters. The Host restores only
  controls changed by the plugin when it stops.
- The Host owns the LVGL root returned to a plugin. Plugins may clean its
  children but must not delete the root.

Raw IMU example:

```c
host->imu_enable(GM_PLUGIN_IMU_ENABLE_GESTURES |
                 GM_PLUGIN_IMU_ENABLE_RAW);

gm_plugin_imu_sample_t sample;
if (host->imu_read(&sample) == GM_PLUGIN_OK) {
    /* sample.gyro_raw[], sample.accel_raw[], sample.pitch_degrees */
}
```

See [GRAPHICS.md](GRAPHICS.md) for rendering rules,
[CAPABILITY_MATRIX.md](CAPABILITY_MATRIX.md) for service mappings, and
[SECURITY.md](SECURITY.md) for the trust boundary.
