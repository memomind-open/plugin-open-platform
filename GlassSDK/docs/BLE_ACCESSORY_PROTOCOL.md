# BLE accessory gateway protocol

This is the App-to-glasses **GM service `0x10`** for the glasses' BLE Central /
HID Host gateway. The accessory link is BLE; the App control link uses the
[GM transport and TLVs](PROTOCOL.md#gm-packet-framing). Do not connect to a
Classic SPP UUID as if it were a GATT characteristic. This service is separate
from plugin package installation and is not exposed as an arbitrary H5 Bridge
method by `device.messaging`.

The current implementation manages one accessory. Query capabilities before
using optional functionality. Do not treat a scan address, a permanent bond
identity, a GM event ID and a GATT transaction `seq` as interchangeable IDs.

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
when available. Use the readiness flags: connected does not imply HID ready,
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
Use the same scan's address/type for connecting. `0x09` is separate and contains
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
Full mapping-rule semantics are a separate accessory profile contract; do not
send guessed rules based only on the command number or a raw HID descriptor.
