# Phone and glasses protocols

For an independent native client, start with the [Bluetooth/BLE integration guide](BLUETOOTH_DEVELOPER_GUIDE.md)
and [byte-by-byte Hex examples](WIRE_EXAMPLES.md). Public business protocols include
[HUD drawing](HUD_PROTOCOL.md), [capture and native HFP playback](AUDIO_PROTOCOL.md),
and [HOGP control and mappings](BLE_ACCESSORY_PROTOCOL.md). Only executable GMP
delivery/installation transactions are deliberately withheld.

## Supported installation path

Desktop Studio builds the developer-app bundle from the selected packages and
shares it with the official phone App. Import it by scanning the Studio QR code
or entering the displayed IP address and port in the App. The App manages package
validation, permissions, Bluetooth delivery, cache selection and execution on
the glasses. See [INSTALLATION.md](INSTALLATION.md) for the workflow.

The LAN address identifies the computer serving the bundle; it is not a direct
connection to the glasses. After import, the App keeps the installed package and
can launch it without downloading it again from Studio.

Device installation and lifecycle control are owned by the official App. This
public guide deliberately does not specify Bluetooth installer command IDs,
packet layouts, acknowledgements or transfer sequences. Plugin developers use
the public SDK APIs for application messages; these APIs are not an alternative
installer. The binary contracts for application messages and other device
functions are documented below; only package-installation details are withheld.

## Flash cache and execution

The glasses use the 3 MiB CUS8 Flash partition as a managed plugin cache without
a general-purpose filesystem. The directory records package identity, location,
length and state; 64 KiB is reserved for directory storage. The current directory
supports up to 28 entries, including incomplete update candidates. Available byte
space and directory slots both constrain capacity.

- Cache names come from `manifest.name`. Keep distinct plugins' names distinct;
  the name must fit in 63 UTF-8 bytes. `manifest.id` remains relevant to package
  identity and pairing, but it is not the Flash cache lookup key.
- A reusable entry must be complete and match the App's package name, version,
  SHA-256, length and ABI. The App's selected package is authoritative; matching
  a name alone does not establish a cache hit. Required integrity and runtime
  checks still apply before execution.
- A complete match runs from the cache without another package transfer.
  Multiple packages can be cached, but only one plugin is loaded at a time.
  Replacing the current runtime stops and unloads it first.
- Updates require space for the new candidate while retaining the previous
  complete version. Only after successful validation does the candidate replace
  that version. This is per-package temporary space, not two halves of CUS8.
- Under space or directory pressure, eligible external entries are reclaimed
  in least-recently-used order, based on successful use. This is not insertion
  order or a count of downloads. Running images cannot be erased or relocated.
- Protected factory entries are excluded from automatic external eviction.
  Protection is a trusted platform policy, not a permission a third-party
  manifest can grant itself. Factory provisioning is reserved for future
  integration; the SDK does not supply a factory-plugin catalog.
- The platform supports read-only capacity evaluation. It is advisory: capacity
  is checked again when installation begins. If reclamation cannot provide
  enough space, installation fails through the App rather than starting an
  unfinishable transfer.
- Interrupted transfers can resume from a durable checkpoint when the saved
  candidate still matches. A checkpoint can precede the last data received.
  A different package identity or invalid cache state requires a fresh transfer;
  incomplete packages never run.

Cache state can become invalid after a firmware change or Flash error. The App
then supplies its package again. Cache persistence is not a guarantee that an
image survives every firmware update, and plugin cache recovery must not block
the device's normal OTA workflow. Plugins must not use the cache as user-data
storage or access its Flash addresses directly.

## Memory during installation and execution

Delivery uses bounded buffers and can compress independent blocks. Receiving,
decoding and Flash writes overlap where possible; the whole GMP is not first
copied into RAM. Transfer buffers are released before allocating runtime data.
Buffer sizes and scheduling are platform implementation details, not a public
plugin protocol contract.

