# Phone and glasses protocols

This document is the public wire contract for the GM command channel used for
plugin management and system commands, plus the separate recording byte stream
used by official Studio and App builds.

## Transports

| Purpose | Bluetooth Classic SPP UUID | iOS iAP2 protocol |
| --- | --- | --- |
| GM commands, plugin transfer, and screenshots | `00007033-0000-1000-8000-00805f9b34fb` | `ql.iap2.protocol02` |
| Recording control and Opus stream | `00002024-0000-1000-8000-00805f9b34fb` | `ql.iap2.protocol01` |

The two transports are independent byte streams. In particular, the raw
recording commands described below are not GM packets and must be sent on the
recording transport.

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

Each physical frame is at most 512 bytes, leaving at most 503 payload bytes.
All frames repeat the logical length, event, service, and command. The logical
packet limit used by the reference client is 80 KiB.

TLVs have a one-byte type, a three-byte unsigned big-endian value length, and
the value bytes. Types used here are:

| Type | ID | Encoding |
| --- | ---: | --- |
| BYTES | `0x01` | Opaque bytes |
| STRING | `0x02` | UTF-8 text without a terminating NUL |
| INT32 | `0x03` | Four-byte unsigned big-endian integer |
| JSON | `0x05` | UTF-8 JSON object; a trailing NUL is tolerated on receive |
| STATUS | `0x06` | One-byte status |
| INT16 | `0x07` | Two-byte unsigned big-endian integer |
| INT8 | `0x08` | One-byte unsigned integer |

## Plugin service (`0x0F`)

Plugin management uses the existing GM transport and service
`BT_SERVICE_GM_PLUGIN`. This product has one controller driving both optical
engines, so every command is sent once over the single phone SPP link.

Multi-byte integer TLVs use network byte order. Install chunks must be
sequential. The phone must wait for the command acknowledgement before sending
the next chunk.

| Direction | Command | ID | TLVs |
| --- | --- | ---: | --- |
| Phone to glasses | Get capabilities | `0x20` | none |
| Phone to glasses | Install begin | `0x21` | `BYTES identity` (108 bytes) |
| Phone to glasses | Install chunk | `0x22` | `INT32 offset`, `BYTES canonical_data` |
| Phone to glasses | Install commit | `0x23` | none |
| Phone to glasses | Install abort | `0x24` | none |
| Phone to glasses | Start | `0x25` | none |
| Phone to glasses | Stop | `0x26` | none |
| Phone to glasses | Remove | `0x27` | none |
| Phone to glasses | Plugin message | `0x28` | `INT16 channel`, `BYTES data` |
| Glasses to phone | Plugin message | `0x29` | `INT16 channel`, `BYTES data` |
| Phone to glasses | Query cache capacity | `0x2a` | `BYTES identity` (108 bytes, same as BEGIN) |
| Phone to glasses | Compressed block begin | `0x2b` | `BYTES metadata` (16 bytes) |
| Phone to glasses | Compressed block data | `0x2c` | `INT32 encoded_offset`, `BYTES compressed_data` |
| Phone to glasses | Open bounded pipeline | `0x2d` | `BYTES capacity` (4-byte little-endian raw block capacity) |
| Phone to glasses | Pipeline frame | `0x2e` | `BYTES metadata` (20 bytes), `BYTES payload` (1..8192 bytes) |

`BYTES data` in a plugin message must contain at least one byte. Use an
application-level opcode when a command has no additional payload; empty BYTES
values are not representable by the current GM TLV allocator.

Platform information is returned as schema-1 JSON with `packageFormat`, packed
16-bit `abi`, `compression` (`lz4-block`), `compressionBlockBytes`, `maxPackageBytes`, `chunkBytes`,
transport flags, display geometry/format and the frozen Host `capabilities`.
`chunkBytes` is only the sequential SPP installer chunk size.
GMP v2 has a 120-byte little-endian header, a metadata prefix padded to 64 B,
Flash text/constants, then RAM initial bytes. The header carries Name, business
version, exact ABI, independent segment sizes, entry, relocation counts and
whole-package CRC32 (computed with its field at byte 48 zeroed). BSS is omitted.
See `gm_plugin_xip.h` for exact fields and bounds.

BEGIN identity is Name[64] (UTF-8, NUL and zero padding), SHA-256[32] of the
canonical GMP, then business version, package length and ABI as three little-
endian uint32 values. Name is the cache key. The phone's package is authoritative,
including downgrades. All identity fields must match to reuse or resume.
BEGIN first stops/unloads the current plugin and preflights capacity. An exact
complete hit returns the package length; an interrupted candidate returns its
durable sector boundary; a new candidate returns zero. No bytes are sent on a
complete hit: that BEGIN has already verified, loaded and started the cached plugin.
Do not send another COMMIT or START on this path. CHUNK carries up to 8192 raw bytes, one ACK per logical block. GM framing splits
it into physical frames; Flash internally processes 256-byte slices. BEGIN/COMMIT/START can take longer
than chunk writes; the APP gives each up to 120 seconds.

