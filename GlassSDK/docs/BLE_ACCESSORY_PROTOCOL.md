# BLE accessory gateway protocol

For a complete desktop/mobile workflow without the official App, start with
[HOGP quick start](HOGP_QUICKSTART.md): connect the controller, scan/connect an
accessory, decode directional input, and explicitly disconnect it.

This is the App-to-glasses **GM service `0x10`** for the glasses' BLE Central /
HID Host gateway. The accessory link is BLE; the App control link uses the
[GM transport and TLVs](PROTOCOL.md#gm-packet-framing). Do not connect to a
Classic SPP UUID as if it were a GATT characteristic. This service is separate
from plugin package installation and is not exposed as an arbitrary H5 Bridge
method by `device.messaging`.

The current implementation manages one accessory. Query capabilities before
using optional functionality. Do not treat a scan address, a permanent bond
identity, a GM event ID and a GATT transaction `seq` as interchangeable IDs.

## Minimal setup for standard HID buttons

A third-party native controller can replace the official App for accessory
setup by implementing the GM transport and this service. No executable plugin
or plugin-package transfer is required for the built-in HID navigation path.

The two links have independent roles: on an enabled phone-facing BLE GM link,
the glasses expose a GATT server to the controller; on the accessory link, the
glasses act as BLE Central, GATT Client and HID Host. The accessory is the HID
Device. This is concurrent role support, not a command that switches the
phone-facing link into a HID Host. The GM control link must be available in the
target firmware; accessory HOGP support alone does not enable that transport.

Follow the [quick-start scan/connect workflow](HOGP_QUICKSTART.md#3-scan-and-connect-an-accessory)
for the ordered requests and response handling. Check existing state before
scanning: reuse the desired connection, or explicitly disconnect another
accessory first. Wait for HID readiness after connect; an ACK only accepts the
request. Firmware performs pairing/security, HID discovery, Report Map parsing
and input notification subscription automatically.

Built-in keyboard mappings include the following HID usages (page `0x07`):

| Usage ID | Local action | Default UI event |
| --- | --- | --- |
| `0x004F` | Right | `EVTSYS_BUTTON_RIGHT` |
| `0x0050` | Left | `EVTSYS_BUTTON_LEFT` |
| `0x0051` | Down | `EVTSYS_BUTTON_DOWN` |
| `0x0052` | Up | `EVTSYS_BUTTON_UP` |
| `0x0028`, `0x0058` | Select / Enter | `EVTSYS_BUTTON_SINGLE` |
| `0x0029` | Back / Escape | `EVTSYS_BUTTON_BACK` |

These are HID usage identifiers, not fixed byte offsets in every report.
The accessory's Report Map defines its actual report layout. The default
navigation path applies when no custom mapping or foreground raw-input
subscription overrides it. Visible behavior depends on the foreground app
handling the corresponding event; a standard HID label alone does not prove
that every device/report layout is supported by this firmware.

`enable_hid_event` only enables a separate mirror to the controller (`0x06`);
it is not required for local navigation. `query_gatt_list`, private GATT
operations and `0x0B` input mappings are optional advanced features, not
prerequisites for standard keys. Disconnecting the controller link stops
forwarding and report-mode scans and abandons controller-driven GATT work;
it leaves the established accessory link intact so local HID input continues.

## Commands and response shapes

| Command | Direction | Payload / result |
| --- | --- | --- |
| `0x01` control | App → glasses | JSON TLV with `op`; response varies below |
| `0x02` scan results | Glasses → App | JSON device batch, separate from scan lifecycle |
| `0x03` GATT operation | App → glasses | JSON metadata, optional BYTES value |
| `0x04` GATT result/data | Glasses → App | JSON metadata, optional BYTES value |
| `0x05` GATT capabilities | Glasses → App | Fragmented JSON capability list |
| `0x06` HID event | Glasses → App | Normalized JSON or raw-report JSON + BYTES |
| `0x07` link state | Glasses → App | JSON connection/readiness state |
| `0x08` bond state | Glasses → App | JSON bond identity/state; not connection state |
| `0x09` scan state | Glasses → App | JSON scan lifecycle |
| `0x0A` HID descriptor summary | Glasses → App | Fragmented JSON field summary |
| `0x0B` input mapping | App → glasses | Versioned JSON rule table, separate from executable plugins |
| `0x0C` raw GATT database | Glasses → App | Fragmented JSON debug snapshot; not an operation handle catalog |

Ordinary immediate responses contain **STATUS TLV**, optionally followed by a
STRING error token. Successful ordinary responses have status 0 and no error
string. They do not generally contain JSON or echo a JSON `seq`; match the GM
response event ID. Nonzero generic status uses the system status table in
[PROTOCOL.md](PROTOCOL.md#system-service-0x01).

Exceptions: `query_cap`, `query_status` and normally `enable_hid_event` return
JSON directly. Memory failure can return a status response instead. A queued
request's ACK is not confirmation that scanning, connecting or unbonding has
finished. In particular, the current `unbond` handler ACKs queue acceptance;
wait for state events or query status for the resulting state.

## Control requests (`0x01`)

Send one UTF-8 JSON TLV. The current control/GATT metadata receive buffer accepts
1–255 bytes; keep metadata compact. This is not the limit of every GM service.

| `op` | Additional fields | Completion |
| --- | --- | --- |
| `query_cap` | None | JSON: `proto`, capability booleans, input-map version/limits |
| `query_status` | None | JSON state snapshot |
| `scan_start` | Optional `report` boolean (default true), `duration_ms` uint32 (default 0, host default) | ACK, then scan state/results |
| `scan_stop` | None | ACK, then scan-ended state |
| `connect` | `addr` as `AA:BB:CC:DD:EE:FF`; `addr_type` from scan result; optional `name` | ACK, then link progression |
| `disconnect` | None | ACK, then link state |
| `unbond` | None | ACK, then cleanup/state changes |
| `query_gatt_list` | None | ACK, then capability list; needs HID-ready connection |
| `query_gatt_db` | None | ACK, then debug database; needs HID-ready connection |
| `query_hid_desc` | Optional `include_usages` boolean | ACK, then descriptor summary; needs HID readiness |
| `enable_hid_event` | `enable` boolean; optional `raw` boolean | JSON `{enable, raw, rate_hz}` |

Example control body: `{"op":"query_cap"}`. The `op` key is not `cmd`.
The current enable flag also accepts `enabled`, but prefer `enable`. Raw reports
are an independent opt-in and default off. `rate_hz` in the response describes
host throttling; this implementation does not parse it as a caller-set rate.

A bonded accessory blocks connecting a different one: handle
`ERR_ALREADY_BONDED` by explicitly completing unbond before replacing it.
Do not persist an RPA as the permanent accessory identity.

A link snapshot includes `state`, `reason`, `err`, `connected`, `bonded`,
`hid_ready`, `gatt_ready`, `addr`, and `addr_type`, with identity/name information
when available. The current wire format uses numeric `0`/`1` for the four
connection/bond/readiness flags; accept these as well as boolean equivalents.
Use the readiness flags: connected does not imply HID ready,
and HID ready does not imply that private GATT discovery is complete. Treat
numeric state/reason values as host enums, not booleans. Clear handle caches
when the link or capability-list generation changes.

## Private GATT operations (`0x03`)

First obtain a current capability list. Operations use its `list_id` and
**characteristic value handle**, not a declaration handle or raw debug DB entry.

```json
{"seq":12,"op":"read","list_id":1,"value_handle":37}
```

```json
{"seq":13,"op":"subscribe","list_id":1,"value_handle":37,"cccd_handle":38,"subscribe_type":"notify"}
```

| Field | Contract |
| --- | --- |
| `seq` | Required uint16 request ID; do not reuse while pending (`req_id` accepted as an alias) |
| `op` | `read`, `write`, `write_no_rsp`, `subscribe`, `unsubscribe` |
| `list_id` | Required uint8 current capability generation |
| `value_handle` | uint16 characteristic value handle |
| `cccd_handle` | uint16 CCCD handle for subscription operations |
| `subscribe_type` | `notify` or `indicate`, matching characteristic properties |
| `value` | Optional hexadecimal write bytes only when BYTES TLV is absent |

For writes, append a BYTES TLV after JSON. BYTES takes precedence over JSON
`value`; `value_hex` is **not** a downlink field. Omitting both payload forms is
invalid. Do not assume long/prepare writes or automatic splitting: oversized
writes can be rejected. The current link, readiness, snapshot and characteristic
properties are checked. An outdated `list_id` returns `ERR_STALE_GATT_DB`;
requery capabilities instead of retrying an old handle.

## Asynchronous results (`0x04`)

Read result JSON example:

```json
{"seq":12,"event":"read_result","list_id":1,"value_handle":37,"att_err":0}
```

The bytes, if present, follow in a BYTES TLV. Result events are `read_result`,
`write_result`, `write_no_rsp_result`, `subscribe_result`, `unsubscribe_result`.
Match the JSON `seq`; immediate GM ACK correlation is a different layer.
`att_err=0` indicates success at the operation's ATT/local-send level.

Unsolicited notification metadata has `event:"notify"`, `seq:0`, `list_id` and
`value_handle`; do not require `att_err` on notifications. A `write_result`
reflects ATT response outcome. `write_no_rsp_result` confirms local submission
only, not execution by the accessory. Define an accessory business reply if
end-to-end confirmation is required.

## HID and failures

Normalized `0x06` events contain `type`, `key`, `phase`, `src`, `axis`, `rid`,
`app`, `val`, `norm`, `x`, `y`, `mod`, `page`, `usage`, `ts`. `type` distinguishes
`hid_key`, `unknown_usage` and `vendor_usage`. `val` is raw, `norm` is normalized;
retain usage-page/application context instead of guessing from `key` alone.
Raw reports use JSON `{ "type":"hid_raw", "rid":..., "len":... }` followed by
BYTES. Validate the byte count against `len`. Events may be throttled or dropped
under queue pressure; do not treat this as a lossless sensor-recording stream.

Handle `ERR_APP_NOT_READY`, `ERR_STALE_GATT_DB`, `ERR_GATT_BUSY`, `ERR_DUP_SEQ`,
`ERR_HANDLE_NOT_FOUND`, `ERR_PROP_NOT_SUPPORTED`, `ERR_PAYLOAD_TOO_LARGE`,
`ERR_AUTH_REQUIRED`, `ERR_QUEUE_FULL`, and malformed-body errors distinctly.
An error string can also be lowercase (`bad body`, `bad json`, `no op`, `need op`);
do not require an `ERR_` prefix to recognize failure. Do not assume every accepted
request must produce a timely result: enforce an application deadline and
refresh status after disconnect/timeouts. Avoid blind replay of writes.

## Scan and fragmented discovery data

`0x02` scan results contain `{seq:0, scan_id, devices:[...]}`. Each device contains
`adv_addr`, `adv_addr_type`, `rssi`, and optional name/service-UUID information.
Use the same scan's address/type for connecting. In report mode the current
firmware only surfaces devices with a nonempty name and recognized HID service
`0x1812` or supported HID appearance, after merging advertising and scan-response
data. An empty result does not prove that no BLE devices are nearby. `0x09` is separate and contains
`seq:0`, `scan_id`, `state` (`scanning` or `ended`) and `reason`; a final empty
scan batch is not a substitute for this lifecycle event.

The discovery commands each use JSON fragments with `seq:0`, `list_id`, zero-based
`index` and fragment count `total`. Collect all indexes for the same command and
list before publishing the snapshot; never combine lists from different sessions.
A new list, disconnect or timeout invalidates a partial assembly. These are
application-level fragments; GM framing may independently split each message.

- `0x05` has `items`: each characteristic contains `service_uuid`, `char_uuid`,
  `value_handle`, `cccd_handle`, `properties`, `domain`. UUID strings use uppercase
  hexadecimal without hyphens. Keep property bits and validate read/write/notify
  support. This list's generation is the one used by GATT operations.
- `0x0A` has `fields`: each HID field contains `rid`, `app`, `page`, `umin`, `umax`,
  `lmin`, `lmax`, `var`, `rel`; requested explicit `usages` can be appended. Its
  list ID identifies this descriptor snapshot, not the operative GATT generation.
- `0x0C` has `attr_total` and `attrs`: a diagnostic attribute snapshot. Its list
  ID likewise must not replace the GATT capability list's operation token.

## Input maps are data, not executable plugins

`0x0B` installs a versioned HID mapping table. Query `inputmap`, `inputmap_frag`,
`inputmap_version`, `inputmap_max` and `inputmap_caps` via `query_cap` first.
Current schema version is 1; limits are per table/record type. This mapping data
is unrelated to GMP installation or `device.messaging` channels. Do not infer
permission to download native code from support for this data command.
The complete mapping schema and logical key numbers are documented below.
Use the actual accessory report descriptor when selecting usage matchers.

## End-to-end connection and event sequence

The phone does not write directly to a ring characteristic when using this
service. It sends GM service `10` requests to the **glasses**; the glasses are
the BLE Central and perform accessory GATT operations.

```mermaid
sequenceDiagram
    participant P as Native phone client
    participant G as Glasses GM / HOGP relay
    participant R as BLE HID accessory
    P->>G: 10/01 JSON query_cap, then query_status
    G-->>P: JSON capability and state responses
    P->>G: 10/01 JSON scan_start
    G-->>P: STATUS ACK, then 10/09 scan state and 10/02 batches
    P->>G: 10/01 JSON connect with scanned address/type
    G-->>P: STATUS queue ACK
    G->>R: Connect, pair/encrypt as needed, discover HID, subscribe
    G-->>P: 10/07 link/readiness and 10/08 bond events
    R-->>G: HID report notification (local input works at HID readiness)
    Note over G: Decode and dispatch local navigation/input
    opt Optional controller mirroring and private GATT access
    P->>G: 10/01 query_gatt_list and enable_hid_event
    G-->>P: ACK/list fragments; JSON HID forwarding configuration
    P->>G: 10/03 JSON seq/read/current list_id/value_handle
    G-->>P: STATUS ACK
    G->>R: ATT read
    R-->>G: ATT result
    G-->>P: 10/04 JSON read_result plus BYTES
    R-->>G: HID report notification
    G-->>P: 10/06 normalized JSON, optionally raw JSON plus BYTES
    end
```

Do not wait for a GATT read result on command `03`: it arrives on `04`. Do not
match an unsolicited HID report to the last GM request. Maintain separate
pending tables for GM event IDs and GATT JSON `seq` values. An App-link disconnect
turns off HID forwarding, clears pending GATT tracking, stops report-mode scans,
and flushes queued GATT work; reconnecting requires status/list refresh and
re-enabling forwarding.

There is no separate public `pair` control op in this handler. The Host manages
pairing during connect. Observe bond and readiness events; a connection ACK is
not a bond-complete event. `disconnect` does not mean `unbond`.

## Byte encoding and examples

The request body for `query_cap` is the 18 UTF-8 bytes of:

```json
{"op":"query_cap"}
```

JSON is serialized without a NUL terminator. Encode it as TLV type `05`, followed
by a three-byte big-endian byte count, followed by its UTF-8 bytes, and wrap it in
GM service `10`, command `01`. JSON numeric values are **decimal**, even when
this document writes command IDs or handles in hexadecimal. A handle `0x0025`
is therefore written as `37`, not the invalid JSON token `0x25`.

The [wire appendix](WIRE_EXAMPLES.md#hogp-capabilities) contains the complete
Hex and individual byte offsets for capabilities, status, scan, connect, HID
forwarding, GATT discovery, descriptor query, read, write, subscribe,
unsubscribe, input mapping, disconnect, and unbond. Its sample handles and
address are illustrative; replace them with values from the current connection.
For a GATT write, the JSON TLV is followed by BYTES, e.g. `01 00 00 02 01 02`
for two value bytes `01 02`. Those bytes are the accessory value, not another
nested GM command. A CCCD handle in command `03` belongs to the accessory;
the phone-facing GM notification CCCD is a different descriptor.

## Readiness enums and characteristic properties

| `state` | Meaning |
| ---: | --- |
| 0 | Idle |
| 1 | Connecting, including pairing/discovery/subscription |
| 2 | Ready: connected and HID ready |
| 3 | Connection attempt failed |
| 4 | Disconnected after a connection |

Numeric `reason`: 0 none, 1 App connect, 2 App scan, 3 boot reconnect,
4 link loss, 5 manual, 6 App query, 7 stopped, 8 timeout, 9 App disconnected,
10 completed. Link `err`: 0 none, 1 timeout, 2 missing bond, 3 peer not found,
4 authentication failure. These numeric link reasons differ from the textual
`reason` in scan-lifecycle events. Preserve unknown future values for diagnostics.

`properties` is a GATT bitmask: `02` read, `04` write without response,
`08` write, `10` notify, `20` indicate (hexadecimal bit values). Select an
operation supported by the returned mask. Subscribe type `notify` and
`indicate` are not interchangeable. For subscription results, wait for the
asynchronous completion before assuming notification delivery is active.

Normalized HID event `phase`: 0 down, 1 up, 2 repeat, 3 move, 4 cancel.
`src`: 0 generic, 1 keyboard, 2 mouse, 3 consumer, 4 digitizer, 5 gamepad,
6 vendor. Signed `val` and normalized `norm` are JSON numbers, not byte arrays.
Use descriptor usage ranges and report IDs to interpret raw BYTES; a HID
report's bits and field widths come from the accessory report descriptor,
not a universal fixed ring report format.

## Input-map schema and persistence

The mapping command is `10/0B`, JSON TLV. It replaces the **whole** mapping table;
it is not a patch to a single rule. The Host copies, applies, and persists the
accepted map on its main task. The immediate ACK confirms queue acceptance,
not completion of the nonvolatile write. An empty `rules` array restores an
empty override table, allowing built-in HID mappings to apply.

```json
{"version":1,"rules":[{"type":"usage_key","page":12,"usage":205,"key":10}]}
```

This maps Consumer usage `0x000C/0x00CD` to logical Play/Pause. Numbers in the
JSON are decimal. This is a data table and does not install executable code.

Common matcher fields for `usage_key`, `value_match`, `rel_axis`, and
`abs_axis_to_key`:

| Field | Type / requirement |
| --- | --- |
| `type` | Exact rule-type string |
| `page`, `usage` | Required uint16 numbers |
| `application` | Optional uint32; 0 means any application collection |
| `rid` | Optional uint8; 0 means any report ID |

All numeric fields must be integers in range. A key is a logical key number
from the table below, or 65535 for an unset/disabled slot where supported.
Do not use a USB HID usage number as a logical key number.

| Rule | Additional fields | Meaning / current capacity |
| --- | --- | --- |
| `usage_key` | Required `key` | Map a usage to a key; 16 rules |
| `value_match` | Required `values:[{value,key},...]`, signed 32-bit `value` | First matching value wins; 4 rules, at most 6 values each |
| `rel_axis` | Optional `positive`, `negative` keys | Dispatch by sign for each nonzero relative report; 4 rules |
| `abs_axis_to_key` | Optional signed `deadzone`, `threshold`, and `positive`, `negative` keys | Edge-triggered in normalized -1000..1000 units; default deadzone 300, threshold 500; 4 rules |
| `abs_region` | `axis` (`x`/`y` or 0/1), `tip:{page,usage}`, `x:{page,usage}`, optional/conditional `y:{page,usage}`, `regions:[{min,max,key},...]`, optional application/rid | On tip-up, map the final coordinate to an inclusive range; 2 rules, 6 regions each |
| `touch_gesture` | Optional application/rid, tip/x/y usage objects, `tap_pct`, `scroll_engage_pct`, `scroll_step_pct`, `swipe`, `scroll`, and key bindings | Continuous touch gestures; 2 rules |

For absolute axes, select nonnegative deadzone/threshold values appropriate to
the normalized range and use a threshold beyond the release deadzone. For
regions, `min` and `max` are signed 32-bit integers with min <= max. `tip` is
required; X is required for a nonempty region table and Y is additionally
required for Y-axis regions. Region matching uses the accessory's coordinates,
not HUD pixel positions.

Touch defaults are: tip usage `000D/0042`, X `0001/0030`, Y `0001/0031`;
`tap_pct=0` selects default 8, `scroll_engage_pct=0` selects 25, and
`scroll_step_pct=0` selects 15. Explicit percentages are 0–100. `swipe:false`
and `scroll:false` disable those recognizers. Key slots are `tap`, `swipe_up`,
`swipe_down`, `swipe_left`, `swipe_right`, `scroll_up`, and `scroll_down`;
put them in a `map` object, or directly in the rule if `map` is absent.
Unspecified slots use the recognizer's defaults; explicit 65535 disables a
slot. Decreasing coordinates mean up/left/scroll-up; swap mapped keys to invert.
Read the accessory descriptor instead of assuming all devices use the same axes.

Always query `inputmap_caps` and `inputmap_max`; the capacities above describe
the inspected implementation. Unknown types and wrong versions are rejected;
a success ACK must not be inferred for a partially valid map.

### Large mapping tables

Small maps are sent directly. Larger maps use ordered JSON-text fragments:

```json
{"xfer_id":1,"frag":0,"frag_count":2,"total_len":300,"crc32":123456789,"chunk":"..."}
```

The numbers and chunk above are **shape placeholders**, not a valid complete
transaction. The exact contract is:

- Serialize the full `{"version":1,"rules":[...]}` text as UTF-8 once.
- `total_len` is the full UTF-8 byte count, not character count. Current maximum
  is 8192 bytes; each outer envelope must contain fewer than 2048 UTF-8 bytes.
  The usual control/GATT 255-byte metadata limit does not apply to this command.
- `crc32` is CRC-32/ISO-HDLC over those full UTF-8 bytes: reflected polynomial
  `EDB88320`, initial `FFFFFFFF`, final XOR `FFFFFFFF`; check vector
  `123456789` -> `CBF43926`. Python `zlib.crc32(data) & 0xffffffff` matches.
- `xfer_id` is uint32. `frag` and `frag_count` are uint16, with nonzero count
  and zero-based fragment indexes. Keep all envelope metadata identical across
  the transaction except `frag` and `chunk`.
- `chunk` is a JSON string containing a slice of the **original JSON text**.
  Escape it once as a string in the outer envelope. Split on UTF-8 character
  boundaries. Reassembly concatenates decoded chunk strings; the outer JSON's
  escaping bytes are not counted in total_len or CRC.
- Wait for STATUS success after each fragment before sending the next. A new
  fragment 0 replaces unfinished assembly. A gap or stale transfer is rejected
  with `ERR_FRAG_SEQ`; size and checksum failures use
  `ERR_INPUTMAP_TOO_LARGE` and `ERR_FRAG_CRC`.
- Only after the last fragment passes whole-text length/CRC and schema checks
  is the completed map queued for replacement. Phone disconnect abandons an
  unfinished assembly. Restart from fragment 0 after failure.

This is **public HOGP configuration transfer**, not the withheld GMP installer.
It can itself be fragmented into GM frames below the JSON-envelope layer.

### Logical key numbers

These values are the current wire ordinals used by `key` in maps and events.
They are distinct from usage-page/usage IDs.

| Decimal | Hex | Logical key |
| ---: | --- | --- |
| 0 | `00` | `KEY_UP` |
| 1 | `01` | `KEY_DOWN` |
| 2 | `02` | `KEY_LEFT` |
| 3 | `03` | `KEY_RIGHT` |
| 4 | `04` | `KEY_SELECT` |
| 5 | `05` | `KEY_BACK` |
| 6 | `06` | `KEY_HOME` |
| 7 | `07` | `KEY_MENU` |
| 8 | `08` | `KEY_NEXT` |
| 9 | `09` | `KEY_PREV` |
| 10 | `0A` | `KEY_PLAY_PAUSE` |
| 11 | `0B` | `KEY_VOL_UP` |
| 12 | `0C` | `KEY_VOL_DOWN` |
| 13 | `0D` | `KEY_MUTE` |
| 14 | `0E` | `BTN_A` |
| 15 | `0F` | `BTN_B` |
| 16 | `10` | `BTN_X` |
| 17 | `11` | `BTN_Y` |
| 18 | `12` | `BTN_L1` |
| 19 | `13` | `BTN_R1` |
| 20 | `14` | `BTN_L2` |
| 21 | `15` | `BTN_R2` |
| 22 | `16` | `BTN_START` |
| 23 | `17` | `BTN_SELECT_GAME` |
| 24 | `18` | `BTN_THUMBL` |
| 25 | `19` | `BTN_THUMBR` |
| 26 | `1A` | `BTN_LEFT` |
| 27 | `1B` | `BTN_RIGHT` |
| 28 | `1C` | `BTN_MIDDLE` |
| 29 | `1D` | `REL_X` |
| 30 | `1E` | `REL_Y` |
| 31 | `1F` | `REL_WHEEL` |
| 32 | `20` | `REL_HWHEEL` |
| 33 | `21` | `ABS_X` |
| 34 | `22` | `ABS_Y` |
| 35 | `23` | `ABS_Z` |
| 36 | `24` | `ABS_RX` |
| 37 | `25` | `ABS_RY` |
| 38 | `26` | `ABS_RZ` |
| 39 | `27` | `ABS_HAT_X` |
| 40 | `28` | `ABS_HAT_Y` |
| 41 | `29` | `ABS_PRESSURE` |
| 42 | `2A` | `TOUCH_DOWN` |
| 43 | `2B` | `TOUCH_UP` |
| 44 | `2C` | `TOUCH_MOVE` |
| 45 | `2D` | `TOUCH_TAP` |
| 46 | `2E` | `TOUCH_DOUBLE_TAP` |
| 47 | `2F` | `TOUCH_LONG_PRESS` |
| 48 | `30` | `TOUCH_SWIPE_UP` |
| 49 | `31` | `TOUCH_SWIPE_DOWN` |
| 50 | `32` | `TOUCH_SWIPE_LEFT` |
| 51 | `33` | `TOUCH_SWIPE_RIGHT` |
| 52 | `34` | `VENDOR_USAGE` |
| 53 | `35` | `KEY_UNKNOWN` |
| 54 | `36` | `KEY_PAGE_UP` |
| 55 | `37` | `KEY_PAGE_DOWN` |
| 56 | `38` | `KEY_SCROLL_UP` |
| 57 | `39` | `KEY_SCROLL_DOWN` |
| 58 | `3A` | `KEY_DOUBLE` |
| 59 | `3B` | `KEY_LONG` |
| 60 | `3C` | `KEY_AI_TRIGGER` |

`65535` (`FFFF`) denotes an unset/disabled mapping slot, not key 0.
