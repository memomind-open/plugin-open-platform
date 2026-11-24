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
up to 63 B alignment padding. No full-package RAM copy is used for installation.

The official App manages bounded, optionally compressed delivery. With the
current default 64 KiB uncompressed block size, the reusable transfer arena is
65,824 B, plus one received payload of up to 8192 B. These are data-buffer sizes,
**not** a whole-system peak: directory state, protocol bookkeeping, allocation
overhead and the worker task stack also consume RAM. The implementation can
reduce buffering under memory pressure. Transfer buffers are released before
runtime RAM is allocated, or when installation is cancelled. See
[PROTOCOL.md](PROTOCOL.md#memory-during-installation-and-execution).

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

## Optional HOGP Input

Extension ID `4` exposes foreground KEY/REL/ABS/TOUCH callbacks without
changing the frozen core ABI. Discover it at runtime and keep existing controls
when unsupported. See [HOGP Input](INPUT.md) for layout, lifecycle and
tests.

## Optional Native BLE GATT

Extension ID `6` exposes a generic, native BLE Central/GATT client. Include
`gm_plugin_ble.h` and call `gm_plugin_ble_get(host, &ble)` after validating your
core Host. `gm_plugin_ble_gatt.h` defines the types; the complete function table
is in `gm_plugin_extensions.h`. This does not change the frozen core ABI or
allocate a new phone service/command ID. It is distinct from `bt_send()` and the
phone-facing [HOGP gateway wire protocol](BLE_ACCESSORY_PROTOCOL.md).

This is a pre-release contract requiring firmware that implements the **full**
ID `6` table. No minimum shipped firmware version or current Desktop Studio
implementation is claimed. Missing support returns `GM_PLUGIN_ENOTSUP`; disable
the feature or explain the requirement. Do not probe undocumented shorter
experimental ID `6` tables: there is no size/version field in this table, and
reading a tail pointer cannot establish that a shorter allocation is safe.
Once published, incompatible changes require a new extension ID.

### Operations and completion

| API | Behavior / completion |
| --- | --- |
| `get_capabilities` | Synchronous size/limits snapshot; does not open a client or access the radio |
| `open` / `close` | One client; open during a visible `on_start`/`on_resume` cycle, close before destroying callback context |
| `get_connected_devices` | `CONNECTED_DEVICE` entries then `CONNECTED_DEVICES_DONE`; includes already connected HOGP peers |
| `scan_start` / `scan_stop` | Filtered `SCAN_RESULT` events, followed by `SCAN_ENDED`; optional name prefix, RSSI and 16/128-bit service UUID |
| `connect` / `disconnect` | Connect completes with `CONNECTION`; disconnect's `RESULT` only acknowledges local revocation, not physical teardown |
| `discover_service` | Discover a known service UUID and its characteristics/descriptors, then `RESULT` |
| `discover_all` | Full metadata without a service UUID; reuse this session's complete snapshot when available, then `RESULT` |
| `read` / `write` | Use discovered characteristic **value handles**; read data or write completion uses `RESULT` |
| `subscribe` | Enable/disable by value handle; `RESULT.subscribed` is confirmed remote configuration; changes arrive as `NOTIFICATION` |
| `pair` | Explicit SDK security request; `RESULT` is separate from request acceptance, and is not permission to unbond |
| `get_session_state` | `SESSION_STATE` is the successful terminal event; errors use `RESULT` |
| `get_bonded_devices` | Owned `BONDED_DEVICE` entries then terminal `RESULT(GET_BONDED_DEVICES)` |
| `get_bonded_accessories` (optional ID `8`) | Same event sequence, also listing the App-bound HOGP accessory with an ownership marker |
| `unbond` | Exact owned identity, offline only; terminal `RESULT(UNBOND)` |

The glasses support one plugin GATT session and one system HOGP link. A
plugin attaching to the same live HOGP peer consumes no new LE slot. On
`RESULT(CONNECT)/GM_PLUGIN_EBUSY`, `properties` can be
`GM_BLE_CONNECT_PLUGIN_LIMIT` (disconnect the existing plugin session first),
`GM_BLE_CONNECT_SLOT_UNAVAILABLE` (no LE slot can be assigned to this
request, including when the remaining capacity is reserved for system HOGP),
or `GM_BLE_CONNECT_BOND_LIMIT` (plugin bond records are full; unpair a managed
accessory first).
The former name `GM_BLE_CONNECT_LE_SLOTS` remains a source-compatible alias
for the same value (`2`), not a slot count.
Zero or an unknown value is ordinary busy, including on older firmware.
No existing link is disconnected on rejection. No event or table layout changed.

For asynchronous requests, a synchronous `GM_PLUGIN_OK` means **accepted**, not
remote success. If submission returns an error, do not wait for a completion.
Use nonzero unique `request_id` values while requests are outstanding and match
both `session_id` and `request_id` when applicable. The Host assigns a new
session for each connect/attach; take it from the connection event, never reuse
the previous session. Client IDs, session IDs and scan IDs are different values.

Callbacks run on the display task, not the Bluetooth stack. Event pointers and
`data` are borrowed until the callback returns. Copy needed metadata/bytes using
the Host libc extension. Do not block, sleep, spin, or keep these pointers.
Delivery can pause under an overlay; resume can report `DATA_LOSS` (count in
`request_id`). With `properties == GM_BLE_DATA_LOSS_NOTIFICATION_ONLY`, only data
notifications for the reported session were dropped. Keep that session and its
subscriptions; the application decides how to resynchronize its stream. The
loss report may precede earlier, already queued notifications and does not mark
an exact stream boundary. Zero or an unknown value can include lost control
events. Discard an incomplete staged enumeration and explicitly retry after
unclassified loss/failure; never treat a partial list as success.

A successful scan also reports the currently connected HOGP peer, even when it
no longer advertises. Name-prefix and known HID UUID filters still apply; the
firmware does not infer other services. Such a row has `device.connected`, may
have `device.hid_ready`, and has no raw advertising payload (`data_length=0`).
RSSI 0 in this connected snapshot means unavailable; the scan RSSI threshold
continues to filter actual radio reports. Later same-address identity/base
aliases use the live peer's address tuple, so normal address/type deduplication
updates one row. No new event or API layout is introduced.

Scanning streams results without a per-scan device-count quota. The firmware's
12-entry recent-fragment/delivery cache rotates; it is not a scan limit or a
connection allowlist. Delivered entries are reused first, so later peers are
still admitted. Pending advertisement bytes are copied while waiting for UI
queue space. Extreme backpressure may replace pending entries; the real
stop/expiry then reports `SCAN_ENDED/GM_PLUGIN_ENOMEM` for partial delivery, not
an early radio stop or global `DATA_LOSS`. Results remain usable but are not a
guaranteed complete inventory. UUID/name fragments and HID advertising hints are
best-effort recent context; explicitly discovering 0x1812 can confirm HID after
a hint has aged out. Explicit stop/expiry drains pending rows before
the terminal event; HOGP preemption cancels unpublished rows and reports
`SCAN_ENDED/GM_PLUGIN_EBUSY` promptly. Genuine notification/response loss still
reports `DATA_LOSS` with the policy described above.

For `SESSION_STATE`, validate `data_length == sizeof(gm_ble_session_state_t)`
before copying into an aligned local struct. Validate its `struct_size` too.
The snapshot reports connection state, actual encryption, SDK bond presence,
management ownership, shared HOGP, busy, and confirmed subscription count.
When `SESSION_STATE.properties == GM_BLE_SESSION_STABLE_IDENTITY`, the
`event.device.address` and `address_type` fields are the verified stable
identity tuple (type 2 or 3) of this live, managed standalone bond. Compare
the tuple and session ID with the intended target before acting. Without this
marker, including on older firmware, the peer address can be the current or
last on-air address and cannot prove the target's identity. `event.mtu` is
zero after logical disconnection. This query does not occupy ATT or initiate
encryption.

### Minimal lifecycle integration

Resolve the optional table in `gm_plugin_entry` without opening a client. The
following functions can be integrated into the plugin's existing lifecycle;
they do not scan or connect automatically:

```c
#include "gm_plugin_ble.h"

static const gm_plugin_ble_gatt_extension_api_t *ble;
static uint32_t client;
static uint32_t session;

static void on_ble(void *context, const gm_ble_event_t *event)
{
    (void)context;
    if (event == NULL || event->struct_size < sizeof(*event)) {
        return;
    }
    if (event->type == GM_BLE_EVENT_CONNECTION) {
        session = event->device.connected ? event->session_id : 0;
    }
    /* Correlate responses and copy any borrowed data needed after return. */
}

static gm_plugin_result_t ble_start(void *context)
{
    gm_plugin_result_t result;
    if (ble == NULL) {
        return GM_PLUGIN_ENOTSUP;
    }
    result = ble->open(on_ble, context, &client);
    if (result != GM_PLUGIN_OK) {
        return result;
    }
    result = ble->get_connected_devices(client);
    if (result != GM_PLUGIN_OK) {
        ble->close(client);
        client = 0;
        return result;
    }
    return GM_PLUGIN_OK;
}

static void ble_stop(void *context)
{
    (void)context;
    if (client != 0) {
        ble->close(client);
        client = 0;
        session = 0;
    }
}
```

In entry, call `gm_plugin_ble_get(host, &ble)` and explicitly handle absence.
Bind `ble_start`/`ble_stop` to the corresponding lifecycle if BLE is required.
If start fails after opening, clean up immediately as above; stop is not called
after a failed start. If you close on suspend, reopen a new client/session on
resume. `close` revokes callbacks immediately; it is not an unbond request.
For safe capability-only discovery, see [the extension example](../examples/extension/README.md).
For interactive physical-device testing without a phone companion, use
[BLE GATT Lab](../examples/ble-gatt-lab/README.md). It exercises the full API,
shows async completion/failure, and connects directly from live scan rows.
Writes, offline unbond, and advanced manual pairing retain explicit confirmation.

### Discovery, bounds and security

Connection alone does not report the GATT catalog. Explicitly call
`discover_service(client, session, &uuid, request)` or
`discover_all(client, session, request)`. Full discovery reports `SERVICE`,
`CHARACTERISTIC`, `DESCRIPTOR`, and other `ATTRIBUTE` metadata in handle order.
Adopt the staged catalog only after terminal `RESULT(DISCOVER_ALL)` succeeds.
It does not read values or parse private vendor protocols. Secondary services
are distinguished by `SERVICE.properties`. Descriptors carry their owning
characteristic's value handle. UUID bytes use Bluetooth little-endian order;
pass the address and address type returned by scan/connected-device discovery
unchanged to `connect`. These native structs are not serialized GM TLVs.

After a successful full discovery, firmware retains that complete metadata
snapshot for this client/session. Repeated `discover_all` calls replay the same
events with the new request ID without another remote discovery. Targeted
discovery cannot overwrite the full snapshot. Values such as battery/name are
not cached. GATT/LE disconnect, client close, database invalidation and power-off
release it; no cross-session cache or Flash persistence is added.

For explicit remote rediscovery, resolve the independently optional extension
ID `7` with `gm_plugin_ble_refresh_get(host, &refresh)`, then call
`refresh->refresh_all(client, session, request)`. Its single-function table does
not grow or replace ID `6`. It uses the same `DISCOVER_ALL` events/completion,
invalidates the previous snapshot before discovery, and never reports an old
table as success after refresh fails. Handle `ENOTSUP` without reading past ID
`6` or disabling its existing operations. Generic databases with no automatic
invalidation require the developer to request refresh when they change; this
feature adds no Service Changed subscription or automatic reconnect policy.

Current bounds are one plugin connection or shared HOGP attachment, four
accepted but not yet consumed request completions, eight subscriptions, writes
of at most 64 B **and** MTU-3, and reads of at most 512 B. Full discovery is
limited to 160 raw attributes (including characteristic value handles),
16 services and 48 characteristics. If any of those limits is exceeded,
`RESULT(DISCOVER_ALL)` reports `GM_PLUGIN_ENOMEM` with
`properties == GM_BLE_DISCOVERY_CATALOG_LIMIT`. This result does not
disconnect the session or make a partial catalog valid; a plugin can use
targeted discovery for a known service UUID. Zero or an unknown `properties`
value is an unspecified `GM_PLUGIN_ENOMEM` and may come from older
firmware. The marker is not defined for other operations or result codes.
Targeted discovery supports four instances of the requested UUID and 48
connection-wide characteristics. No Prepare/Execute long writes, automatic
write replay, or background auto-reconnect are provided. Query capabilities rather than assuming
every device supports the maximum. Full discovery collection times out at
15 s; remote operations use 5 s and connect uses 10 s. Publication is bounded
and releases shared ATT before waiting for the consumer.

The API is generic, not specific to health devices. Developers interpret
standard/vendor data themselves. SDK pairing does not bypass proprietary
application authentication or implement a new PIN/passkey UI.

### HOGP, ANCS and ownership

Firmware shares the SDK transport and can attach both consumers to the same
physical HOGP connection, in either connection order. The WQ configuration has
three LE instances shared by phone/ANCS, HOGP, and an independent plugin peer;
this API does not increase that limit. There is no App-first connection requirement.
Explicit HOGP scanning can preempt plugin scanning (`SCAN_ENDED/EBUSY`).
Foreground HOGP setup is busy, not forcibly interrupted. An offline HOGP's background
whitelist initiator can yield through its normal cancel-completion path; the plugin
waits at most 5 s before starting its own scan/connect window, otherwise reports busy.
The system's background HOGP reconnect resumes after plugin radio work finishes.
Plugin filters affect only plugin results.
Do not infer a complete remote service database from advertising UUIDs.

Plugin `connect` establishes **generic GATT only**, even when the peripheral contains
HID services. It never enables a second system HOGP input device. Only an explicit
App/HOGP connection request promotes a peer; the single existing HOGP binding must
be removed before selecting another. The App may attach to a live plugin peer in
either connection order, without another physical link. Alternatively one HOGP peer
and a different generic plugin peer can coexist. An App scan does not discard the
already connected HOGP owner, including a connection still completing pairing.

Only the persisted, App-authorized HOGP peer is automatically reconnected at boot
or after link loss. SDK pairing keys (and resolving-list entries) do not grant that
authority. A standalone plugin peer reconnects only when the plugin calls `connect`;
no automatic firmware reconnect is added. If system reconnect finds its authorized
peer already connected by the plugin, it reuses that link. It does not adopt a
different HID-capable plugin peer or silently recreate a closed plugin session.

Background/suspend is not stop/close: the existing plugin client and connection are
retained unless the plugin explicitly closes them. Callback delivery can pause and
queues remain bounded; handle `DATA_LOSS` as above. Closing an independent plugin
session tears down only that generic link; closing a shared session leaves HOGP up.

Shared ATT waits for the existing system owner. HID (`0x1812`), Generic Attribute
(`0x1801`) and Battery Service (`0x180F`) characteristic values remain read-only.
Their Notify/Indicate CCCDs may be configured through `subscribe` on an independent
GATT link. On a shared HOGP link these services' CCCDs and all other system-reserved
CCCDs remain protected; subscribe cannot take over system-owned notifications.
The Host checks the live link owner on each request, including after App takeover.
Other services can be subscribed through the discovered value handles. Failed cleanup may
leave a remote CCCD enabled until physical disconnect. Closing a shared plugin
session leaves the system HOGP connection intact; system disconnect/unbond or
Service Changed can invalidate the plugin session/catalog.

Source tagging and HOGP key-snapshot handoff are private firmware mechanisms,
not SDK commands exposed to plugins. A plugin-only pairing does not overwrite
the HOGP backup; explicit HOGP takeover can copy existing SDK keys without a
new pairing. Phone Peripheral/ANCS policy is not controlled by this extension.
Validate simultaneous phone notifications, HID input, reconnect and power-loss
behavior on real hardware; host tests/builds are not hardware certification.

`get_bonded_devices` returns only this manager's fresh, explicitly recorded
standalone-plugin bonds, intersected with actual SDK records. It never exports
keys or the phone/global/HOGP/unknown legacy pairing table. The plugin bond
limit (`GM_BLE_BOND_MAX`, four) is shared by all plugin packages on the glasses.
At that limit, the Host rejects a new link to any peer without a recognized
existing bond, even if that peer permits unencrypted GATT access. The rejection
is a `RESULT(CONNECT)` with `GM_PLUGIN_EBUSY` and
`properties == GM_BLE_CONNECT_BOND_LIMIT`, not a `PAIR` result or a measurement
of free slots in the underlying SMP key store. Known bonded peers and
attachments to an existing shared HOGP link remain eligible for connection.
This check never evicts an existing bond. Direct the user to unpair a managed
accessory before retrying, or use the stable identity returned in the bonded
device list to reconnect to an existing bond.

Ownership survives plugin unload/reboot but is **not** a per-package security
ACL. Failed/corrupt ownership storage is conservative: encryption success alone
does not grant management authority. See [the native trust boundary](SECURITY.md).

For a reconnect/management UI, optionally resolve ID `8` with
`gm_plugin_ble_bond_list_get(host, &bond_list)` and call
`bond_list->get_bonded_accessories(client, request)`. It adds at most one
current App-bound HOGP peer with a usable SDK bond, marked by
`BONDED_DEVICE.properties & GM_BLE_BOND_SYSTEM_HOGP`. When it is also a
previous plugin bond, it appears only once. The list contains accessories only,
not phone/ANCS/global bonds, and is bounded by `GM_BLE_BONDED_ACCESSORY_MAX`.
Use the returned stable identity to connect GATT directly; the Host can attach
to an existing system HOGP link. Display the ownership marker on both the
reconnect and unpair lists, but direct users to the App to unpair a marked peer.
The marker is a snapshot: the Host still checks current ownership when an
unbond request executes. Without ID `8`, fall back to ID `6`'s unchanged
plugin-owned list.

Use the exact `BONDED_DEVICE` stable identity tuple (address type `2` public
identity or `3` random identity) for `unbond`. First disconnect and wait for the
actual `CONNECTION` event with `device.connected == 0`; the disconnect `RESULT`
is not sufficient. Active, shared/system/HOGP, unknown or rotating-address
targets are denied or busy. `RESULT OK` confirms SDK RAM deletion and ownership
storage; SDK key flash uses the original asynchronous FIFO and is not an atomic
power-loss transaction. If SDK deletion succeeded but ownership persistence
failed, the result is `EIO`; retry is idempotent. No global clear-pair-list or
automatic disconnect/reconnect is exposed.

On firmware supporting accessory-forget notifications, a successful App HOGP
unpair emits unsolicited `RESULT(UNBOND)` events with `request_id=0` and the
target in `device`. An exact verified alias, if known, may be reported
separately. Treat these events as cache invalidation for that peer, not as
request completions or a filter against new advertisements. Older firmware
may not send them; explicit plugin Unpair still reports its own request ID.

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
