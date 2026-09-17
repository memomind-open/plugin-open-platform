# GM Plugin ABI

This document defines the compatibility and lifecycle rules for native GM
plugins. The public headers under `include/` are the canonical source.

## Compatibility baseline

This pre-release revision uses GMP v2 only. Package ABI, plugin descriptor ABI,
and firmware Host ABI must match exactly. A mismatched package is rejected
before execution. There is no GMP v1 loading or wire fallback.

The public headers define the current core and extension table layouts. The
build compiles RV32 layout assertions. Future release compatibility policy is
separate from this first-stage strict matching rule.

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
- If `on_load` returns an error, it must clean up its partial state. The Host
  does not call `on_unload` for a failed load.
- `on_start` creates UI and starts one visible application cycle. A display root
  does not exist during `on_load`.
- If `on_start` returns an error, `on_stop` is not called. The failed callback
  must clean up its partial start/UI state.
- `on_stop` releases resources belonging to the visible cycle.
- `on_unload` releases load-lifetime resources before RAM data is
  freed. The cached Flash image remains available.

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
- Do not use TLS, external GOT imports, exceptions, RTTI, static constructors, dynamic
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

Code and read-only constants execute in CUS8 Flash. Writable data, BSS, GOT and
relocatable pointer tables use RAM. The package contains only RAM initial data;
BSS is zeroed at load. Constant pointer tables may require RAM even when their
pointed-to strings or assets remain in Flash.

The package storage limit is 3 MiB minus 64 KiB of directory space. Flash
code/constants must be at most 500 KiB. Static RAM data/BSS (including linked
padding, GOT and pointer tables) must be strictly below 100 KiB. This static
limit excludes dynamic heap, task stack and Host bookkeeping; allocation can
still fail under system pressure. There is one 512 B static address-slot table. RAM allocations include
up to 63 B alignment padding. No full-package RAM copy is used by SPP install.
Bluetooth installation uses independent blocks, normally 64 KiB before compression.
The bounded pipeline reuses one 65,824 B arena for encoded input, decoded output,
and next-block data in already-written prefixes. One existing BT payload of at
most 8192 B is parsed directly into its final TLV node; ownership of the whole
node passes to the worker without an extra 8 KiB copy. The arena is
released at the final COMMIT fence before runtime RAM is allocated, or on abort.
65,824 + 8192 B is a data-buffer subtotal, **not** the whole-system peak: directory,
protocol metadata and the existing 8 KiB worker stack also use RAM. OPEN temporarily
probes 8704 B receive headroom, then frees that probe; other tasks can allocate later.
Low-memory OPEN failures reduce the block size; ultimately plain 8 KiB CHUNKs
need no large arena. Unhelpful compression is carried as raw data through the same
pipeline. See [PROTOCOL.md](PROTOCOL.md#bounded-receive--decode--flash-pipeline).

Only one plugin is loaded at a time. Beginning a replacement stops and unloads
the current runtime. The cache may retain the old complete version until the
new candidate has been fully checked and committed.

The Host tracks plugin allocations by load/run lifetime and reclaims them on
stop/unload. Allocation headers, guards and tracking state are additional RAM;
the static-RAM build check does not measure or cap this dynamic footprint.

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

## Build memory and stack checks

`build.py` checks ROM and RAM independently. The linker and GMP packer both
reject code/constants above 512000 bytes and static RAM at or above 102400
bytes. The same limits apply in the phone, firmware loader and simulator.
The stored GMP also contains metadata and RAM initialization bytes; its total
size is not the same as the Flash code/constants segment size.

The C compiler launcher always requests GCC `-fstack-usage`. Each compiled
function, after optimization/inlining, must have a statically known frame of
at most **1024 bytes**. Larger frames and dynamic stack allocation (including
VLA/alloca) fail the build before assembly. Reports are saved beside each
object as `.su`; empty translation units are valid. A failed incremental check
removes the previous object and report rather than leaving stale checked output.

The glasses' display task and Flash worker each use **2048 four-byte words =
8192 bytes** of task stack. Plugin entry and lifecycle callbacks run on the
display task, sharing its stack with firmware and Host calls. There is no
separate 8 KiB allowance for each plugin callback. The 1024-byte check is a
per-function policy, not proof of maximum call-chain depth. Recursion, nested
callbacks, indirect calls and hand-written stack-changing assembly still need
review and on-device stack high-water measurements. Prebuilt objects or builds
that bypass the SDK launcher are not covered by this compiler check.

Use Host-managed heap for a large temporary buffer (the SDK equivalent of
malloc/free), and handle allocation failure:

```c
uint8_t *buffer = host->alloc(4096);
if (buffer == NULL) return GM_PLUGIN_ENOMEM;
/* Use the buffer while this lifecycle scope is active. */
host->free(buffer);
```

Do not replace a large local array with a large global just to silence the
stack check: it consumes the static-RAM budget for the entire loaded lifetime.
Truly read-only assets should be `static const` so their bytes stay in Flash.