### Bounded receive / decode / Flash pipeline

The APP uses `0x2d`/`0x2e` for cache misses or resumes larger than one 8 KiB
frame. A complete cache hit still launches through the single installation BEGIN.
No network capability round trip is added. APP and firmware must be updated together.

STREAM_OPEN (`0x2d`) reserves one arena for the whole transfer. Its capacity is
normally **64 KiB before compression**, configurable with
`GmPluginInstaller.compressionBlockBytes` and capped by firmware
`GM_PLUGIN_LZ4_BLOCK_BYTES` (4 KiB multiples, up to 256 KiB). The successful ACK
returns the durable raw starting offset. Before any write, ENOMEM/ENOTSUP causes
the APP to halve its request; at 8 KiB or below it uses ordinary raw CHUNKs.
Zero disables compression/pipeline. Tiny packages use raw CHUNKs directly.
For a smaller remaining package, the APP rounds the requested capacity up to
4 KiB instead of reserving the full configured block size.

STREAM_DATA (`0x2e`) has five little-endian uint32 metadata fields:
`rawOffset`, `decodedLength`, `encodedLength`, `rawCRC32`, `encodedOffset`.
All metadata except encodedOffset must stay identical throughout the block.
Encoded bytes are independent raw LZ4 when encodedLength < decodedLength;
equality means an uncompressed block. Empty blocks and larger encoded lengths
are invalid. The APP sends compression only if it saves more than 128 bytes;
there is no separate per-block BEGIN/ACK. Frames still carry at most 8192 bytes.

A successful frame ACK returns the cumulative **encoded receive offset** and
one-frame credit. It does **not** guarantee Flash persistence, even for the last
frame. The receiver frees the existing BT payload before acknowledging it;
the parser fills the final TLV allocation directly, and the worker owns/frees
the complete node rather than its interior payload pointer. There is no parser
completion copy or additional 8 KiB worker copy. With one unacknowledged frame, the
phone may receive a delayed ACK when the next arena prefix is not free yet.

After receiving a block, the worker decodes and verifies its CRC while one next
frame may queue. It writes verified raw data in at-most-8 KiB slices through the
existing XIP binding / sector checkpoint path. Each successfully written prefix
can receive next-block encoded data. The next block is moved within the same
arena and decoded only after the previous raw block has been fully written.
No second 64 KiB buffer or extra Flash task is allocated. Scheduling may limit
actual overlap; correctness does not depend on hardware concurrency.

COMMIT is the **durable fence**: it waits behind decoding/writes and checks the
complete package before acknowledging success. START is forbidden before that
success. CRC/write failures remain latched and fail pending/future DATA/COMMIT;
a receive ACK cannot turn a failed write into a successful install. STOP and
link loss request cancellation; worker access ends before arena/session teardown.
Reconnection uses installation BEGIN's durable **raw** offset and recompresses
from that offset. No encoded progress is persisted.

Arena size is `capacity + (capacity >> 8) + 32`: 65,824 B for 64 KiB.
One existing BT payload adds at most 8192 B (74,016 B combined), plus small
metadata/jobs. OPEN temporarily probes 8704 B of receive headroom; this is not a
reservation against other tasks. Directory/session memory, transport internals
and the existing 8 KiB Flash task stack are additional. The arena is released
before runtime loading. `streamedInstallChunks=true` describes this pipeline.

One firmware `[PluginTransfer] pipeline=1` summary reports arena size,
`prefetch` bytes received into written prefixes, `write_ms`, `process_ms`
(decode/CRC/prefetch work), `wait_ms`, `total_ms`, and result. Phone
`transfer.summary` reports streaming, frame count and wire payload bytes.
These timers are measured on device; host tests do not establish a speedup.

### Independent compressed installation blocks

The serial `0x2b`/`0x2c` path remains available for direct protocol diagnostics.
The current APP uses the bounded pipeline above. Serial blocks are normally
**64 KiB before compression**.
`GmPluginInstaller.compressionBlockBytes` configures the phone's preferred size
(default 65536; zero disables compression). Firmware `GM_PLUGIN_LZ4_BLOCK_BYTES`
in `gm_plugin_transfer.h` configures its maximum (default 65536). Both accept
4 KiB multiples up to 256 KiB; a final block may be shorter.

`0x2b` metadata contains four **little-endian uint32** values: canonical raw
package offset, decoded length, encoded length, CRC32 of the raw block. Encoded
length must be positive and strictly smaller than decoded length. The success
ACK returns the raw starting offset. No Flash bytes are written by block begin.

`0x2c` sends at most 8192 encoded bytes. Its INT32 offset is in network byte
order and starts at zero for each block. Its ACK returns cumulative **encoded**
bytes received. Only the final ACK also guarantees exact-length decompression,
raw-block CRC32 verification and successful writing through the normal XIP
binding/Flash path. Compressed bytes are not stored as the executable image.