Code and ordinary read-only constants execute from Flash. Writable data, BSS,
GOT and relocatable pointer tables require RAM; dynamic allocations and shared
Host services also consume memory. A 500 KiB Flash image does not imply a
500 KiB RAM allocation, but the static RAM build check is not a bound on total
runtime or system peak memory. See [ABI.md](ABI.md#runtime-and-memory).

## Application messages

A running glasses plugin receives phone messages through
`GM_PLUGIN_EVENT_BT_MESSAGE` and sends replies through `host->bt_send`.
The phone plugin uses the public PhoneSDK Bluetooth APIs through the App.
The App manages the device connection and permission checks.

The application owns its channel numbers and payload schemas. Document the
payload version, lengths, byte order and response semantics for each channel;
validate lengths and values before reading incoming data. Copy incoming bytes
if they must outlive the event callback. Keep callbacks short and non-blocking.
Applications that retry requests should define request identifiers and duplicate
handling appropriate to their own operations.

Declare compatible application protocols in the manifests so Studio and the
App can match the phone and glasses packages. See
[PROTOCOL_COMPATIBILITY.md](PROTOCOL_COMPATIBILITY.md). An application protocol
version is separate from the GMP format version and the firmware Host ABI.

Examples of public application payloads:

- [Bluetooth text round trip](../examples/bluetooth/README.md).
- [Web bridge scenes and input events](../examples/web_bridge/README.md).

These channel schemas describe plugin functionality, not device installation.
For graphics, input, audio and other capabilities, use the published Host or
PhoneSDK APIs and their permission contracts; see
[CAPABILITY_MATRIX.md](CAPABILITY_MATRIX.md).

## Transports

| Purpose | Bluetooth Classic SPP UUID | iOS iAP2 protocol |
| --- | --- | --- |
| GM application commands and screenshots | `00007033-0000-1000-8000-00805f9b34fb` | `ql.iap2.protocol02` |
| Recording control and Opus stream | `00002024-0000-1000-8000-00805f9b34fb` | `ql.iap2.protocol01` |

The command and recording transports are independent byte streams. In particular, the raw
recording commands described below are not GM packets and must be sent on the
recording transport.

The SPP UUIDs above identify Classic Bluetooth services, not BLE GATT
characteristics. iAP2 is another transport. BLE accessory/HOGP gateway operations
use their own connection and attribute model; a GM frame length is not a BLE MTU.
H5 plugins access application messages through the App Bridge, with
[permission and channel checks](../../PhoneSDK/docs/web-plugin/application-messaging.md).

For phone-facing BLE service/characteristic UUIDs, CCCD setup, MTU handling,
and firmware configuration gaps, see the [BLE discovery contract](BLUETOOTH_DEVELOPER_GUIDE.md#phone-facing-ble-discovery).
SPP support does not establish BLE recording support.

## GM packet framing

A logical GM packet starts with this 9-byte envelope (the final two bytes are
the checksum):

| Offset | Size | Meaning |
| ---: | ---: | --- |
| 0 | 1 | Frame head: `0xFA` for the first frame, then `0x01`, `0x02`, ... for continuation frames |
| 1 | 3 | Total logical packet length, unsigned big-endian |
| 4 | 1 | Event ID, copied into the response |
| 5 | 1 | Service ID |
| 6 | 1 | Command ID |
| 7 | variable | TLVs or continuation payload |
| last 2 | 2 | Unsigned big-endian sum of all preceding bytes in this frame, modulo 65536 |

The standard SPP/firmware transmit frame ceiling is 512 bytes, leaving at most
503 payload bytes. BLE writes must use complete GM frames within the actual
characteristic write limit; the header/checksum consume nine bytes of that limit.
All frames repeat the logical length, event, service, and command. The logical
packet limit used by the reference client is 80 KiB.

TLVs have a one-byte type, a three-byte unsigned big-endian value length, and
the value bytes. Types used here are:

| Type | ID | Encoding |
| --- | ---: | --- |
| BYTES | `0x01` | Opaque bytes |
| STRING | `0x02` | UTF-8 text without a terminating NUL |
| INT32 | `0x03` | Four-byte unsigned big-endian integer |
| JSON | `0x05` | UTF-8 JSON value (object or array as required by the command); a trailing NUL is tolerated on receive |
| STATUS | `0x06` | One-byte status |
| INT16 | `0x07` | Two-byte unsigned big-endian integer |
| INT8 | `0x08` | One-byte unsigned integer |

### Fragmentation and recovery

The 24-bit length is the **logical** length: all TLV bytes plus one 9-byte
envelope. It is not the sum of physical frame lengths. Each physical frame has
its own repeated 7-byte header and 2-byte checksum. With a 512-byte physical
ceiling, non-final frames carry 503 body bytes and are 512 bytes long; the final
frame carries the remainder. BLE downlinks can use smaller complete GM frames
at ATT write boundaries. See the integration guide for the current uplink MTU limitation.
A TLV header or value may straddle frames. Reassemble the logical TLV stream
before interpreting its fields; never interpret a continuation body as a fresh
TLV list.

For example, channel 1 with 600 ASCII `A` bytes has 610 TLV bytes and logical
length 619 (`0x00026B`). With event 1, service `0x0F`, command `0x28`:

```text
frame 1 (512 bytes):
FA 00 02 6B 01 0F 28 07 00 00 02 00 01 01 00 02 58
[41 repeated 493 times] 7F 31

frame 2 (116 bytes):
01 00 02 6B 01 0F 28
[41 repeated 107 times] 1B D1
```

The bracketed repetition is notation, not literal bytes. The wire total is 628
bytes because two envelopes were transmitted, while both headers still advertise
619. Each checksum covers only its own physical frame, excluding its final two
checksum bytes. No whole-message checksum replaces these per-frame sums.

Continuation heads advance from `0x01` upward; the firmware increment helper
wraps `0xF0` to `0x01`. The 80 KiB logical limit is reached before that wrap for
ordinary maximum-size frames. Event/service/command and logical length must
remain consistent throughout the message. The App serializer historically wraps
after `0xEF`, while the firmware macro wraps after `0xF0`. Avoid this boundary
by keeping a business message below 240 physical frames; on small-MTU BLE links,
reduce logical payload size instead of assuming the 80 KiB limit is always usable.

The parser rejects bad checksums, missing/out-of-order continuation heads,
impossible lengths and malformed TLV lengths. A new `0xFA` first frame replaces
an incomplete message; it does not resume that business payload. After a
connection reset, discard partial data and old request correlation state. Do not
interleave two partial messages on one reassembly context. An empty flow-control
response is not the final application-delivery ACK.

On SPP/iAP2 the receive side must account for a byte stream: it may contain part of a
frame or several frames. Buffer/split by the protocol lengths rather than read
callback boundaries. Product builds can add an outer transport envelope; the GM
layout described here is the inner command packet, not a BLE MTU or a promise
that every raw socket begins directly with `0xFA`.

On timeout, check whether the business operation is safe to repeat. A lost reply
does not prove the operation did not execute. The GM frame format alone does not
supply an exactly-once application transaction. On the legacy BLE command
write callback, preserve complete GM physical-frame boundaries; arbitrary
partial-frame writes are not reassembled as a generic stream by that callback.

## Plugin application service (`0x0F`)

Application messaging uses `BT_SERVICE_GM_PLUGIN` on the GM command transport.
It is separate from delivery of the executable plugin package. Multi-byte
integer TLV values use network byte order; the application defines byte order
inside its opaque `BYTES data` payload.

| Direction | Command | ID | TLVs, in order |
| --- | --- | ---: | --- |
| Phone to glasses | Deliver application message | `0x28` | `INT16 channel`, `BYTES data` |
| Glasses to phone | Delivery acknowledgement | `0x28` | `INT8 status`, `INT32 next_offset` |
| Glasses to phone | Application message from `bt_send()` | `0x29` | `INT16 channel`, `BYTES data` |

The delivery acknowledgement reuses the request event ID. Status `0` means
success; nonzero is the positive form of the corresponding `GM_PLUGIN_E*`
result. `next_offset` is part of the shared response layout and is not an
application-message cursor; do not use it to segment business payloads.
A delivery acknowledgement is not an application-level response: use a reply
payload on `0x29` when the phone needs the result of a plugin operation.

Unsolicited plugin messages from glasses use command `0x29`. A phone must route
`0x29` directly to its plugin-event path instead of placing it in the
command-response queue. Its payload contains `INT16 channel`, followed by
`BYTES data`. Glasses never send command `0x29` unless the running plugin
explicitly calls `bt_send()`, so existing plugins that do not use the Host send
API do not add traffic.

The `data` pointer in `GM_PLUGIN_EVENT_BT_MESSAGE` is borrowed and remains valid
only for the duration of `on_event()`. A plugin must copy data that it needs
later. A successful phone-to-glasses acknowledgement means the event was
delivered to the running plugin; it does not depend on the callback's local
handled return value. Runtime messages use the existing GM continuation-frame mechanism through the shared command transport. The current 80 KB GM logical-package limit
allows 81,901 data bytes after the packet, channel TLV and BYTES TLV overhead;
this follows directly from the shared GM package format and is not a separate
plugin capability.
An empty packet returned while a logical GM packet is incomplete is transport
flow control, not the final command acknowledgement; clients must keep waiting
for the response containing `INT8 status`.

### Plugin status values

Plugin acknowledgements use INT8 status; system commands below use a separate
STATUS TLV namespace. Do not apply one service's error table to another.

| Status | Plugin result | Meaning |
| ---: | --- | --- |
| 0 | `GM_PLUGIN_OK` | Success |
| 1 | `GM_PLUGIN_EINVAL` | Invalid pointer, size, value or layout |
| 2 | `GM_PLUGIN_ENOTSUP` | Unsupported operation |
| 3 | `GM_PLUGIN_EBUSY` | Resource temporarily unavailable |
| 4 | `GM_PLUGIN_ENOMEM` | Insufficient RAM or output capacity |
| 5 | `GM_PLUGIN_EIO` | Device or transport operation failed |
| 6 | `GM_PLUGIN_EPERM` | Operation not permitted |
| 7 | `GM_PLUGIN_ESTATE` | Invalid runtime state |
| 8 | `GM_PLUGIN_EVERSION` | Incompatible ABI or table version |
| 9 | `GM_PLUGIN_EFULL` | Insufficient plugin Flash capacity |

An early dispatch failure, before the display/plugin handler runs, can instead
return a generic STATUS TLV (for example queue pressure or allocation failure).
Inspect the TLV type before choosing an error namespace.

This is the shared result namespace; not every command returns every value.
The Host C API returns the negative error value; the wire carries its positive
form. A plugin-defined reply payload has its own application status convention.

### Binary application-message example

The following complete single frame sends ASCII `ABC` to application channel 1
with event ID 1. Whitespace separates bytes; it is not transmitted.

```text
FA 00 00 16 01 0F 28  07 00 00 02 00 01  01 00 00 03 41 42 43  02 1C
```

- `00 00 16`: 22-byte logical packet, including envelope and checksum.
- `0F 28`: plugin service and phone-to-glasses application command.
- `07 00 00 02 00 01`: INT16 channel 1.
- `01 00 00 03 41 42 43`: BYTES payload `ABC`.
- `02 1C`: sum of the preceding frame bytes, modulo 65536, big-endian.

For a glasses-originated application message, the command is `0x29`; the channel
and data TLV layout is the same. Its event ID belongs to that outgoing message,
not necessarily the earlier request. Applications needing request/reply matching
should put their own request identifier in the business payload.

## System service (`0x01`)

System-command acknowledgements contain `STATUS status` and may append a
`STRING message`. Unlike plugin-management status values, system status values
use this generic namespace:

| Value | Meaning |
| ---: | --- |
| `0` | Success |
| `1` | Generic error |
| `2` | Invalid command |
| `3` | Invalid parameter |
| `4` | Out of memory |
| `5` | Timeout |
| `6` | Not supported |
| `7` | Busy |
| `8` | Error; retry immediately |

### Recording audio processing

Audio processing is selected on the GM command transport before opening the
recording transport. The setting applies to the next recording.

| Direction | Command | ID | TLVs, in order |
| --- | --- | ---: | --- |
| Phone to glasses | Set recording processing | `0x2F` | `INT8 noise_mode`, optional `INT8 pickup_mode` |
| Glasses to phone | Acknowledgement | `0x2F` | `STATUS status`, optional `STRING message` |

`noise_mode` values:

| Value | Meaning |
| ---: | --- |
| `0` | Enable recording noise reduction (ENC) |
| `1` | Disable recording noise reduction; use the raw path |

`pickup_mode` values:

| Value | Meaning |
| ---: | --- |
| omitted | Keep the glasses' current setting; use this for older firmware |
| `0` | Fixed forward pickup (legacy default) |
| `1` | Adaptive three-microphone meeting pickup |
| `2` | Focus on non-wearer speech and suppress wearer speech |
| `3` | Balanced forward pickup retaining wearer and forward speech |
| `4` | Focus on the forward target and suppress side speech |

The TLV order is part of the command contract: noise mode comes first and the
optional pickup mode second. Unsupported values must not be sent.

### Screenshot capture

Screenshots are transferred asynchronously on the GM command transport. The
glasses return a normal system acknowledgement for `0x65` and `0x6A`, but the
reference clients do not wait for it: they route `0x66` upload packets
independently and treat the start/error markers as the capture result. A client
that does wait for the acknowledgement must still begin accepting `0x66`
packets immediately because upload can start before the acknowledgement is
consumed.

| Direction | Command | ID | TLVs |
| --- | --- | ---: | --- |
| Phone to glasses | Request screenshot | `0x65` | none |
| Glasses to phone | Screenshot upload | `0x66` | `INT16 index`, `JSON metadata`, `BYTES data` |
| Phone to glasses | Request missing chunks | `0x6A` | `JSON {"missing":[index,...]}` |

The `0x65` and `0x6A` acknowledgements use `STATUS status` plus an optional
`STRING message`. Status `7` means that a capture or resend is already in
progress. For `0x6A`, status `1` can also mean that the retained resend buffer
has expired. Current firmware retains that buffer for about 15 seconds and
processes at most 64 missing indices per request, so a receiver should split a
larger missing set across rounds.

The upload `index` has three reserved values:

| Index | Meaning |
| ---: | --- |
| `0xFFFE` | Start marker. It resets the receiver and declares the transfer metadata. |
| `0xFFFD` | Complete marker. The receiver verifies completeness and requests missing chunks if necessary. |
| `0xFFFF` | Error marker. `metadata.message`, when present, describes the error. |
| `0..total_chunks-1` | Screenshot data chunk. |

Start metadata fields used by the reference client are:

| Field | Meaning |
| --- | --- |
| `total_chunks` | Number of data chunks |
| `width`, `height` | Stored framebuffer dimensions in pixels |
| `visible_width` | Width to retain in the exported image; the client falls back to `width` when absent or invalid |
| `original_size` | Decompressed byte count; must equal `ceil(width * height / 2)` |
| `data_size` / `total_size` | Total compressed byte count. Current start markers use `data_size`; clients accept `total_size` as an alias, and complete markers use `total_size`. |
| `compression` | Compression name; currently `lz4` |
| `format` | Optional layout; `banded` selects the banded layout, otherwise one raw LZ4 block is assumed |
| `band_count` | Number of bands when `format` is `banded` |
| `band_rows` | Rows per band, used to derive the band count when `band_count` is absent |
| `bpp` | Bits per pixel; current screenshots use `4` |
| `cmd_id` | Informational producer command ID; current uploads use decimal `102` (`0x66`) |
| `status` | Marker state such as `start` or `complete` |

Data-chunk metadata may additionally contain `chunk_index`, `chunk_size`, and
`offset`. Retransmitted chunks set `is_resend` to `true`; the following complete
marker may also include `resent_count`. These fields are diagnostic—the leading
`INT16 index` and the start marker's transfer metadata remain authoritative.

Current firmware emits 384-byte data chunks, except for the shorter final
chunk. The reference clients deliberately accept any non-empty chunk up to
1024 bytes for compatibility with older/newer senders. Chunks may be duplicated
or arrive out of order; `index` determines their position. The receiver joins
exactly `total_chunks` chunks and requires the result to equal the declared
compressed byte count.
If chunks are missing at the complete marker or after an idle interval, it
sends `0x6A` with their numeric indices. The reference client allows eight
retries and uses an eight-second idle timeout.

The uncompressed framebuffer is 4-bit grayscale, two pixels per byte, with the
left/even pixel in the high nibble and the right/odd pixel in the low nibble.
A non-banded screenshot is one raw LZ4 block and must decode to
`original_size` bytes. A banded stream is a concatenation of:

| Offset | Size | Meaning |
| ---: | ---: | --- |
| 0 | 1 | Zero-based, sequential band index |
| 1 | 2 | Row count, unsigned big-endian |
| 3 | 2 | Compressed band size, unsigned big-endian |
| 5 | variable | Raw LZ4 block for `rows * (width / 2)` bytes |

The banded layout requires an even `width`, because each independently decoded
row occupies exactly `width / 2` bytes. All bands together must cover exactly
`height` rows and `original_size` bytes.

## Recording transport

### Control sequence

The phone sends these raw three-byte messages on the recording transport:

| Operation | Bytes | Response |
| --- | --- | --- |
| Select legacy stream format | `52 12 00` | `52 13 00 31 28 00` when negotiation is supported |
| Start default recording scene | `52 01 00` | `52 11 noise_mode timestamp_be24`; an active-stream packet may also confirm start |
| Stop recording | `52 00 00` | No acknowledgement |

For `52 12 mode`, mode `0` selects legacy 49-byte frame records and mode `1`
selects compact 40-byte Opus records. The six-byte response is `52 13 mode 31
28 00`: `0x31` (49) and `0x28` (40) advertise the two record sizes. Unsupported
mode values fall back to legacy mode. GM Plugin Studio explicitly requests mode
`0`.

For `52 01 scene`, the third byte is a recording-scene selector. The reference
client sends `0`, which is not a public scene ID and therefore selects the
firmware's default normal-recording scene. The start notification contains the
effective `noise_mode` followed by the low 24 bits of the recording timestamp
in big-endian order.

Send the format command first and wait for `52 13` before sending start.
For compatibility with old firmware that does not acknowledge negotiation, the
reference client waits up to 800 ms and then sends start. It waits up to three
seconds for recording start. Because stop has no acknowledgement, continue
reading briefly after sending it so queued tail audio is not lost, then close
the recording transport.

### Legacy Opus stream

Recording data packets use this outer header:

| Offset | Size | Meaning |
| ---: | ---: | --- |
| 0 | 1 | Magic `0x52` |
| 1 | 1 | Stream command (`0x90` through `0x94`) |
| 2 | 2 | Reserved; must be zero |
| 4 | 2 | Reserved/firmware data |
| 6 | 1 | Number of frames, `1..8` |
| 7 | 4 | Reserved/firmware data |
| 11 | 1 | Header terminator `0xFF` |
| 12 | variable | `frame_count` consecutive 49-byte frame records |

The active recording started by `52 01 00` arrives with stream command `0x92`.
Commands `0x90`, `0x91`, `0x93`, and `0x94` belong to other audio consumers and
must not be mixed into this recording.

Within each legacy 49-byte frame record, byte 4 is the encoded payload length and is
`0x28` (40) for the legacy format. The 40-byte Opus packet starts at byte 9 and
occupies bytes 9 through 48. Thus the total outer packet length is
`12 + frame_count * 49` bytes. The transport is a byte stream: implementations
must buffer fragmented packets, handle multiple packets in one read, and
resynchronize on `0x52` after malformed or unrelated data.

In negotiated compact mode, the same 12-byte outer header is followed directly
by `frame_count` consecutive 40-byte Opus packets, for a total length of
`12 + frame_count * 40`. There is no per-frame 9-byte legacy prefix. The current
GM Plugin Studio parser supports only legacy mode and must not request compact
mode without changing its record parser.

The Opus packets may be consumed directly or muxed into an Ogg Opus container;
no decode/re-encode step is required. The reference app advertises a 16 kHz
input rate in `OpusHead` and derives Ogg granule positions at the mandatory
48 kHz Opus clock from each packet's TOC byte.


## Trust and diagnostics

Use a trusted LAN and trusted packages. CRC and SHA-256 detect inconsistent
content; they do not authenticate a publisher. Documenting only the supported
App workflow does not create transport authentication or a native-code sandbox.
See [SECURITY.md](SECURITY.md).

When reporting a slow or failed launch, retain App and glasses logs from the
same attempt. Distinguish Studio-to-phone download, package preparation,
phone-to-glasses transfer, Flash validation and runtime startup. A warm cache hit
should omit package data transfer; it still needs runtime startup and checks.
Do not implement retries by sending private device commands from plugin code.

## Documentation coverage

See [the coverage audit](PROTOCOL_COVERAGE.md) for confirmed documentation gaps
and the distinction between public protocol visibility and installation authorization.

## Related device contracts

- [BLE accessory gateway](BLE_ACCESSORY_PROTOCOL.md): control, GATT operations,
  data, HID events and response/error handling.
- [Device business commands](DEVICE_BUSINESS_PROTOCOL.md): system/display,
  translation, teleprompter, notifications and navigation boundaries.