The receiver stores compressed input at the end of one in-place decompression
arena: `decodedLength + (decodedLength >> 8) + 32` bytes. A 64 KiB block uses
65,824 bytes, not two 64 KiB buffers. Before accepting it, the receiver probes
16,896 bytes of temporary headroom for the next BT TLV and worker copy, then
releases that probe. This is an allocation-time check, not a reservation against
unrelated tasks allocating later. Other transport, directory and task-stack
allocations remain additional; the arena size is not total system RAM peak.

A block-begin ENOMEM (4) or configured-size ENOTSUP (2) leaves the installation
session unchanged. The phone halves its preferred block size, retaining that
smaller size for the rest of this transfer; when compression has no useful
saving it sends ordinary raw `0x22` fragments. Compression is used only when it
saves at least 128 payload bytes and does not increase command/ACK count.
DATA, integrity and Flash failures abort rather than blindly replaying the block.

Abort/disconnect discards a partial compressed block. The next installation
BEGIN returns the durable **raw** offset; the phone compresses a fresh independent
block starting there. No compressed offset survives reconnection. The arena is
freed before the final DATA ACK and before runtime loading. Cache hits perform
neither compression nor transfer. Whole-package verification at COMMIT is retained.

Except for the capacity query described below, management acknowledgements reuse the request command ID and contain `INT8
status`, followed by `INT32 next_offset`. Status `0` is success; nonzero is the
positive form of the corresponding `GM_PLUGIN_E*` value. For a successful raw chunk,
`next_offset` is the first raw byte the phone should send next; compressed DATA
uses the encoded offset described above. Command `0x28`
therefore has only two valid roles: a phone-to-glasses plugin message and its
glasses-to-phone transport acknowledgement.

### Read-only capacity query

QUERY is optional and advisory. It never starts/stops a plugin, verifies a whole
package, writes metadata, evicts entries, moves data or changes LRU. Do not add
it before each launch: BEGIN already checks capacity immediately before an
installation. A query describes the space available after stopping the current
plugin and applying the normal eviction/compaction rules. Another operation can
change the result; BEGIN remains authoritative.

A QUERY response contains `INT8 status`. Status 0 (fits) or 9 (Flash full) also
contains one 24-byte BYTES value with six little-endian uint32 values:

| Offset | Value |
| ---: | --- |
| 0 | Healthy payload capacity, excluding the 64 KiB metadata area and quarantined sectors |
| 4 | Currently free payload bytes, before proposed reclamation |
| 8 | Additional reservation required, rounded to 4 KiB; zero for a healthy metadata match/reservation |
| 12 | Bytes the proposed victim set would reclaim |
| 16 | Complete plugins in that victim set (abandoned incomplete data is not counted) |
| 20 | Flags: bit 0 complete identity match, bit 1 receiving identity match, bit 2 compaction needed |

An identity-match flag describes metadata only; execution still requires BEGIN's
content verification. On FULL the proposed victims are hypothetical: nothing is
removed. Other errors have no valid metrics. An active transfer/worker or a
partition needing recovery can return BUSY (3); I/O failures return 5. An
uninitialized blank partition is inspected as empty without formatting it.

Status 9 is `GM_PLUGIN_EFULL`: insufficient plugin Flash space. Status 4 remains
`GM_PLUGIN_ENOMEM`: insufficient RAM or an output buffer. The phone distinguishes
these errors and reports a dedicated Flash-space error for failed installation.

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
handled return value. Runtime messages use the existing GM continuation-frame mechanism rather
than the 8192-byte installer chunks. The current 80 KB GM logical-package limit
allows 81,901 data bytes after the packet, channel TLV and BYTES TLV overhead;
this follows directly from the shared GM package format and is not a separate
plugin capability.
An empty packet returned while a logical GM packet is incomplete is transport
flow control, not the final command acknowledgement; clients must keep waiting
for the response containing `INT8 status`.

The Flash worker writes small slices directly to CUS8. COMMIT verifies the full
package CRC, SHA-256 and installed instruction bindings before publishing the
new version. Failed candidates never run. Runtime loads Flash code/constants
in place, allocating only writable data/BSS and Host state. LRU updates run in the background after a successful runtime start and its ACK;
consecutive launches of the already most-recent plugin need no LRU write. Running images cannot be moved or erased.

ABORT releases transfer RAM but preserves the last durable checkpoint. A retry
must use BEGIN to discover that checkpoint; the last CHUNK ACK can include a
volatile tail. Definitively malformed prefix/instruction data discards only the
candidate, allowing restart from zero. STOP after a failed/timed-out START is
serialized after an outstanding Flash operation, with its ACK after cleanup.
Remove unloads and removes the current external cache entry. Protected factory
entries cannot be removed or replaced by the laboratory transport.

The wire entry is always external. It never accepts a caller-supplied flag to
grant factory-management rights. Factory restoration and firmware upgrade
orchestration require a separately authorized internal product entry.

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
